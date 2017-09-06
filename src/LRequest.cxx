/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "LRequest.hxx"
#include "Request.hxx"
#include "lua/Class.hxx"
#include "CgroupProc.hxx"
#include "MountProc.hxx"

#include <sys/socket.h>
#include <string.h>

class RichRequest : public Request {
	const struct ucred cred;

public:
	RichRequest(Request &&src, const struct ucred &_peer_cred)
		:Request(std::move(src)), cred(_peer_cred) {}

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
};

static constexpr char lua_request_class[] = "qrelay.request";
typedef Lua::Class<RichRequest, lua_request_class> LuaRequest;

static int
GetCgroup(lua_State *L)
try {
	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameters");

	auto &request = (RichRequest &)CastLuaRequest(L, 1);

	if (!lua_isstring(L, 2))
		luaL_argerror(L, 2, "string expected");
	const char *controller = lua_tostring(L, 2);

	const int pid = request.GetPid();
	if (pid < 0)
		return 0;

	const auto path = ReadProcessCgroup(pid, controller);
	if (path.empty())
		return 0;

	Lua::Push(L, path.c_str());
	return 1;
} catch (const std::runtime_error &e) {
	return luaL_error(L, e.what());
}

static int
GetMountInfo(lua_State *L)
try {
	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameters");

	auto &request = (RichRequest &)CastLuaRequest(L, 1);

	if (!lua_isstring(L, 2))
		luaL_argerror(L, 2, "string expected");
	const char *mountpoint = lua_tostring(L, 2);

	const int pid = request.GetPid();
	if (pid < 0)
		return 0;

	const auto m = ReadProcessMount(pid, mountpoint);
	if (!m.IsDefined())
		return 0;

	lua_newtable(L);
	Lua::SetField(L, -2, "root", m.root.c_str());
	Lua::SetField(L, -2, "filesystem", m.filesystem.c_str());
	Lua::SetField(L, -2, "source", m.source.c_str());
	return 1;
} catch (const std::runtime_error &e) {
	return luaL_error(L, e.what());
}

static constexpr struct luaL_reg request_methods [] = {
	{"get_cgroup", GetCgroup},
	{"get_mount_info", GetMountInfo},
	{nullptr, nullptr}
};

static int
LuaRequestIndex(lua_State *L)
{
	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameters");

	auto &request = (RichRequest &)CastLuaRequest(L, 1);

	if (!lua_isstring(L, 2))
		luaL_argerror(L, 2, "string expected");

	const char *name = lua_tostring(L, 2);

	for (const auto *i = request_methods; i->name != nullptr; ++i) {
		if (strcmp(i->name, name) == 0) {
			Lua::Push(L, i->func);
			return 1;
		}
	}

	if (strcmp(name, "command") == 0) {
		Lua::Push(L, request.GetCommand().c_str());
		return 1;
	} else if (strcmp(name, "pid") == 0) {
		if (!request.HavePeerCred())
			return 0;

		Lua::Push(L, request.GetPid());
		return 1;
	} else if (strcmp(name, "uid") == 0) {
		if (!request.HavePeerCred())
			return 0;

		Lua::Push(L, request.GetUid());
		return 1;
	} else if (strcmp(name, "gid") == 0) {
		if (!request.HavePeerCred())
			return 0;

		Lua::Push(L, request.GetGid());
		return 1;
	}

	return luaL_error(L, "Unknown attribute");
}

void
RegisterLuaRequest(lua_State *L)
{
	LuaRequest::Register(L);
	Lua::SetTable(L, -3, "__index", LuaRequestIndex);
	lua_pop(L, 1);
}

Request *
NewLuaRequest(lua_State *L, Request &&src, const struct ucred &peer_cred)
{
	return LuaRequest::New(L, std::move(src), peer_cred);
}

Request &
CastLuaRequest(lua_State *L, int idx)
{
	return LuaRequest::Cast(L, idx);
}
