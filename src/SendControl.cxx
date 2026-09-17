// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "SendControl.hxx"
#include "net/SocketAddress.hxx"
#include "net/ConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/control/Client.hxx"

void
FadeChildren(SocketAddress address, std::string_view tag)
{
	using namespace BengControl;
	Client client{CreateConnectDatagramSocket(address)};
	client.Send(Command::FADE_CHILDREN, tag);
}

void
FlushHttpCache(SocketAddress address, std::string_view tag)
{
	using namespace BengControl;
	Client client{CreateConnectDatagramSocket(address)};
	client.Send(Command::FLUSH_HTTP_CACHE, tag);
}
