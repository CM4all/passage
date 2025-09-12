// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "ExecPipe.hxx"
#include "Action.hxx" // for StderrOption
#include "system/Error.hxx"
#include "system/SetupProcess.hxx"
#include "io/Pipe.hxx"

#include <fmt/format.h>

static void
ReadDummy(FileDescriptor fd) noexcept
{
	std::byte dummy[1];
	(void)fd.Read(dummy);
}

ExecPipeResult
ExecPipe(const char *path, const char *const*args,
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

	auto [exec_wait_r, exec_wait_w] = CreatePipe();

	const auto pid = fork();
	if (pid < 0)
		throw MakeErrno("fork() failed");

	if (pid == 0) {
		PostFork();

		w.CheckDuplicate(FileDescriptor(STDOUT_FILENO));

		if (stderr_w.IsDefined())
			stderr_w.CheckDuplicate(FileDescriptor(STDERR_FILENO));

		execv(path, const_cast<char*const*>(args));
		fmt::print(stderr, "Failed to execute '{}': {}\n",
			   path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* wait for the execv() to complete, so all inherited file
	   descriptors are closed (to avoid race condition inside
	   EventLoop) */
	exec_wait_w.Close();
	ReadDummy(exec_wait_r);

	return {
		.stdout_pipe = std::move(r),
		.stderr_pipe = std::move(stderr_r),
	};
}
