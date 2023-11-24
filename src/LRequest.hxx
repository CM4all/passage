// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct ucred;
struct lua_State;
struct Entity;

void
RegisterLuaRequest(lua_State *L);

/**
 * @param L the lua_State on whose stack the new object will be pushed
 * @param main_L the lua_State which is used to destruct Lua values
 * owned by the new object
 */
Entity *
NewLuaRequest(lua_State *L, lua_State *main_L,
	      Entity &&src, const struct ucred &peer_cred);

Entity &
CastLuaRequest(lua_State *L, int idx);
