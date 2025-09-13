// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "lua/AutoCloseList.hxx"
#include "lua/CoRunner.hxx"
#include "lua/Resume.hxx"
#include "lua/ValuePtr.hxx"
#include "event/net/UdpListener.hxx"
#include "event/net/UdpHandler.hxx"
#include "net/linux/PeerAuth.hxx"
#include "io/Logger.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "co/InvokeTask.hxx"
#include "util/IntrusiveList.hxx"

#include <cstdint>
#include <string_view>

struct Action;
struct Entity;
class Instance;
class UniqueSocketDescriptor;
class FileDescriptor;

class PassageConnection final
	: public AutoUnlinkIntrusiveListHook,
	  Lua::ResumeListener,
	  UdpHandler
{
	Instance &instance;

	const Lua::ValuePtr handler;

	const SocketPeerAuth peer_auth;

	ChildLogger logger;

	Lua::AutoCloseList auto_close;

	UdpListener listener;

	/**
	 * The Lua thread which runs the handler coroutine.
	 */
	Lua::CoRunner thread;

	Co::InvokeTask invoke_task;

	bool pending_response = false;

public:
	PassageConnection(Instance &_instance,
			  Lua::ValuePtr _handler,
			  const RootLogger &parent_logger,
			  UniqueSocketDescriptor &&_fd, SocketAddress address);

	~PassageConnection() noexcept;

	static void Register(lua_State *L);

private:
	Co::InvokeTask Do(SocketAddress address, const Action &action);

	void SendResponse(SocketAddress address, std::string_view status);
	void SendResponse(SocketAddress address, std::string_view status,
			  FileDescriptor fd,
			  FileDescriptor fd2=FileDescriptor::Undefined());
	void SendResponse(SocketAddress address, const Entity &response);
	void SendError(SocketAddress address, const Action &action);

	void OnCoComplete(std::exception_ptr error) noexcept;

	/* virtual methods from class UdpHandler */
	bool OnUdpDatagram(std::span<const std::byte> payload,
			   std::span<UniqueFileDescriptor> fds,
			   SocketAddress address, int uid) override;
	bool OnUdpHangup() override;
	void OnUdpError(std::exception_ptr &&error) noexcept override;

	/* virtual methods from class Lua::ResumeListener */
	void OnLuaFinished(lua_State *L) noexcept override;
	void OnLuaError(lua_State *L, std::exception_ptr e) noexcept override;
};
