/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

struct ucred;
struct lua_State;
struct Request;

void
RegisterLuaRequest(lua_State *L);

Request *
NewLuaRequest(lua_State *L, Request &&src, const struct ucred &peer_cred);

Request &
CastLuaRequest(lua_State *L, int idx);
