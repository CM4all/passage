/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Request.hxx"
#include "util/StringView.hxx"
#include "util/CharUtil.hxx"
#include "util/Compiler.h"

#include <stdexcept>

gcc_pure
static bool
IsAlphaNumericASCII(StringView s) noexcept
{
	for (char ch : s)
		if (!IsAlphaNumericASCII(ch))
			return false;

	return true;
}

Request::Request(StringView payload)
{
	if (payload.IsEmpty() || !IsAlphaNumericASCII(payload))
		throw std::runtime_error("Malformed command");

	command.assign(payload.data, payload.size);
}
