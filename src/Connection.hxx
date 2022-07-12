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

#ifndef CONNECTION_HXX
#define CONNECTION_HXX

#include "lua/ValuePtr.hxx"
#include "event/net/UdpListener.hxx"
#include "event/net/UdpHandler.hxx"
#include "io/Logger.hxx"
#include "util/IntrusiveList.hxx"

#include <string_view>

#include <sys/socket.h>

struct Action;
class Instance;
class UniqueSocketDescriptor;
class FileDescriptor;

class PassageConnection final
	: public AutoUnlinkIntrusiveListHook,
	UdpHandler {

	Instance &instance;

	Lua::ValuePtr handler;

	const struct ucred peer_cred;
	ChildLogger logger;

	UdpListener listener;

	bool pending_response = false;

public:
	PassageConnection(Instance &_instance,
			  Lua::ValuePtr _handler,
			  const RootLogger &parent_logger,
			  UniqueSocketDescriptor &&_fd, SocketAddress address);

	static void Register(lua_State *L);

private:
	void Do(SocketAddress address, const Action &action);

	void SendResponse(SocketAddress address, std::string_view status);
	void SendResponse(SocketAddress address, std::string_view status,
			  FileDescriptor fd);

	/* virtual methods from class UdpHandler */
	bool OnUdpDatagram(ConstBuffer<void> payload,
			   WritableBuffer<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr ep) noexcept override;
};

#endif
