// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct ucred;
struct lua_State;
struct Entity;

void
RegisterLuaRequest(lua_State *L);

Entity *
NewLuaRequest(lua_State *L,
	      Entity &&src, const struct ucred &peer_cred);

Entity &
CastLuaRequest(lua_State *L, int idx);
