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

#include "Instance.hxx"
#include "Parser.hxx"
#include "Entity.hxx"
#include "LRequest.hxx"
#include "LAction.hxx"
#include "Action.hxx"
#include "FadeChildren.hxx"
#include "ExecPipe.hxx"
#include "lua/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "net/SocketAddress.hxx"
#include "net/ScmRightsBuilder.hxx"
#include "util/StaticArray.hxx"
#include "util/StringView.hxx"
#include "util/ScopeExit.hxx"
#include "util/Macros.hxx"

static std::string
MakeLoggerDomain(const struct ucred &cred, SocketAddress)
{
	if (cred.pid < 0)
		return "connection";

	char buffer[128];
	snprintf(buffer, sizeof(buffer), "pid=%d uid=%d",
		 int(cred.pid), int(cred.uid));
	return buffer;
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
PassageConnection::SendResponse(SocketAddress address, StringView status)
{
	assert(pending_response);

	pending_response = false;
	listener.Reply(address, status.data, status.size);
}

void
PassageConnection::SendResponse(SocketAddress address, StringView status,
				FileDescriptor fd)
{
	assert(pending_response);

	pending_response = false;

	const auto payload = status.ToVoid();

	struct iovec vec[] = {
		{
			.iov_base = const_cast<void *>(payload.data),
			.iov_len = payload.size,
		},
	};

	struct msghdr m = {
		.msg_name = const_cast<void *>(static_cast<const void *>(address.GetAddress())),
		.msg_namelen = address.GetSize(),
		.msg_iov = vec,
		.msg_iovlen = ARRAY_SIZE(vec),
		.msg_control = nullptr,
		.msg_controllen = 0,
		.msg_flags = 0,
	};

	ScmRightsBuilder<1> rb(m);
	rb.push_back(fd.Get());
	rb.Finish(m);

	sendmsg(listener.GetSocket().Get(), &m, MSG_DONTWAIT|MSG_NOSIGNAL);
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
			StaticArray<const char *, 64> args;
			for (const auto &i : action.args)
				if (!args.checked_append(i.c_str()))
					throw std::runtime_error("Too many EXEC_PIPE arguments");
			if (!args.checked_append(nullptr))
				throw std::runtime_error("Too many EXEC_PIPE arguments");

			SendResponse(address, "OK", ExecPipe(instance.GetChildProcessRegistry(),
							     args.front(),
							     &args.front()));
		}

		break;
	}
}

bool
PassageConnection::OnUdpDatagram(const void *data, size_t length,
				 SocketAddress address, int)
try {
	assert(!pending_response);

	if (length == 0) {
		delete this;
		return false;
	}

	pending_response = true;

	auto request = ParseEntity(StringView((const char *)data, length));

	handler->Push();

	const auto L = handler->GetState();
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
