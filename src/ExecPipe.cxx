/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "ExecPipe.hxx"
#include "spawn/Registry.hxx"
#include "system/Error.hxx"
#include "system/SetupProcess.hxx"
#include "io/UniqueFileDescriptor.hxx"

UniqueFileDescriptor
ExecPipe(ChildProcessRegistry &registry,
	 const char *path, const char *const*args)
{
	UniqueFileDescriptor r, w;
	if (!UniqueFileDescriptor::CreatePipe(r, w))
		throw MakeErrno("pipe() failed");

	const auto pid = fork();
	if (pid < 0)
		throw MakeErrno("fork() failed");

	if (pid == 0) {
		PostFork();

		w.CheckDuplicate(FileDescriptor(STDOUT_FILENO));
		execv(path, const_cast<char*const*>(args));
		fprintf(stderr, "Failed to execute '%s': %s\n",
			path, strerror(errno));
		exit(EXIT_FAILURE);
	}

	registry.Add(pid, path, nullptr);

	return r;
}
