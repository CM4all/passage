// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Verify.hxx"
#include "Entity.hxx"
#include "Parser.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "net/ConnectSocket.hxx"
#include "net/ReceiveMessage.hxx"
#include "net/LocalSocketAddress.hxx"
#include "net/SocketProtocolError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"
#include "util/StringCompare.hxx"
#include "util/SpanCast.hxx"

#include <fmt/format.h>

#include <thread>

#include <limits.h> // for INT_MAX
#include <stdlib.h>
#include <fcntl.h>

struct Usage {};

static UniqueSocketDescriptor
CreateConnect(std::string_view path)
{
	return CreateConnectSocket(LocalSocketAddress{path}, SOCK_SEQPACKET);
}

static void
SendOrThrow(SocketDescriptor fd, std::span<const std::byte> payload)
{
	ssize_t nbytes = fd.Send(payload);
	if (nbytes < 0)
		throw MakeErrno("Failed to send");
	if (size_t(nbytes) < payload.size())
		throw MakeErrno("Short send");
}

static void
SendRequest(SocketDescriptor fd, const Entity &request)
{
	const auto payload = request.Serialize();
	SendOrThrow(fd, AsBytes(payload));
}

static std::vector<UniqueFileDescriptor>
ReceiveResponse(SocketDescriptor s)
{
	ReceiveMessageBuffer<4096, 1024> buffer;
	auto result = ReceiveMessage(s, buffer, 0);
	if (result.payload.empty())
		throw std::runtime_error("Server closed the connection prematurely");

	const auto response = ParseEntity(ToStringView(result.payload));

	if (response.command == "OK") {
		if (!response.body.empty())
			// TODO let caller decide what to do with the response body
			(void)FileDescriptor{STDOUT_FILENO}.Write(AsBytes(response.body));

		return std::move(result.fds);
	} else if (response.command == "ERROR") {
		if (response.args.empty())
			throw std::runtime_error("Server error");
		else
			throw FmtRuntimeError("Server error: {:?}",
					      response.args.front());
	} else
		throw SocketProtocolError{"Malformed response"};
}

static void
Copy(FileDescriptor in, FileDescriptor out)
{
	while (true) {
		ssize_t nbytes = splice(in.Get(), nullptr, out.Get(), nullptr,
					INT_MAX, SPLICE_F_MOVE);
		if (nbytes == 0)
			return;

		if (nbytes < 0)
			/* whatever happened - fall back to simple
			   read() + write() */
			break;
	}

	while (true) {
		std::byte buffer[8192];
		auto nbytes = in.Read(buffer);
		if (nbytes <= 0)
			break;

		out.FullWrite(std::span{buffer}.first(nbytes));
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
				fmt::print(stderr, "Malformed header: {}\n", h);
				throw Usage();
			}

			// TODO: verify valid name
			request.headers.emplace(std::piecewise_construct,
						std::forward_as_tuple(h, colon),
						std::forward_as_tuple(colon + 1));
		} else {
			fmt::print(stderr, "Unknown option: {}\n", argv[i]);
			throw Usage();
		}
	}

	if (i >= argc)
		throw Usage();

	const std::string_view command(argv[i++]);
	CheckCommand(command);
	request.command = command;

	for (auto args_tail = request.args.before_begin(); i < argc; ++i)
		args_tail = request.args.emplace_after(args_tail, argv[i]);

	auto fd = CreateConnect(path);
	SendRequest(fd, request);
	auto returned_fds = ReceiveResponse(fd);

	if (!returned_fds.empty() && returned_fds.front().IsPipe()) {
		/* if the returned file descriptor is a pipe, copy its
		   data to stdout */
		fd.Close();

		const auto stdout_pipe = std::move(returned_fds.front());

		std::jthread stderr_thread;

		if (returned_fds.size() >= 2 && returned_fds[1].IsPipe())
			stderr_thread = std::jthread{
				[stderr_pipe = std::move(returned_fds[1])](){
					Copy(stderr_pipe, FileDescriptor(STDERR_FILENO));
				},
			};

		returned_fds.clear();

		Copy(stdout_pipe, FileDescriptor(STDOUT_FILENO));

		if (stderr_thread.joinable())
			stderr_thread.join();
	}

	return EXIT_SUCCESS;
} catch (Usage) {
	fmt::print(stderr, "Usage: {} [--server=PATH] [--header=NAME:VALUE ...] COMMAND [ARGS...]\n",
		   argv[0]);
	return EXIT_FAILURE;
} catch (const std::exception &e) {
	PrintException(e);
	return EXIT_FAILURE;
}
