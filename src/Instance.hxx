/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef INSTANCE_HXX
#define INSTANCE_HXX

#include "Listener.hxx"
#include "spawn/Registry.hxx"
#include "io/Logger.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "lua/State.hxx"
#include "lua/ValuePtr.hxx"

#include <forward_list>

struct SocketAddress;
class UniqueSocketDescriptor;

class Instance final {
	EventLoop event_loop;
	ShutdownListener shutdown_listener;

	ChildProcessRegistry child_process_registry;

	Lua::State lua_state;

	std::forward_list<PassageListener> listeners;

public:
	RootLogger logger;

	Instance();
	~Instance();

	EventLoop &GetEventLoop() {
		return event_loop;
	}

	ChildProcessRegistry &GetChildProcessRegistry() {
		return child_process_registry;
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
	void ShutdownCallback();
};

#endif
