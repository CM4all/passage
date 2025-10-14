// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "CommandLine.hxx"
#include "Instance.hxx"
#include "LResolver.hxx"
#include "lib/fmt/SystemError.hxx"
#include "system/SetupProcess.hxx"
#include "net/LocalSocketAddress.hxx"
#include "lua/PushCClosure.hxx"
#include "lua/Value.hxx"
#include "lua/Util.hxx"
#include "lua/Error.hxx"
#include "lua/LightUserData.hxx"
#include "lua/RunFile.hxx"
#include "lua/StringView.hxx"
#include "lua/io/XattrTable.hxx"
#include "lua/io/CgroupInfo.hxx"
#include "lua/net/SocketAddress.hxx"
#include "lua/event/Init.hxx"
#include "util/PrintException.hxx"
#include "config.h"

#ifdef HAVE_PG
#include "lua/pg/Init.hxx"
#endif

#ifdef HAVE_LIBSODIUM
#include "lua/sodium/Init.hxx"
#endif

#ifdef HAVE_CURL
#include "lib/curl/Init.hxx"
#endif

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h> // for EX_*
#include <unistd.h> // for chdir()

#ifdef HAVE_LIBSYSTEMD

static int systemd_magic = 42;

static bool
IsSystemdMagic(lua_State *L, int idx)
{
	return lua_islightuserdata(L, idx) &&
		lua_touserdata(L, idx) == &systemd_magic;
}

#endif // HAVE_LIBSYSTEMD

static int
l_passage_listen(lua_State *L)
try {
	auto &instance = *(Instance *)lua_touserdata(L, lua_upvalueindex(1));

	if (lua_gettop(L) != 2)
		return luaL_error(L, "Invalid parameter count");

	if (!lua_isfunction(L, 2))
		luaL_argerror(L, 2, "function expected");

	auto handler = std::make_shared<Lua::Value>(L, Lua::StackIndex(2));

	if (lua_isstring(L, 1)) {
		const auto address_string = Lua::ToStringView(L, 1);

		instance.AddListener(LocalSocketAddress{address_string}, std::move(handler));
#ifdef HAVE_LIBSYSTEMD
	} else if (IsSystemdMagic(L, 1)) {
		instance.AddSystemdListener(std::move(handler));
#endif
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
	Lua::InitResume(L);

#ifdef HAVE_LIBSODIUM
	Lua::InitSodium(L);
#endif

	Lua::InitEvent(L, instance.GetEventLoop());

#ifdef HAVE_PG
	Lua::InitPg(L, instance.GetEventLoop());
#endif

	Lua::InitSocketAddress(L);
	RegisterLuaResolver(L);

#ifdef HAVE_LIBSYSTEMD
	Lua::SetGlobal(L, "systemd", Lua::LightUserData(&systemd_magic));
#endif

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
	Lua::RegisterCgroupInfo(L);

	PassageConnection::Register(L);

	UnregisterLuaResolver(L);
}

static int
Run(const CommandLine &cmdline)
{
#ifdef HAVE_CURL
	const ScopeCurlInit curl_init;
#endif

	Instance instance;

	try {
		SetupConfigState(instance.GetLuaState(), instance);

		LoadConfigFile(instance.GetLuaState(), cmdline.config_path.c_str());

		instance.Check();

		SetupRuntimeState(instance.GetLuaState());
	} catch (...) {
		PrintException(std::current_exception());
		return EX_CONFIG;
	}

#ifdef HAVE_LIBSYSTEMD
	/* tell systemd we're ready */
	sd_notify(0, "READY=1");
#endif

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
