/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

class UniqueFileDescriptor;

/**
 * Launch a process with a pipe connected to STDOUT.
 *
 * @param args a nullptr-terminated list of command-line arguments
 * @return the pipe's read end
 */
UniqueFileDescriptor
ExecPipe(const char *path, const char *const*args);
