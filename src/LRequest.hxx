// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct lua_State;
struct Entity;
class SocketPeerAuth;
namespace Lua { class AutoCloseList; }

void
RegisterLuaRequest(lua_State *L);

Entity *
NewLuaRequest(lua_State *L, Lua::AutoCloseList &auto_close,
	      Entity &&src, const SocketPeerAuth &peer_auth);

Entity &
CastLuaRequest(lua_State *L, int idx);
