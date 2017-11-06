/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Protocol.hxx"
#include "net/ReceiveMessage.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"
#include "util/StringCompare.hxx"
#include "util/StringView.hxx"

#include <stdlib.h>

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
SendRequest(SocketDescriptor fd, StringView command)
{
	ssize_t nbytes = send(fd.Get(), command.data, command.size, 0);
	if (nbytes < 0)
		throw MakeErrno("Failed to send request");
	if (size_t(nbytes) < command.size)
		throw MakeErrno("Short send");
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

	const char *newline = payload.Find('\n');
	if (newline != nullptr)
		payload.SetEnd(newline);

	if (payload.StartsWith("OK") &&
	    (payload.size == 2 || payload[2] == ' ')) {
		return result.fds.empty()
			? UniqueFileDescriptor()
			: std::move(result.fds.front());
	} else if (payload.StartsWith("ERROR")) {
		if (payload.size == 5)
			throw std::runtime_error("Server error");
		if (payload[5] == ' ')
			throw std::runtime_error("Server error: " +
						 std::string(payload.data + 6,
							     payload.size - 6));
		else
			throw std::runtime_error("Malformed server error");
	} else
		throw std::runtime_error("Malformed response");
}

static void
Copy(FileDescriptor in, FileDescriptor out)
{
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

	int i = 1;
	for (i = 1; i < argc && *argv[i] == '-'; ++i) {
		if (auto p = StringAfterPrefix(argv[i], "--server=")) {
			path = p;
		} else {
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			throw Usage();
		}
	}

	if (i + 1 != argc)
		throw Usage();

	const StringView command(argv[i]);

	CheckCommand(command);

	auto fd = CreateConnect(path);
	SendRequest(fd, command);
	auto returned_fd = ReceiveResponse(fd);

	if (returned_fd.IsDefined() && returned_fd.IsPipe()) {
		/* if the returned file descriptor is a pipe, copy its
		   data to stdout */
		fd.Close();
		Copy(returned_fd.ToFileDescriptor(),
		     FileDescriptor(STDOUT_FILENO));
	}

	return EXIT_SUCCESS;
} catch (Usage) {
	fprintf(stderr, "Usage: %s [--server=PATH] COMMAND\n", argv[0]);
	return EXIT_FAILURE;
} catch (const std::exception &e) {
	PrintException(e);
	return EXIT_FAILURE;
}
