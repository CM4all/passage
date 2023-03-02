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

	if (!lua_isstring(L, 1))
		luaL_argerror(L, 1, "string expected");

	const char *s = lua_tostring(L, 1);

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

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
