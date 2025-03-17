// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Listener.hxx"
#include "lua/ReloadRunner.hxx"
#include "lua/State.hxx"
#include "lua/ValuePtr.hxx"
#include "spawn/ZombieReaper.hxx"
#include "io/Logger.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/SignalEvent.hxx"
#include "config.h"

#ifdef HAVE_LIBSYSTEMD
#include "event/systemd/Watchdog.hxx"
#endif

#include <forward_list>

class SocketAddress;
class UniqueSocketDescriptor;

class Instance final {
	EventLoop event_loop;
	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};
	SignalEvent sighup_event;

#ifdef HAVE_LIBSYSTEMD
	Systemd::Watchdog systemd_watchdog{event_loop};
#endif

	ZombieReaper zombie_reaper{event_loop};

	Lua::State lua_state;

	Lua::ReloadRunner reload{lua_state.get()};

	std::forward_list<PassageListener> listeners;

public:
	RootLogger logger;

	Instance();
	~Instance() noexcept;

	auto &GetEventLoop() noexcept {
		return event_loop;
	}

	lua_State *GetLuaState() {
		return lua_state.get();
	}

	void AddListener(UniqueSocketDescriptor &&fd,
			 Lua::ValuePtr &&handler);

	void AddListener(SocketAddress address,
			 Lua::ValuePtr &&handler);

#ifdef HAVE_LIBSYSTEMD
	/**
	 * Listen for incoming connections on sockets passed by systemd
	 * (systemd socket activation).
	 */
	void AddSystemdListener(Lua::ValuePtr &&handler);
#endif // HAVE_LIBSYSTEMD

	void Check();

private:
	void OnShutdown() noexcept;
	void OnReload(int) noexcept;
};
