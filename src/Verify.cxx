// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Verify.hxx"
#include "net/SocketProtocolError.hxx"

void
CheckCommand(std::string_view s)
{
	if (s.empty())
		throw SocketProtocolError{"Empty command"};

	if (!CheckChars(s, IsValidCommandChar))
		throw SocketProtocolError{"Malformed command"};
}
