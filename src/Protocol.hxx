// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/CharUtil.hxx"

#include <stdexcept>
#include <string_view>

static constexpr bool
IsValidCommandChar(char ch)
{
	return IsAlphaNumericASCII(ch) || ch == '_';
}

static void
CheckCommand(std::string_view s)
{
	if (s.empty())
		throw std::runtime_error("Empty command");

	for (char ch : s)
		if (!IsValidCommandChar(ch))
			throw std::runtime_error("Malformed command");
}
