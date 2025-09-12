// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/CharUtil.hxx"
#include "util/StringVerify.hxx"

#include <string_view>

constexpr bool
IsValidCommandChar(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) || ch == '_';
}

constexpr bool
IsValidUnquotedParameterChar(char ch) noexcept
{
	return IsAlphaNumericASCII(ch) || ch == '_' || ch == '-';
}

constexpr bool
IsValidUnquotedParameter(std::string_view s) noexcept
{
	return CheckCharsNonEmpty(s, IsValidUnquotedParameterChar);
}

void
CheckCommand(std::string_view s);
