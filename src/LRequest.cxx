// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "LRequest.hxx"
#include "Entity.hxx"
#include "LAction.hxx"
#include "Action.hxx"
#include "lua/AutoCloseList.hxx"
#include "lua/CheckArg.hxx"
#include "lua/Class.hxx"
#include "lua/Error.hxx"
#include "lua/FenvCache.hxx"
#include "lua/ForEach.hxx"
#include "lua/StringView.hxx"
#include "lua/io/CgroupInfo.hxx"
#include "lua/net/SocketAddress.hxx"
#include "net/control/Protocol.hxx"
#include "net/linux/PeerAuth.hxx"
#include "io/Beneath.hxx"
#include "io/FileAt.hxx"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"

#ifdef HAVE_CURL
#include "uri/Escape.hxx"
#include "http/HeaderName.hxx"
#include "util/AllocatedString.hxx"
#endif

#include <assert.h>
#include <sys/socket.h>
#include <string.h>

using std::string_view_literals::operator""sv;

class RichRequest : public Entity {
	Lua::AutoCloseList *auto_close;

	const SocketPeerAuth &peer_auth;

public:
	RichRequest(lua_State *L, Lua::AutoCloseList &_auto_close,
		    Entity &&src, const SocketPeerAuth &_peer_auth)
		:Entity(std::move(src)),
		 auto_close(&_auto_close),
		 peer_auth(_peer_auth)
	{
		auto_close->Add(L, Lua::RelativeStackIndex{-1});

		lua_newtable(L);
		lua_setfenv(L, -2);
	}

	bool IsStale() const noexcept {
		return auto_close == nullptr;
	}

	int Close(lua_State *) {
		auto_close = nullptr;
		return 0;
	}

	int Index(lua_State *L);
};

static constexpr char lua_request_class[] = "passage.request";
typedef Lua::Class<RichRequest, lua_request_class> LuaRequest;

static void
LuaTableToStringMap(std::map<std::string, std::string, std::less<>> &dest,
		    lua_State *L, int table_idx)
{
	Lua::ForEach(L, table_idx, [L, &dest](auto key_idx, auto value_idx){
		if (!lua_isstring(L, Lua::GetStackIndex(key_idx)))
			throw std::invalid_argument{"Key is not a string"};

		if (!lua_isstring(L, Lua::GetStackIndex(value_idx)))
			throw std::invalid_argument{"Value is not a string"};

		dest.emplace(Lua::ToStringView(L, Lua::GetStackIndex(key_idx)),
			     Lua::ToStringView(L, Lua::GetStackIndex(value_idx)));
	});
}

static int
NewErrorAction(lua_State *L)
{
	const auto top = lua_gettop(L);
	if (top < 1 || top > 3)
		return luaL_error(L, "Invalid parameters");

	auto &action = *NewLuaAction(L, 1);
	action.type = Action::Type::ERROR;
	if (top >= 2 && !lua_isnil(L, 2)) {
		action.param = Lua::CheckStringView(L, 2);
	}

	if (top >= 3) {
		luaL_checktype(L, 3, LUA_TTABLE);
		LuaTableToStringMap(action.response_headers, L, 3);
	}

	return 1;
}

static int
NewFadeChildrenAction(lua_State *L)
{
	const auto top = lua_gettop(L);
	if (top < 2 || top > 3)
		return luaL_error(L, "Invalid parameters");

	AllocatedSocketAddress address;

	try {
		address = Lua::ToSocketAddress(L, 2, BengControl::DEFAULT_PORT);
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
NewFlushHttpCacheAction(lua_State *L)
{
	const auto top = lua_gettop(L);
	if (top != 3)
		return luaL_error(L, "Invalid parameters");

	AllocatedSocketAddress address;

	try {
		address = Lua::ToSocketAddress(L, 2, BengControl::DEFAULT_PORT);
	} catch (const std::exception &e) {
		return luaL_error(L, e.what());
	}

	const char *child_tag = luaL_checkstring(L, 3);

	auto &action = *NewLuaAction(L, 1);
	action.type = Action::Type::FLUSH_HTTP_CACHE;
	action.address = std::move(address);
	action.param = child_tag;
	return 1;
}

static StderrOption
ParseStderrOption(lua_State *L, std::string_view value)
{
	if (value == "journal"sv)
		return StderrOption::JOURNAL;
	else if (value == "pipe"sv)
		return StderrOption::PIPE;
	else {
		luaL_error(L, "Bad 'stderr' option");
		std::unreachable();
	}
}

static StderrOption
ParseStderrOption(lua_State *L, int idx)
{
	if (lua_type(L, idx) == LUA_TSTRING)
		return ParseStderrOption(L, Lua::ToStringView(L, idx));
	else {
		luaL_error(L, "Bad 'stderr' option");
		std::unreachable();
	}
}

/**
 * Collect parameters from the "options" table passed to exec_pipe().
 */
static void
CollectExecOptions(Action &action, lua_State *L, Lua::AnyStackIndex auto idx)
{
	Lua::ForEach(L, idx, [L, &action](auto key_idx, auto value_idx){
		if (lua_type(L, Lua::GetStackIndex(key_idx)) != LUA_TSTRING)
			luaL_error(L, "Option key is not a string");

		const auto key = Lua::ToStringView(L, Lua::GetStackIndex(key_idx));
		if (key == "stderr"sv) {
			action.stderr = ParseStderrOption(L, Lua::GetStackIndex(value_idx));
		} else
			luaL_error(L, "Unknown option");
	});
}

static int
NewExecPipeAction(lua_State *L)
{
	const auto top = lua_gettop(L);
	if (top < 2 || top > 3)
		return luaL_error(L, "Invalid parameters");

	if (!lua_istable(L, 2))
		luaL_argerror(L, 2, "array expected");

	Action action{
		.type = Action::Type::EXEC_PIPE,
	};

	auto tail = action.args.before_begin();

	for (lua_pushnil(L); lua_next(L, 2); lua_pop(L, 1)) {
		if (!lua_isstring(L, -1))
			luaL_error(L, "string expected");

		tail = action.args.emplace_after(tail, lua_tostring(L, -1));
	}

	if (top >= 3)
		CollectExecOptions(action, L, 3);

	NewLuaAction(L, 1, std::move(action));
	return 1;
}

#ifdef HAVE_CURL

static std::string
ParseHttpQuery(lua_State *L, Lua::RelativeStackIndex query_idx)
{
	if (lua_isstring(L, Lua::GetStackIndex(query_idx)))
		return std::string{Lua::ToStringView(L, Lua::GetStackIndex(query_idx))};

	luaL_checktype(L, Lua::GetStackIndex(query_idx), LUA_TTABLE);

	std::string query;

	Lua::ForEach(L, query_idx, [L, &query](const auto key_idx, const auto value_idx){
		if (!lua_isstring(L, Lua::GetStackIndex(key_idx)))
			throw std::invalid_argument{"Key is not a string"};

		if (!query.empty())
			query.push_back('&');

		query.append(UriEscape(Lua::ToStringView(L, Lua::GetStackIndex(key_idx))));

		if (lua_isnil(L, Lua::GetStackIndex(value_idx)))
			return;

		query.push_back('=');

		if (lua_isstring(L, Lua::GetStackIndex(value_idx)))
			query.append(UriEscape(Lua::ToStringView(L, Lua::GetStackIndex(value_idx))));
		else
			throw std::invalid_argument{"Value is not a string"};
	});

	return query;
}

static void
ParseHttpHeaders(std::map<std::string, std::string, std::less<>> &headers,
		 lua_State *L, Lua::RelativeStackIndex query_idx)
{
	luaL_checktype(L, Lua::GetStackIndex(query_idx), LUA_TTABLE);

	Lua::ForEach(L, query_idx, [L, &headers](const auto key_idx, const auto value_idx){
		if (!lua_isstring(L, Lua::GetStackIndex(key_idx)))
			throw std::invalid_argument{"Key is not a string"};

		const std::string_view name{Lua::ToStringView(L, Lua::GetStackIndex(key_idx))};
		if (!http_header_name_valid(name))
			throw std::invalid_argument{"Not a valid HTTP header name"};

		if (!lua_isstring(L, Lua::GetStackIndex(value_idx)))
			throw std::invalid_argument{"Value is not a string"};

		const std::string_view value{Lua::ToStringView(L, Lua::GetStackIndex(value_idx))};
		// TODO check value

		headers.emplace(name, value);
	});
}

static void
ParseHttpRequest(Action &action, lua_State *L, int request_idx)
{
	if (lua_isstring(L, request_idx)) {
		const auto value = Lua::ToStringView(L, request_idx);
		if (value.empty())
			throw std::invalid_argument{"Bad URL"};

		action.param = value;
		return;
	}

	luaL_checktype(L, request_idx, LUA_TTABLE);

	std::string url, query;

	Lua::ForEach(L, request_idx, [L, &action, &url, &query](const auto key_idx, const auto value_idx){
		if (!lua_isstring(L, Lua::GetStackIndex(key_idx)))
			throw std::invalid_argument{"Key is not a string"};

		const auto key = Lua::ToStringView(L, Lua::GetStackIndex(key_idx));
		if (key == "url"sv) {
			if (!lua_isstring(L, Lua::GetStackIndex(value_idx)))
				throw std::invalid_argument{"url is not a string"};

			const auto value = Lua::ToStringView(L, Lua::GetStackIndex(value_idx));
			if (value.empty())
				throw std::invalid_argument{"Bad URL"};

			url = value;
		} else if (key == "query"sv) {
			query = ParseHttpQuery(L, value_idx);
		} else if (key == "headers"sv) {
			ParseHttpHeaders(action.request_headers, L, value_idx);
		} else
			throw std::invalid_argument{"Unrecognized key"};
	});

	if (url.empty())
		throw std::invalid_argument{"No URL"};

	if (!query.empty()) {
		url.push_back(url.find('?') == url.npos ? '?' : '&');
		url.append(query);
	}

	action.param = std::move(url);
}

static int
NewHttpRequestAction(lua_State *L)
try {
	const auto top = lua_gettop(L);
	if (top != 2)
		return luaL_error(L, "Invalid parameters");

	Action action{
		.type = Action::Type::HTTP_REQUEST,
	};

	ParseHttpRequest(action, L, 2),

	NewLuaAction(L, 1, std::move(action));
	return 1;
} catch (...) {
	Lua::RaiseCurrent(L);
}

#endif // HAVE_CURL

static constexpr struct luaL_Reg request_methods [] = {
	{"error", NewErrorAction},
	{"fade_children", NewFadeChildrenAction},
	{"flush_http_cache", NewFlushHttpCacheAction},
	{"exec_pipe", NewExecPipeAction},
#ifdef HAVE_CURL
	{"http_request", NewHttpRequestAction},
	{"http_get", NewHttpRequestAction}, // pre 0.25 legacy
#endif
	{nullptr, nullptr}
};

inline int
RichRequest::Index(lua_State *L)
try {
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

		// copy a reference to the fenv (our cache)
		Lua::SetFenvCache(L, 1, name_idx, Lua::RelativeStackIndex{-1});

		return 1;
	} else if (StringIsEqual(name, "body")) {
		if (body.empty())
			return 0;

		Lua::Push(L, body);
		return 1;
	} else if (StringIsEqual(name, "pid")) {
		if (!peer_auth.HaveCred())
			return 0;

		Lua::Push(L, static_cast<lua_Integer>(peer_auth.GetPid()));
		return 1;
	} else if (StringIsEqual(name, "uid")) {
		if (!peer_auth.HaveCred())
			return 0;

		Lua::Push(L, static_cast<lua_Integer>(peer_auth.GetUid()));
		return 1;
	} else if (StringIsEqual(name, "gid")) {
		if (!peer_auth.HaveCred())
			return 0;

		Lua::Push(L, static_cast<lua_Integer>(peer_auth.GetGid()));
		return 1;
	} else if (StringIsEqual(name, "cgroup")) {
		const auto path = peer_auth.GetCgroupPath();
		if (path.empty())
			return 0;

		Lua::NewCgroupInfo(L, *auto_close, path);

		// copy a reference to the fenv (our cache)
		Lua::SetFenvCache(L, 1, name_idx, Lua::RelativeStackIndex{-1});

		return 1;
	} else
		return luaL_error(L, "Unknown attribute");
} catch (...) {
	Lua::RaiseCurrent(L);
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
	      Entity &&src, const SocketPeerAuth &peer_auth)
{
	return LuaRequest::New(L, L, auto_close,
			       std::move(src), peer_auth);
}

Entity &
CastLuaRequest(lua_State *L, int idx)
{
	return LuaRequest::Cast(L, idx);
}
