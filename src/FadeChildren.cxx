/*
 * Copyright 2007-2020 CM4all GmbH
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

#include "FadeChildren.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SendMessage.hxx"
#include "io/Iovec.hxx"
#include "system/Error.hxx"
#include "util/ByteOrder.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"

#include <beng-proxy/Control.hxx>

using namespace BengProxy;

static void
SendControl(SocketDescriptor fd, SocketAddress address,
	    ControlCommand cmd, ConstBuffer<void> payload)
{
	uint32_t magic = ToBE32(control_magic);
	ControlHeader header{
		.length = ToBE16(payload.size),
		.command = ToBE16(uint16_t(cmd)),
	};

	const size_t padding = (0 - payload.size) & 0x3;

	const struct iovec v[] = {
		MakeIovecT(magic),
		MakeIovecT(header),
		MakeIovec(payload),
		{ &magic, padding },
	};

	MessageHeader msg =
		ConstBuffer<struct iovec>(v, 2u + 2u * !payload.IsNull());
	msg.SetAddress(address);

	auto nbytes = SendMessage(fd, msg, MSG_DONTWAIT|MSG_NOSIGNAL);
	if (size_t(nbytes) != sizeof(magic) + sizeof(header) + payload.size + padding)
		throw std::runtime_error("Short control send");
}

static void
SendControl(SocketAddress address, ControlCommand cmd,
	    ConstBuffer<void> payload)
{
	UniqueSocketDescriptor fd;
	if (!fd.Create(address.GetFamily(), SOCK_DGRAM, 0))
		throw MakeErrno("Failed to create control socket");

	SendControl(fd, address, cmd, payload);
}

void
FadeChildren(SocketAddress address, const char *tag)
{
	SendControl(address, ControlCommand::FADE_CHILDREN,
		    StringView(tag).ToVoid());
}
