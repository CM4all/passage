// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "lua/AutoCloseList.hxx"
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

	Lua::AutoCloseList auto_close;

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
	bool OnUdpDatagram(std::span<const std::byte> payload,
			   std::span<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr ep) noexcept override;
};
