// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
#include "net/ScmRightsBuilder.hxx"
#include "net/SendMessage.hxx"
#include "util/StaticVector.hxx"
#include "util/ScopeExit.hxx"
#include "util/SpanCast.hxx"
#include "util/Macros.hxx"
#include "util/Compiler.h"

#include <fmt/format.h>

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
	 listener(instance.GetEventLoop(), std::move(_fd), *this)
{
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
				FileDescriptor fd)
{
	assert(pending_response);

	pending_response = false;

	const struct iovec vec[] = {
		MakeIovec(AsBytes(status)),
	};

	MessageHeader m{vec};
	m.SetAddress(address);

	ScmRightsBuilder<1> rb(m);
	rb.push_back(fd.Get());
	rb.Finish(m);

	SendMessage(listener.GetSocket(), m, MSG_DONTWAIT|MSG_NOSIGNAL);
}

void
PassageConnection::Do(SocketAddress address, const Action &action)
{
	assert(pending_response);

	switch (action.type) {
	case Action::Type::UNDEFINED:
		assert(false);
		gcc_unreachable();

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

			SendResponse(address, "OK", ExecPipe(args.front(),
							     &args.front()));
		}

		break;
	}
}

bool
PassageConnection::OnUdpDatagram(std::span<const std::byte> payload,
				 std::span<UniqueFileDescriptor>,
				 SocketAddress address, int)
try {
	assert(!pending_response);

	if (payload.empty()) {
		delete this;
		return false;
	}

	pending_response = true;

	auto request = ParseEntity(ToStringView(payload));

	const auto L = handler->GetState();

	handler->Push(L);

	NewLuaRequest(L, auto_close,
		      std::move(request), peer_auth);
	if (lua_pcall(L, 1, 1, 0))
		throw Lua::PopError(L);

	AtScopeExit(L) { lua_pop(L, 1); };

	auto *action = CheckLuaAction(L, -1);
	if (action == nullptr)
		throw std::runtime_error("Wrong return type from Lua handler");

	Do(address, *action);

	if (pending_response)
		SendResponse(address, "OK");

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
PassageConnection::OnUdpError(std::exception_ptr ep) noexcept
{
	logger(1, ep);
	delete this;
}
