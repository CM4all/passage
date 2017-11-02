/*
 * author: Max Kellermann <mk@cm4all.com>
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
