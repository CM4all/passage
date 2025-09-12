// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/CharUtil.hxx"
#include "util/StringVerify.hxx"

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

	if (!CheckChars(s, IsValidCommandChar))
		throw std::runtime_error{"Malformed command"};
}
