/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "CommandLine.hxx"
#include "Instance.hxx"
#include "system/SetupProcess.hxx"
#include "net/AllocatedSocketAddress.hxx"
#include "lua/Value.hxx"
#include "lua/Util.hxx"
#include "lua/RunFile.hxx"
#include "util/PrintException.hxx"

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <systemd/sd-daemon.h>

#include <stdlib.h>

static int systemd_magic = 42;

static bool
IsSystemdMagic(lua_State *L, int idx)
{
	return lua_islightuserdata(L, idx) &&
		lua_touserdata(L, idx) == &systemd_magic;
}

static int
l_passage_listen(lua_State *L)
{
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
}

static void
SetupConfigState(lua_State *L, Instance &instance)
{
	luaL_openlibs(L);

	Lua::SetGlobal(L, "systemd", Lua::LightUserData(&systemd_magic));

	Lua::SetGlobal(L, "passage_listen",
		       Lua::MakeCClosure(l_passage_listen,
					 Lua::LightUserData(&instance)));
}

static void
SetupRuntimeState(lua_State *L)
{
    Lua::SetGlobal(L, "passage_listen", nullptr);

    PassageConnection::Register(L);
}

static int
Run(const CommandLine &cmdline)
{
	Instance instance;
	SetupConfigState(instance.GetLuaState(), instance);

	Lua::RunFile(instance.GetLuaState(), cmdline.config_path.c_str());

	instance.Check();

	SetupRuntimeState(instance.GetLuaState());

	/* tell systemd we're ready */
	sd_notify(0, "READY=1");

	instance.GetEventLoop().Dispatch();

	instance.logger(3, "exiting");
	return EXIT_SUCCESS;
}

int
main(int argc, char **argv)
try {
	const auto cmdline = ParseCommandLine(argc, argv);

	SetupProcess();

	return Run(cmdline);
} catch (const std::exception &e) {
	PrintException(e);
	return EXIT_FAILURE;
}
