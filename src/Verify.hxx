// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/CharUtil.hxx"

#include <string_view>

static constexpr bool
IsValidCommandChar(char ch)
{
	return IsAlphaNumericASCII(ch) || ch == '_';
}

void
CheckCommand(std::string_view s);
