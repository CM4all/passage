/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Listener.hxx"
#include "event/net/UdpListener.hxx"
#include "net/SocketAddress.hxx"
#include "system/Error.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <systemd/sd-daemon.h>

#include <stdexcept>

#include <sys/un.h>

Instance::Instance()
	:shutdown_listener(event_loop, BIND_THIS_METHOD(ShutdownCallback)),
	 lua_state(luaL_newstate())
{
	shutdown_listener.Enable();
}

Instance::~Instance()
{
}

inline void
Instance::AddListener(UniqueSocketDescriptor &&fd, Lua::ValuePtr &&handler)
{
	listeners.emplace_front(event_loop, std::move(handler),
				logger, event_loop);
	listeners.front().Listen(std::move(fd));
}

static UniqueSocketDescriptor
MakeListener(SocketAddress address)
{
	const int family = address.GetFamily();
	constexpr int socktype = SOCK_SEQPACKET;
	constexpr int protocol = 0;

	UniqueSocketDescriptor fd;
	if (!fd.CreateNonBlock(family, socktype, protocol))
		throw MakeErrno("Failed to create socket");

	if (family == AF_LOCAL) {
		const struct sockaddr_un *sun = (const struct sockaddr_un *)address.GetAddress();
		if (sun->sun_path[0] != '\0')
			/* delete non-abstract socket files before reusing them */
			unlink(sun->sun_path);

		/* we want to receive the client's UID */
		fd.SetBoolOption(SOL_SOCKET, SO_PASSCRED, true);
	}

	if (!fd.Bind(address))
		throw MakeErrno("Failed to bind");

	if (!fd.Listen(64))
		throw MakeErrno("Failed to listen");

	return fd;
}

void
Instance::AddListener(SocketAddress address, Lua::ValuePtr &&handler)
{
	AddListener(MakeListener(address), std::move(handler));
}

void
Instance::AddSystemdListener(Lua::ValuePtr &&handler)
{
	int n = sd_listen_fds(true);
	if (n < 0)
		throw MakeErrno("sd_listen_fds() failed");

	if (n == 0)
		throw std::runtime_error("No systemd socket");

	for (unsigned i = 0; i < unsigned(n); ++i)
		AddListener(UniqueSocketDescriptor(SD_LISTEN_FDS_START + i),
			    std::move(handler));
}

void
Instance::Check()
{
	if (listeners.empty())
		throw std::runtime_error("No listeners configured");
}

void
Instance::ShutdownCallback()
{
	logger(3, "quit");
	event_loop.Break();
}
