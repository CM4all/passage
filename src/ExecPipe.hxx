// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "io/UniqueFileDescriptor.hxx"

#include <cstdint>

enum class StderrOption : uint_least8_t;

struct ExecPipeResult {
	/**
	 * The read end of the pipe that is connected to the child's
	 * #STDOUT_FILENO.
	 */
	UniqueFileDescriptor stdout_pipe;

	UniqueFileDescriptor stderr_pipe;
};

/**
 * Launch a process with a pipe connected to STDOUT.
 *
 * @param args a nullptr-terminated list of command-line arguments
 */
ExecPipeResult
ExecPipe(const char *path, const char *const*args,
	 FileDescriptor cgroup,
	 StderrOption stderr_option);
