// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Instance.hxx"
#include "Parser.hxx"
#include "Entity.hxx"
#include "LRequest.hxx"
#include "LAction.hxx"
#include "Action.hxx"
#include "SendControl.hxx"
#include "ExecPipe.hxx"
#include "lua/Error.hxx"
#include "io/Iovec.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "net/SocketAddress.hxx"
#include "net/SocketProtocolError.hxx"
#include "net/ScmRightsBuilder.hxx"
#include "net/SendMessage.hxx"
#include "util/StaticVector.hxx"
#include "util/ScopeExit.hxx"
#include "util/SpanCast.hxx"
#include "util/Macros.hxx"
#include "util/Compiler.h"

#ifdef HAVE_CURL
#include "lib/curl/CoRequest.hxx"
#include "lib/curl/Easy.hxx"
#include "lib/curl/Setup.hxx"
#include "co/Task.hxx"
#endif

#include <fmt/format.h>

#include <utility> // for std::unreachable()

using std::string_view_literals::operator""sv;

static std::string
MakeLoggerDomain(const SocketPeerAuth &auth, SocketAddress)
{
	if (!auth.HaveCred())
		return "connection";

	return fmt::format("pid={} uid={}", auth.GetPid(), auth.GetUid());
}

PassageConnection::PassageConnection(Instance &_instance,
				     Lua::ValuePtr _handler,
				     const RootLogger &parent_logger,
				     UniqueSocketDescriptor &&_fd,
				     SocketAddress address)
	:instance(_instance), handler(std::move(_handler)),
	 peer_auth(_fd),
	 logger(parent_logger, MakeLoggerDomain(peer_auth, address).c_str()),
	 auto_close(handler->GetState()),
	 listener(instance.GetEventLoop(), std::move(_fd), *this),
	 thread(handler->GetState())
{
}

PassageConnection::~PassageConnection() noexcept
{
	thread.Cancel();
}

void
PassageConnection::Register(lua_State *L)
{
	RegisterLuaAction(L);
	RegisterLuaRequest(L);
}

void
PassageConnection::SendResponse(SocketAddress address, std::string_view status)
{
	assert(pending_response);

	pending_response = false;
	listener.Reply(address, AsBytes(status));
}

void
PassageConnection::SendResponse(SocketAddress address, std::string_view status,
				FileDescriptor fd, FileDescriptor fd2)
{
	assert(pending_response);
	assert(fd.IsDefined());

	pending_response = false;

	const struct iovec vec[] = {
		MakeIovec(AsBytes(status)),
	};

	MessageHeader m{vec};
	m.SetAddress(address);

	ScmRightsBuilder<2> rb(m);
	rb.push_back(fd.Get());
	if (fd2.IsDefined())
		rb.push_back(fd2.Get());
	rb.Finish(m);

	SendMessage(listener.GetSocket(), m, MSG_DONTWAIT|MSG_NOSIGNAL);
}

void
PassageConnection::SendResponse(SocketAddress address, const Entity &response)
{
	SendResponse(address, response.Serialize());
}

inline void
PassageConnection::SendError(SocketAddress address, const Action &action)
{
	Entity response{
		.command = std::string{"ERROR"sv},
	};

	if (!action.param.empty())
		response.args.emplace_front(action.param);

	SendResponse(address, response);
}

#ifdef HAVE_CURL

static CurlEasy
ActionToCurlEasy(const Action &action)
{
	CurlEasy easy{action.param.c_str()};
	Curl::Setup(easy);
	easy.SetFailOnError();
	return easy;
}

static Co::Task<Entity>
HttpGet(CurlGlobal &curl, const Action &action)
{
	// TODO body size limit
	auto response = co_await Curl::CoRequest(curl, ActionToCurlEasy(action));

	Entity entity{
		.command = std::string{"OK"sv},
		.body = std::move(response.body),
	};

	co_return entity;
}

#endif // HAVE_CURL

Co::InvokeTask
PassageConnection::Do(SocketAddress address, const Action &action)
{
	assert(pending_response);

	switch (action.type) {
	case Action::Type::UNDEFINED:
		std::unreachable();

	case Action::Type::ERROR:
		SendError(address, action);
		break;

	case Action::Type::FADE_CHILDREN:
		FadeChildren(action.address,
			     action.param.empty() ? nullptr : action.param.c_str());
		break;

	case Action::Type::FLUSH_HTTP_CACHE:
		FlushHttpCache(action.address, action.param.c_str());
		break;

	case Action::Type::EXEC_PIPE:
		{
			StaticVector<const char *, 64> args;
			for (const auto &i : action.args) {
				args.emplace_back(i.c_str());
				if (args.full())
					throw std::runtime_error("Too many EXEC_PIPE arguments");
			}

			args.emplace_back(nullptr);

			auto result = ExecPipe(args.front(), &args.front(), action.stderr);

			SendResponse(address, "OK", result.stdout_pipe, result.stderr_pipe);
		}

		break;

#ifdef HAVE_CURL
	case Action::Type::HTTP_GET:
		SendResponse(address,
			     (co_await HttpGet(instance.GetCurl(), action)).Serialize());
		break;
#endif // HAVE_CURL
	}

	co_return;
}

bool
PassageConnection::OnUdpDatagram(std::span<const std::byte> payload,
				 std::span<UniqueFileDescriptor>,
				 SocketAddress address, int)
try {
	if (payload.empty()) {
		delete this;
		return false;
	}

	if (pending_response)
		throw SocketProtocolError{"Received another datagram while handling request"};

	assert(!invoke_task);

	pending_response = true;

	auto request = ParseEntity(ToStringView(payload));

	/* create a new thread for the handler coroutine */
	const auto L = thread.CreateThread(*this);

	handler->Push(L);

	NewLuaRequest(L, auto_close,
		      std::move(request), peer_auth);

	Lua::Resume(L, 1);

	return true;
} catch (...) {
	if (pending_response)
		SendResponse(address, "ERROR");

	logger(1, std::current_exception());
	delete this;
	return false;
}

bool
PassageConnection::OnUdpHangup()
{
	delete this;
	return false;
}

void
PassageConnection::OnUdpError(std::exception_ptr &&error) noexcept
{
	logger(1, std::move(error));
	delete this;
}

void
PassageConnection::OnLuaFinished(lua_State *L) noexcept
try {
	assert(!invoke_task);

	const Lua::ScopeCheckStack check_thread_stack(L);

	if (!lua_isnil(L, -1)) {
		auto *action = CheckLuaAction(L, -1);
		if (action == nullptr)
			throw std::runtime_error("Wrong return type from Lua handler");

		invoke_task = Do(nullptr, *action);
		invoke_task.Start(BIND_THIS_METHOD(OnCoComplete));
	} else if (pending_response)
		SendResponse(nullptr, "OK");
} catch (...) {
	OnLuaError(L, std::current_exception());
}

void
PassageConnection::OnLuaError(lua_State *, std::exception_ptr e) noexcept
{
	assert(!invoke_task);

	logger(1, e);

	if (pending_response)
		SendResponse(nullptr, "ERROR");
}

inline void
PassageConnection::OnCoComplete(std::exception_ptr error) noexcept
{
	if (error) {
		logger(1, std::move(error));

		if (pending_response)
			SendResponse(nullptr, "ERROR");
	} else {
		if (pending_response)
			SendResponse(nullptr, "OK");
	}
}
