// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "LAction.hxx"
#include "Action.hxx"
#include "lua/Class.hxx"
#include "lua/Util.hxx"

struct LAction : Action {
	LAction(lua_State *L, Lua::StackIndex request_idx) {
		// fenv.request = request
		lua_newtable(L);
		Lua::SetTable(L, Lua::RelativeStackIndex{-1},
			      "request", request_idx);
		lua_setfenv(L, -2);
	}

	LAction(lua_State *L, Lua::StackIndex request_idx, Action &&_action)
		:Action(std::move(_action)) {
		// fenv.request = request
		lua_newtable(L);
		Lua::SetTable(L, Lua::RelativeStackIndex{-1},
			      "request", request_idx);
		lua_setfenv(L, -2);
	}

	static void PushRequest(lua_State *L, int action_idx) {
		lua_getfenv(L, action_idx);
		lua_getfield(L, -1, "request");
		lua_replace(L, -2);
	}
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
PushLuaActionRequest(lua_State *L, int action_idx)
{
	LAction::PushRequest(L, action_idx);
}
