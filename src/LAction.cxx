/*
 * Copyright 2017-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "LAction.hxx"
#include "Action.hxx"
#include "lua/Class.hxx"
#include "lua/Value.hxx"

struct LAction : Action {
	Lua::Value request;

	LAction(lua_State *L, Lua::StackIndex request_idx)
		:request(L, request_idx) {}

	LAction(lua_State *L, Lua::StackIndex request_idx, Action &&_action)
		:Action(std::move(_action)), request(L, request_idx) {}
};

static constexpr char lua_action_class[] = "passage.action";
typedef Lua::Class<LAction, lua_action_class> LuaAction;

void
RegisterLuaAction(lua_State *L)
{
	LuaAction::Register(L);
	lua_pop(L, 1);
}

Action *
NewLuaAction(lua_State *L, int request_idx)
{
	return LuaAction::New(L, L, Lua::StackIndex(request_idx));
}

Action *
NewLuaAction(lua_State *L, int request_idx, Action &&action)
{
	return LuaAction::New(L, L, Lua::StackIndex(request_idx),
			      std::move(action));
}

Action *
CheckLuaAction(lua_State *L, int idx)
{
	return LuaAction::Check(L, idx);
}

void
PushLuaActionRequest(const Action &_action)
{
	auto &action = (const LAction &)_action;
	action.request.Push();
}
