// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Instance.hxx"
#include "Listener.hxx"
#include "event/net/UdpListener.hxx"
#include "net/SocketConfig.hxx"
#include "system/Error.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <systemd/sd-daemon.h>

#include <stdexcept>

Instance::Instance()
	:shutdown_listener(event_loop, BIND_THIS_METHOD(ShutdownCallback)),
	 lua_state(luaL_newstate())
{
	shutdown_listener.Enable();
}

Instance::~Instance() noexcept = default;

inline void
Instance::AddListener(UniqueSocketDescriptor &&fd, Lua::ValuePtr &&handler)
{
	listeners.emplace_front(event_loop, *this, std::move(handler),
				logger);
	listeners.front().Listen(std::move(fd));
}

static UniqueSocketDescriptor
MakeListener(SocketAddress address)
{
	constexpr int socktype = SOCK_SEQPACKET;

	SocketConfig config(address);
	config.mode = 0666;

	/* we want to receive the client's UID */
	config.pass_cred = true;

	config.listen = 64;

	return config.Create(socktype);
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
			    Lua::ValuePtr(handler));
}

void
Instance::Check()
{
	if (listeners.empty())
		throw std::runtime_error("No listeners configured");
}

void
Instance::ShutdownCallback() noexcept
{
	logger(3, "quit");
	event_loop.Break();
}
