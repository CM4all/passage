/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Protocol.hxx"
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

static void
ReceiveResponse(SocketDescriptor fd)
{
	char buffer[4096];
	ssize_t nbytes = recv(fd.Get(), buffer, sizeof(buffer) - 1, 0);
	if (nbytes < 0)
		throw MakeErrno("Failed to receive response");
	if (nbytes == 0)
		throw std::runtime_error("Server closed the connection prematurely");

	buffer[nbytes] = 0;

	char *newline = strchr(buffer, '\n');
	if (newline != nullptr)
		newline = 0;

	if (strncmp(buffer, "OK", 2) == 0 &&
	    (buffer[2] == 0 || buffer[2] == ' ')) {
		return;
	} else if (strncmp(buffer, "ERROR", 5) == 0) {
		if (buffer[5] == ' ')
			throw std::runtime_error(std::string("Server error: ") + (buffer + 6));
		else if (buffer[5] == 0)
			throw std::runtime_error("Server error");
		else
			throw std::runtime_error("Malformed server error");
	} else
		throw std::runtime_error("Malformed response");
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
	ReceiveResponse(fd);

	return EXIT_SUCCESS;
} catch (Usage) {
	fprintf(stderr, "Usage: %s [--server=PATH] COMMAND\n", argv[0]);
	return EXIT_FAILURE;
} catch (const std::exception &e) {
	PrintException(e);
	return EXIT_FAILURE;
}
