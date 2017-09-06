/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Request.hxx"
#include "Protocol.hxx"
#include "util/StringView.hxx"
#include "util/CharUtil.hxx"
#include "util/Compiler.h"

#include <stdexcept>

Request::Request(StringView payload)
{
	CheckCommand(payload);

	command.assign(payload.data, payload.size);
}
