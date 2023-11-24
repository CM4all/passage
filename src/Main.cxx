// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CommandLine.hxx"
#include "Instance.hxx"
#include "LResolver.hxx"
#include "lib/fmt/SystemError.hxx"
#include "system/SetupProcess.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "lua/PushCClosure.hxx"
#include "lua/Value.hxx"
#include "lua/Util.hxx"
#include "lua/Error.hxx"
#include "lua/RunFile.hxx"
#include "lua/io/XattrTable.hxx"
#include "lua/net/SocketAddress.hxx"
#include "util/PrintException.hxx"

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <systemd/sd-daemon.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // for chdir()

static int systemd_magic = 42;

static bool
IsSystemdMagic(lua_State *L, int idx)
{
	return lua_islightuserdata(L, idx) &&
		lua_touserdata(L, idx) == &systemd_magic;
}

static int
l_passage_listen(lua_State *L)
try {
	auto &instance = *(Instance *)lua_touserdata(L, lua_upvalueindex(1));

	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameter count");

	if (!lua_isfunction(L, 2))
		luaL_argerror(L, 2, "function expected");

	auto handler = std::make_shared<Lua::Value>(L, Lua::StackIndex(2));

	if (IsSystemdMagic(L, 1)) {
		instance.AddSystemdListener(std::move(handler));
	} else if (lua_isstring(L, 1)) {
		const char *address_string = lua_tostring(L, 1);

		AllocatedSocketAddress address;
		address.SetLocal(address_string);

		instance.AddListener(address, std::move(handler));
	} else
		luaL_argerror(L, 1, "path expected");

	return 0;
} catch (...) {
	Lua::RaiseCurrent(L);
}

static void
SetupConfigState(lua_State *L, Instance &instance)
{
	luaL_openlibs(L);

	Lua::InitSocketAddress(L);
	RegisterLuaResolver(L);

	Lua::SetGlobal(L, "systemd", Lua::LightUserData(&systemd_magic));

	Lua::SetGlobal(L, "passage_listen",
		       Lua::MakeCClosure(l_passage_listen,
					 Lua::LightUserData(&instance)));
}

static void
ChdirContainingDirectory(const char *path)
{
	const char *slash = strrchr(path, '/');
	if (slash == nullptr || slash == path)
		return;

	const std::string parent{path, slash};
	if (chdir(parent.c_str()) < 0)
		throw FmtErrno("Failed to change to {}", parent);
}

static void
LoadConfigFile(lua_State *L, const char *path)
{
	ChdirContainingDirectory(path);
	Lua::RunFile(L, path);

	if (chdir("/") < 0)
		throw FmtErrno("Failed to change to {}", "/");
}

static void
SetupRuntimeState(lua_State *L)
{
	Lua::SetGlobal(L, "passage_listen", nullptr);

	Lua::InitXattrTable(L);

	PassageConnection::Register(L);

	UnregisterLuaResolver(L);
}

static int
Run(const CommandLine &cmdline)
{
	Instance instance;
	SetupConfigState(instance.GetLuaState(), instance);

	LoadConfigFile(instance.GetLuaState(), cmdline.config_path.c_str());

	instance.Check();

	SetupRuntimeState(instance.GetLuaState());

	/* tell systemd we're ready */
	sd_notify(0, "READY=1");

	instance.GetEventLoop().Run();

	instance.logger(3, "exiting");
	return EXIT_SUCCESS;
}

int
main(int argc, char **argv) noexcept
try {
	const auto cmdline = ParseCommandLine(argc, argv);

	SetupProcess();

	/* force line buffering so Lua "print" statements are flushed
	   even if stdout is a pipe to systemd-journald */
	setvbuf(stdout, nullptr, _IOLBF, 0);
	setvbuf(stderr, nullptr, _IOLBF, 0);

	return Run(cmdline);
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
