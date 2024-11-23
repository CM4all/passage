// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct ucred;
struct lua_State;
struct Entity;
class FileDescriptor;
namespace Lua { class AutoCloseList; }

void
RegisterLuaRequest(lua_State *L);

Entity *
NewLuaRequest(lua_State *L, Lua::AutoCloseList &auto_close,
	      Entity &&src, const struct ucred &peer_cred,
	      FileDescriptor pidfd);

Entity &
CastLuaRequest(lua_State *L, int idx);
