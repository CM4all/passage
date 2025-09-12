// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Verify.hxx"

#include <stdexcept>

void
CheckCommand(std::string_view s)
{
	if (s.empty())
		throw std::runtime_error("Empty command");

	if (!CheckChars(s, IsValidCommandChar))
		throw std::runtime_error{"Malformed command"};
}
