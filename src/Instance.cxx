// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Instance.hxx"
#include "Listener.hxx"
#include "net/SocketConfig.hxx"
#include "system/Error.hxx"

extern "C" {
#include <lauxlib.h>
}

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <stdexcept>

Instance::Instance()
	:sighup_event(event_loop, SIGHUP, BIND_THIS_METHOD(OnReload)),
	 lua_state(luaL_newstate())
{
	shutdown_listener.Enable();
	sighup_event.Enable();
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

	const SocketConfig config{
		.bind_address = AllocatedSocketAddress{address},
		.listen = 64,
		.mode = 0666,

		/* we want to receive the client's UID */
		.pass_cred = true,
	};

	return config.Create(socktype);
}

void
Instance::AddListener(SocketAddress address, Lua::ValuePtr &&handler)
{
	AddListener(MakeListener(address), std::move(handler));
}

#ifdef HAVE_LIBSYSTEMD

void
Instance::AddSystemdListener(Lua::ValuePtr &&handler)
{
	int n = sd_listen_fds(true);
	if (n < 0)
		throw MakeErrno("sd_listen_fds() failed");

	if (n == 0)
		throw std::runtime_error("No systemd socket");

	for (unsigned i = 0; i < unsigned(n); ++i)
		AddListener(UniqueSocketDescriptor(AdoptTag{}, SD_LISTEN_FDS_START + i),
			    Lua::ValuePtr(handler));
}

#endif // HAVE_LIBSYSTEMD

void
Instance::Check()
{
	if (listeners.empty())
		throw std::runtime_error("No listeners configured");
}

void
Instance::OnShutdown() noexcept
{
	shutdown_listener.Disable();
	sighup_event.Disable();

#ifdef HAVE_LIBSYSTEMD
	systemd_watchdog.Disable();
#endif

	event_loop.Break();
}

void
Instance::OnReload(int) noexcept
{
	reload.Start();
}
