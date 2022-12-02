/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "Instance.hxx"
#include "Parser.hxx"
#include "Entity.hxx"
#include "LRequest.hxx"
#include "LAction.hxx"
#include "Action.hxx"
#include "FadeChildren.hxx"
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

#include <fmt/format.h>

static std::string
MakeLoggerDomain(const struct ucred &cred, SocketAddress)
{
	if (cred.pid < 0)
		return "connection";

	return fmt::format("pid={} uid={}", cred.pid, cred.uid);
}

PassageConnection::PassageConnection(Instance &_instance,
				     Lua::ValuePtr _handler,
				     const RootLogger &parent_logger,
				     UniqueSocketDescriptor &&_fd,
				     SocketAddress address)
	:instance(_instance), handler(std::move(_handler)),
	 peer_cred(_fd.GetPeerCredentials()),
	 logger(parent_logger, MakeLoggerDomain(peer_cred, address).c_str()),
	 listener(instance.GetEventLoop(), std::move(_fd), *this) {}

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

	NewLuaRequest(L, std::move(request), peer_cred);
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

void
PassageConnection::OnUdpError(std::exception_ptr ep) noexcept
{
	logger(1, ep);
	delete this;
}
