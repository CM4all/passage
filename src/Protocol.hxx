/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "util/StringView.hxx"
#include "util/CharUtil.hxx"

#include <stdexcept>

static constexpr bool
IsValidCommandChar(char ch)
{
	return IsAlphaNumericASCII(ch) || ch == '_';
}

static void
CheckCommand(StringView s)
{
	if (s.empty())
		throw std::runtime_error("Empty command");

	for (char ch : s)
		if (!IsValidCommandChar(ch))
			throw std::runtime_error("Malformed command");
}
