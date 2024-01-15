// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LRequest.hxx"
#include "Entity.hxx"
#include "LAction.hxx"
#include "Action.hxx"
#include "lua/AutoCloseList.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/FenvCache.hxx"
#include "lua/io/CgroupInfo.hxx"
#include "lua/net/SocketAddress.hxx"
#include "net/control/Protocol.hxx"
#include "io/Beneath.hxx"
#include "io/FileAt.hxx"
#include "io/Open.hxx"
#include "io/linux/ProcCgroup.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"

#include <assert.h>
#include <sys/socket.h>
#include <string.h>

class RichRequest : public Entity {
	Lua::AutoCloseList *auto_close;

	const struct ucred cred;

public:
	RichRequest(lua_State *L, Lua::AutoCloseList &_auto_close,
		    Entity &&src, const struct ucred &_peer_cred)
		:Entity(std::move(src)),
		 auto_close(&_auto_close),
		 cred(_peer_cred)
	{
		auto_close->Add(L, Lua::RelativeStackIndex{-1});

		lua_newtable(L);
		lua_setfenv(L, -2);
	}

	bool IsStale() const noexcept {
		return auto_close == nullptr;
	}

	bool HavePeerCred() const noexcept {
		return cred.pid >= 0;
	}

	int GetPid() const noexcept {
		assert(HavePeerCred());

		return cred.pid;
	}

	int GetUid() const noexcept {
		assert(HavePeerCred());

		return cred.uid;
	}

	int GetGid() const noexcept{
		assert(HavePeerCred());

		return cred.gid;
	}

	int Close(lua_State *) {
		auto_close = nullptr;
		return 0;
	}

	int Index(lua_State *L);
};

static constexpr char lua_request_class[] = "passage.request";
typedef Lua::Class<RichRequest, lua_request_class> LuaRequest;

static int
NewFadeChildrenAction(lua_State *L)
{
	const auto top = lua_gettop(L);
	if (top < 2 || top > 3)
		return luaL_error(L, "Invalid parameters");

	AllocatedSocketAddress address;

	try {
		address = Lua::ToSocketAddress(L, 2, BengProxy::CONTROL_PORT);
	} catch (const std::exception &e) {
		return luaL_error(L, e.what());
	}

	const char *child_tag = nullptr;
	if (top >= 3) {
		child_tag = luaL_checkstring(L, 3);
	}

	auto &action = *NewLuaAction(L, 1);
	action.type = Action::Type::FADE_CHILDREN;
	action.address = std::move(address);
	if (child_tag != nullptr)
		action.param = child_tag;
	return 1;
}

static int
NewExecPipeAction(lua_State *L)
{
	const auto top = lua_gettop(L);
	if (top != 2)
		return luaL_error(L, "Invalid parameters");

	if (!lua_istable(L, 2))
		luaL_argerror(L, 2, "array expected");

	Action action;
	action.type = Action::Type::EXEC_PIPE;

	auto tail = action.args.before_begin();

	for (lua_pushnil(L); lua_next(L, 2); lua_pop(L, 1)) {
		if (!lua_isstring(L, -1))
			luaL_error(L, "string expected");

		tail = action.args.emplace_after(tail, lua_tostring(L, -1));
	}

	NewLuaAction(L, 1, std::move(action));
	return 1;
}

static constexpr struct luaL_Reg request_methods [] = {
	{"fade_children", NewFadeChildrenAction},
	{"exec_pipe", NewExecPipeAction},
	{nullptr, nullptr}
};

inline int
RichRequest::Index(lua_State *L)
{
	using namespace Lua;

	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameters");

	constexpr Lua::StackIndex name_idx{2};
	const char *const name = luaL_checkstring(L, 2);

	if (IsStale())
		return luaL_error(L, "Stale object");

	for (const auto *i = request_methods; i->name != nullptr; ++i) {
		if (StringIsEqual(i->name, name)) {
			Lua::Push(L, i->func);
			return 1;
		}
	}

	// look it up in the fenv (our cache)
	if (Lua::GetFenvCache(L, 1, name_idx))
		return 1;

	if (StringIsEqual(name, "command")) {
		Lua::Push(L, command);
		return 1;
	} else if (StringIsEqual(name, "args")) {
		lua_newtable(L);

		lua_Integer i = 1;
		for (const auto &a : args)
			SetTable(L, RelativeStackIndex{-1}, i++, a);

		return 1;
	} else if (StringIsEqual(name, "headers")) {
		lua_newtable(L);

		for (const auto &i : headers)
			SetTable(L, RelativeStackIndex{-1}, i.first, i.second);

		return 1;
	} else if (StringIsEqual(name, "pid")) {
		if (!HavePeerCred())
			return 0;

		Lua::Push(L, static_cast<lua_Integer>(GetPid()));
		return 1;
	} else if (StringIsEqual(name, "uid")) {
		if (!HavePeerCred())
			return 0;

		Lua::Push(L, static_cast<lua_Integer>(GetUid()));
		return 1;
	} else if (StringIsEqual(name, "gid")) {
		if (!HavePeerCred())
			return 0;

		Lua::Push(L, static_cast<lua_Integer>(GetGid()));
		return 1;
	} else if (StringIsEqual(name, "cgroup")) {
		if (!HavePeerCred())
			return 0;

		const auto path = ReadProcessCgroup(GetPid());
		if (path.empty())
			return 0;

		Lua::NewCgroupInfo(L, *auto_close, path);

		// copy a reference to the fenv (our cache)
		Lua::SetFenvCache(L, 1, name_idx, Lua::RelativeStackIndex{-1});

		return 1;
	} else
		return luaL_error(L, "Unknown attribute");
}

void
RegisterLuaRequest(lua_State *L)
{
	using namespace Lua;

	LuaRequest::Register(L);
	SetField(L, RelativeStackIndex{-1}, "__close", LuaRequest::WrapMethod<&RichRequest::Close>());
	SetField(L, RelativeStackIndex{-1}, "__index", LuaRequest::WrapMethod<&RichRequest::Index>());
	lua_pop(L, 1);
}

Entity *
NewLuaRequest(lua_State *L, Lua::AutoCloseList &auto_close,
	      Entity &&src, const struct ucred &peer_cred)
{
	return LuaRequest::New(L, L, auto_close,
			       std::move(src), peer_cred);
}

Entity &
CastLuaRequest(lua_State *L, int idx)
{
	return LuaRequest::Cast(L, idx);
}
