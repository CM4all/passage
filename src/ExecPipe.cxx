// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "ExecPipe.hxx"
#include "Action.hxx" // for StderrOption
#include "lib/fmt/SystemError.hxx"
#include "io/Pipe.hxx"
#include "util/ScopeExit.hxx"

#include <signal.h>
#include <spawn.h>

ExecPipeResult
ExecPipe(const char *path, const char *const*args,
	 const char *const*env,
	 FileDescriptor cgroup,
	 StderrOption stderr_option)
{
	auto [r, w] = CreatePipe();

	UniqueFileDescriptor stderr_r, stderr_w;
	switch (stderr_option) {
	case StderrOption::JOURNAL:
		break;

	case StderrOption::PIPE:
		std::tie(stderr_r, stderr_w) = CreatePipe();
		break;
	}

	posix_spawnattr_t attr;
	posix_spawnattr_init(&attr);
	AtScopeExit(&attr) { posix_spawnattr_destroy(&attr); };

	sigset_t signals;
	sigemptyset(&signals);
	posix_spawnattr_setsigmask(&attr, &signals);
	sigaddset(&signals, SIGPIPE);
	posix_spawnattr_setsigdefault(&attr, &signals);

	short flags = POSIX_SPAWN_SETSIGDEF|POSIX_SPAWN_SETSIGMASK;

	if (cgroup.IsDefined()) {
		flags |= POSIX_SPAWN_SETCGROUP;
		posix_spawnattr_setcgroup_np(&attr, cgroup.Get());
	}

	posix_spawnattr_setflags(&attr, flags);

	posix_spawn_file_actions_t file_actions;
	posix_spawn_file_actions_init(&file_actions);
	AtScopeExit(&file_actions) { posix_spawn_file_actions_destroy(&file_actions); };

        posix_spawn_file_actions_adddup2(&file_actions, w.Get(), STDOUT_FILENO);

	if (stderr_w.IsDefined())
		posix_spawn_file_actions_adddup2(&file_actions, stderr_w.Get(), STDERR_FILENO);

	pid_t pid;
	if (int error = posix_spawn(&pid, path, &file_actions, &attr,
				    const_cast<char *const *>(args),
				    const_cast<char *const *>(env));
	    error != 0)
		throw FmtErrno(error, "Failed to execute {:?}", path);

	return {
		.stdout_pipe = std::move(r),
		.stderr_pipe = std::move(stderr_r),
	};
}
