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

#include "Protocol.hxx"
#include "Entity.hxx"
#include "Parser.hxx"
#include "net/ReceiveMessage.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"
#include "util/StringCompare.hxx"
#include "util/StringView.hxx"

#include <stdlib.h>
#include <fcntl.h>

struct Usage {};

static UniqueSocketDescriptor
CreateConnect(SocketAddress address)
{
	const int family = address.GetFamily();
	constexpr int socktype = SOCK_SEQPACKET;
	constexpr int protocol = 0;

	UniqueSocketDescriptor fd;
	if (!fd.Create(family, socktype, protocol))
		throw MakeErrno("Failed to create socket");

	if (!fd.Connect(address))
		throw MakeErrno("Failed to connect");

	return fd;
}

static UniqueSocketDescriptor
CreateConnect(const char *path)
{
	AllocatedSocketAddress address;
	address.SetLocal(path);
	return CreateConnect(address);
}

static void
SendOrThrow(SocketDescriptor fd, ConstBuffer<void> payload)
{
	ssize_t nbytes = send(fd.Get(), payload.data, payload.size, 0);
	if (nbytes < 0)
		throw MakeErrno("Failed to send");
	if (size_t(nbytes) < payload.size)
		throw MakeErrno("Short send");
}

static void
SendRequest(SocketDescriptor fd, const Entity &request)
{
	const auto payload = request.Serialize();
	SendOrThrow(fd, StringView(payload.data(), payload.size()).ToVoid());
}

static UniqueFileDescriptor
ReceiveResponse(SocketDescriptor s)
{
	ReceiveMessageBuffer<4096, 1024> buffer;
	auto result = ReceiveMessage(s, buffer, 0);
	if (result.payload.empty())
		throw std::runtime_error("Server closed the connection prematurely");

	StringView payload((const char *)result.payload.data,
			   result.payload.size);

	const auto response = ParseEntity(payload);

	if (response.command == "OK") {
		return result.fds.empty()
			? UniqueFileDescriptor()
			: std::move(result.fds.front());
	} else if (response.command == "ERROR") {
		if (response.args.empty())
			throw std::runtime_error("Server error");
		else
			throw std::runtime_error("Server error: " +
						 response.args.front());
	} else
		throw std::runtime_error("Malformed response");
}

static void
Copy(FileDescriptor in, FileDescriptor out)
{
	while (true) {
		ssize_t nbytes = splice(in.Get(), nullptr, out.Get(), nullptr,
					1024 * 1024 * 1024, SPLICE_F_MOVE);
		if (nbytes == 0)
			return;

		if (nbytes < 0)
			/* whatever happened - fall back to simple
			   read() + write() */
			break;
	}

	while (true) {
		char buffer[8192];
		auto nbytes = in.Read(buffer, sizeof(buffer));
		if (nbytes <= 0)
			break;

		out.Write(buffer, nbytes);
	}
}

int
main(int argc, char **argv)
try {
	const char *path = "/run/cm4all/passage/socket";
	Entity request;

	int i = 1;
	for (i = 1; i < argc && *argv[i] == '-'; ++i) {
		if (auto p = StringAfterPrefix(argv[i], "--server=")) {
			path = p;
		} else if (auto h = StringAfterPrefix(argv[i], "--header=")) {
			const char *colon = strchr(h, ':');
			if (colon == nullptr || colon == h) {
				fprintf(stderr, "Malformed header: %s\n", h);
				throw Usage();
			}

			// TODO: verify valid name
			request.headers.emplace(std::piecewise_construct,
						std::forward_as_tuple(h, colon),
						std::forward_as_tuple(colon + 1));
		} else {
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			throw Usage();
		}
	}

	if (i >= argc)
		throw Usage();

	const StringView command(argv[i++]);
	CheckCommand(command);
	request.command.assign(command.data, command.size);

	for (auto args_tail = request.args.before_begin(); i < argc; ++i)
		args_tail = request.args.emplace_after(args_tail, argv[i]);

	auto fd = CreateConnect(path);
	SendRequest(fd, request);
	auto returned_fd = ReceiveResponse(fd);

	if (returned_fd.IsDefined() && returned_fd.IsPipe()) {
		/* if the returned file descriptor is a pipe, copy its
		   data to stdout */
		fd.Close();
		Copy(returned_fd, FileDescriptor(STDOUT_FILENO));
	}

	return EXIT_SUCCESS;
} catch (Usage) {
	fprintf(stderr, "Usage: %s [--server=PATH] [--header=NAME:VALUE ...] COMMAND [ARGS...]\n",
		argv[0]);
	return EXIT_FAILURE;
} catch (const std::exception &e) {
	PrintException(e);
	return EXIT_FAILURE;
}
