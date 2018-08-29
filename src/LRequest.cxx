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

#include "LRequest.hxx"
#include "Entity.hxx"
#include "LAction.hxx"
#include "Action.hxx"
#include "LAddress.hxx"
#include "lua/String.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "CgroupProc.hxx"
#include "MountProc.hxx"
#include "util/StringAPI.hxx"

#include <assert.h>
#include <sys/socket.h>
#include <string.h>

class RichRequest : public Entity {
	const struct ucred cred;

public:
	RichRequest(Entity &&src, const struct ucred &_peer_cred)
		:Entity(std::move(src)), cred(_peer_cred) {}

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

static constexpr char lua_request_class[] = "passage.request";
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
} catch (...) {
	Lua::Raise(L, std::current_exception());
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
} catch (...) {
	Lua::Raise(L, std::current_exception());
}

static int
NewFadeChildrenAction(lua_State *L)
{
	const auto top = lua_gettop(L);
	if (top < 2 || top > 3)
		return luaL_error(L, "Invalid parameters");

	AllocatedSocketAddress address;

	try {
		address = GetLuaAddress(L, 2);
	} catch (const std::exception &e) {
		return luaL_error(L, e.what());
	}

	const char *child_tag = nullptr;
	if (top >= 3) {
		if (!lua_isstring(L, 3))
			luaL_argerror(L, 3, "string expected");

		child_tag = lua_tostring(L, 3);
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
		if (StringIsEqual(i->name, name)) {
			Lua::Push(L, i->func);
			return 1;
		}
	}

	if (StringIsEqual(name, "command")) {
		Lua::Push(L, request.command.c_str());
		return 1;
	} else if (StringIsEqual(name, "args")) {
		lua_newtable(L);

		int i = 1;
		for (const auto &a : request.args)
			Lua::SetTable(L, -3, i++, a);

		return 1;
	} else if (StringIsEqual(name, "headers")) {
		lua_newtable(L);

		for (const auto &i : request.headers)
			Lua::SetTable(L, -3, i.first, i.second);

		return 1;
	} else if (StringIsEqual(name, "pid")) {
		if (!request.HavePeerCred())
			return 0;

		Lua::Push(L, request.GetPid());
		return 1;
	} else if (StringIsEqual(name, "uid")) {
		if (!request.HavePeerCred())
			return 0;

		Lua::Push(L, request.GetUid());
		return 1;
	} else if (StringIsEqual(name, "gid")) {
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

Entity *
NewLuaRequest(lua_State *L, Entity &&src, const struct ucred &peer_cred)
{
	return LuaRequest::New(L, std::move(src), peer_cred);
}

Entity &
CastLuaRequest(lua_State *L, int idx)
{
	return LuaRequest::Cast(L, idx);
}
