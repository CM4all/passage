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
#include "event/systemd/Watchdog.hxx"

#include <forward_list>

class SocketAddress;
class UniqueSocketDescriptor;

class Instance final {
	EventLoop event_loop;
	ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};
	SignalEvent sighup_event;

	Systemd::Watchdog systemd_watchdog{event_loop};

	ZombieReaper zombie_reaper{event_loop};

	Lua::State lua_state;

	Lua::ReloadRunner reload{lua_state.get()};

	std::forward_list<PassageListener> listeners;

public:
	RootLogger logger;

	Instance();
	~Instance() noexcept;

	EventLoop &GetEventLoop() {
		return event_loop;
	}

	lua_State *GetLuaState() {
		return lua_state.get();
	}

	void AddListener(UniqueSocketDescriptor &&fd,
			 Lua::ValuePtr &&handler);

	void AddListener(SocketAddress address,
			 Lua::ValuePtr &&handler);

	/**
	 * Listen for incoming connections on sockets passed by systemd
	 * (systemd socket activation).
	 */
	void AddSystemdListener(Lua::ValuePtr &&handler);

	void Check();

private:
	void OnShutdown() noexcept;
	void OnReload(int) noexcept;
};
