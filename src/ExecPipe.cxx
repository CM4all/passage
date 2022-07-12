/*
 * Copyright 2017-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ExecPipe.hxx"
#include "system/Error.hxx"
#include "system/SetupProcess.hxx"
#include "io/UniqueFileDescriptor.hxx"

static void
ReadDummy(FileDescriptor fd) noexcept
{
	char dummy;
	fd.Read(&dummy, sizeof(dummy));
}

UniqueFileDescriptor
ExecPipe(const char *path, const char *const*args)
{
	UniqueFileDescriptor r, w;
	if (!UniqueFileDescriptor::CreatePipe(r, w))
		throw MakeErrno("pipe() failed");

	UniqueFileDescriptor exec_wait_r, exec_wait_w;
	if (!UniqueFileDescriptor::CreatePipe(exec_wait_r, exec_wait_w))
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

	/* wait for the execv() to complete, so all inherited file
	   descriptors are closed (to avoid race condition inside
	   EventLoop) */
	exec_wait_w.Close();
	ReadDummy(exec_wait_r);

	return r;
}
