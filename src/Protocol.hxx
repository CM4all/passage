/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "util/StringView.hxx"
#include "util/CharUtil.hxx"

#include <stdexcept>

static void
CheckCommand(StringView s)
{
    if (s.IsEmpty())
        throw std::runtime_error("Empty command");

    for (char ch : s)
        if (!IsAlphaNumericASCII(ch))
            throw std::runtime_error("Malformed command");
}
