// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LResolver.hxx"
#include "lua/Util.hxx"
#include "lua/net/Resolver.hxx"
#include "net/AddressInfo.hxx"
#include "net/control/Protocol.hxx"

extern "C" {
#include <lua.h>
}

void
RegisterLuaResolver(lua_State *L)
{
	static constexpr auto hints = MakeAddrInfo(0, AF_UNSPEC, SOCK_STREAM);
	Lua::PushResolveFunction(L, hints, BengControl::DEFAULT_PORT);
	lua_setglobal(L, "control_resolve");
}

void
UnregisterLuaResolver(lua_State *L)
{
	Lua::SetGlobal(L, "control_resolve", nullptr);
}
