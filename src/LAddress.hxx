/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

struct lua_State;
class SocketAddress;
class AllocatedSocketAddress;

void
RegisterLuaAddress(lua_State *L);

AllocatedSocketAddress *
NewLuaAddress(lua_State *L, SocketAddress src);

AllocatedSocketAddress *
NewLuaAddress(lua_State *L, AllocatedSocketAddress &&src);

AllocatedSocketAddress
GetLuaAddress(lua_State *L, int idx);
