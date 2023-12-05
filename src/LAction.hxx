// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

struct lua_State;
struct Action;

void
RegisterLuaAction(lua_State *L);

/**
 * @param request_idx the index of the associated #Request instance on
 * the Lua stack
 */
Action *
NewLuaAction(lua_State *L, int request_idx);

Action *
NewLuaAction(lua_State *L, int request_idx, Action &&action);

Action *
CheckLuaAction(lua_State *L, int idx);

/**
 * Push the associated request object on the Lua stack.
 */
void
PushLuaActionRequest(lua_State *L, const Action &action);
