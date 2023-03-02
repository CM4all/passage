// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "FadeChildren.hxx"
#include "net/SocketAddress.hxx"
#include "net/ConnectSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/control/Client.hxx"

void
FadeChildren(SocketAddress address, const char *tag)
{
	BengControlClient client{CreateConnectDatagramSocket(address)};
	client.Send(BengProxy::ControlCommand::FADE_CHILDREN, std::string_view{tag});
}
