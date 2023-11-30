// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "LRequest.hxx"
#include "Entity.hxx"
#include "LAction.hxx"
#include "Action.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/FenvCache.hxx"
#include "lua/io/XattrTable.hxx"
#include "lua/net/SocketAddress.hxx"
#include "io/Beneath.hxx"
#include "io/FileAt.hxx"
#include "io/Open.hxx"
#include "io/linux/MountInfo.hxx"
#include "io/linux/ProcCgroup.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"

#include <assert.h>
#include <sys/socket.h>
#include <string.h>

class RichRequest : public Entity {
	const struct ucred cred;

public:
	RichRequest(lua_State *L, Entity &&src, const struct ucred &_peer_cred)
		:Entity(std::move(src)),
		 cred(_peer_cred)
	{
		lua_newtable(L);
		lua_setfenv(L, -2);
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

	int Index(lua_State *L, const char *name);
};

static constexpr char lua_request_class[] = "passage.request";
typedef Lua::Class<RichRequest, lua_request_class> LuaRequest;

static int
GetCgroup(lua_State *L)
try {
	const auto top = lua_gettop(L);
	if (top < 1 || top > 2)
		return luaL_error(L, "Invalid parameters");

	auto &request = (RichRequest &)CastLuaRequest(L, 1);

	const char *controller = luaL_optstring(L, 2, "");
	if (controller != nullptr && !StringIsEmpty(controller))
		luaL_argerror(L, 2, "cgroup1 not supported anymore");

	const int pid = request.GetPid();
	if (pid < 0)
		return 0;

	const auto path = ReadProcessCgroup(pid);
	if (path.empty())
		return 0;

	Lua::Push(L, path);
	return 1;
} catch (...) {
	Lua::RaiseCurrent(L);
}

static int
GetMountInfo(lua_State *L)
try {
	using namespace Lua;

	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameters");

	auto &request = (RichRequest &)CastLuaRequest(L, 1);
	const char *const mountpoint = luaL_checkstring(L, 2);

	const int pid = request.GetPid();
	if (pid < 0)
		return 0;

	const auto m = ReadProcessMount(pid, mountpoint);
	if (!m.IsDefined())
		return 0;

	lua_newtable(L);
	SetField(L, RelativeStackIndex{-1}, "root", m.root);
	SetField(L, RelativeStackIndex{-1}, "filesystem", m.filesystem);
	SetField(L, RelativeStackIndex{-1}, "source", m.source);
	return 1;
} catch (...) {
	Lua::RaiseCurrent(L);
}

static int
NewFadeChildrenAction(lua_State *L)
{
	const auto top = lua_gettop(L);
	if (top < 2 || top > 3)
		return luaL_error(L, "Invalid parameters");

	AllocatedSocketAddress address;

	try {
		address = Lua::ToSocketAddress(L, 2, 5478);
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
	{"get_cgroup", GetCgroup},
	{"get_mount_info", GetMountInfo},
	{"fade_children", NewFadeChildrenAction},
	{"exec_pipe", NewExecPipeAction},
	{nullptr, nullptr}
};

inline int
RichRequest::Index(lua_State *L, const char *name)
{
	using namespace Lua;

	for (const auto *i = request_methods; i->name != nullptr; ++i) {
		if (StringIsEqual(i->name, name)) {
			Lua::Push(L, i->func);
			return 1;
		}
	}

	// look it up in the fenv (our cache)
	if (Lua::GetFenvCache(L, 1, name))
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

		Lua::Push(L, path);
		return 1;
	} else if (StringIsEqual(name, "cgroup_xattr")) {
		if (!HavePeerCred())
			return 0;

		const auto path = ReadProcessCgroup(GetPid());
		if (path.empty())
			return 0;

		try {
			const auto sys_fs_cgroup = OpenPath("/sys/fs/cgroup");
			auto fd = OpenReadOnlyBeneath({sys_fs_cgroup, path.c_str() + 1});
			Lua::NewXattrTable(L, std::move(fd));
		} catch (...) {
			Lua::RaiseCurrent(L);
		}

		// copy a reference to the fenv (our cache)
		Lua::SetFenvCache(L, 1, name, Lua::RelativeStackIndex{-1});

		return 1;
	} else
		return luaL_error(L, "Unknown attribute");
}

static int
LuaRequestIndex(lua_State *L)
{
	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameters");

	auto &request = (RichRequest &)CastLuaRequest(L, 1);

	return request.Index(L, luaL_checkstring(L, 2));
}

void
RegisterLuaRequest(lua_State *L)
{
	using namespace Lua;

	LuaRequest::Register(L);
	SetTable(L, RelativeStackIndex{-1}, "__index", LuaRequestIndex);
	lua_pop(L, 1);
}

Entity *
NewLuaRequest(lua_State *L, lua_State *main_L,
	      Entity &&src, const struct ucred &peer_cred)
{
	return LuaRequest::New(L, main_L, std::move(src), peer_cred);
}

Entity &
CastLuaRequest(lua_State *L, int idx)
{
	return LuaRequest::Cast(L, idx);
}
