// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LResolver.hxx"
#include "lua/Util.hxx"
#include "lua/Error.hxx"
#include "lua/net/SocketAddress.hxx"
#include "net/Resolver.hxx"
#include "net/AddressInfo.hxx"

extern "C" {
#include <lauxlib.h>
}

#include <string.h>

static int
l_control_resolve(lua_State *L)
try {
	if (lua_gettop(L) != 1)
		return luaL_error(L, "Invalid parameter count");

	const char *s = luaL_checkstring(L, 1);

	static constexpr auto hints = MakeAddrInfo(0, AF_UNSPEC, SOCK_DGRAM);

	const auto ai = Resolve(s, 5478, &hints);
	Lua::NewSocketAddress(L, std::move(ai.GetBest()));
	return 1;
} catch (...) {
	Lua::RaiseCurrent(L);
}

void
RegisterLuaResolver(lua_State *L)
{
	Lua::SetGlobal(L, "control_resolve", l_control_resolve);
}

void
UnregisterLuaResolver(lua_State *L)
{
	Lua::SetGlobal(L, "control_resolve", nullptr);
}
