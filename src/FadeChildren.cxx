/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "FadeChildren.hxx"
#include "net/SocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"
#include "util/ByteOrder.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"

#include <beng-proxy/control.h>

static void
SendControl(SocketDescriptor fd, SocketAddress address,
	    enum beng_control_command cmd, ConstBuffer<void> payload)
{
	uint32_t magic = ToBE32(control_magic);
	struct beng_control_header header = {
		.length = ToBE16(payload.size),
		.command = ToBE16(uint16_t(cmd)),
	};

	const size_t padding = (0 - payload.size) & 0x3;

	struct iovec v[] = {
		{ &magic, sizeof(magic) },
		{ &header, sizeof(header) },
		{ const_cast<void *>(payload.data), payload.size },
		{ &magic, padding },
	};

	struct msghdr msg = {
		.msg_name = const_cast<struct sockaddr *>(address.GetAddress()),
		.msg_namelen = address.GetSize(),
		.msg_iov = v,
		.msg_iovlen = 2u + 2u * !payload.IsNull(),
		.msg_control = nullptr,
		.msg_controllen = 0,
		.msg_flags = 0,
	};

	auto nbytes = sendmsg(fd.Get(), &msg, MSG_DONTWAIT|MSG_NOSIGNAL);
	if (nbytes < 0)
		throw MakeErrno("Failed to send control packet");

	if (size_t(nbytes) != sizeof(magic) + sizeof(header) + payload.size + padding)
		throw std::runtime_error("Short control send");
}

static void
SendControl(SocketAddress address, enum beng_control_command cmd,
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
	SendControl(address, CONTROL_FADE_CHILDREN, StringView(tag).ToVoid());
}
