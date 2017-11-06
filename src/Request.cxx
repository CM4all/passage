/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Request.hxx"
#include "Protocol.hxx"
#include "util/StringView.hxx"

static StringView
NextSplit(StringView &buffer, char separator) noexcept
{
	StringView result = buffer;
	const char *newline = buffer.Find(separator);
	if (newline == nullptr)
		buffer = nullptr;
	else
		buffer.MoveFront(newline + 1);

	return result;
}

static StringView
NextLine(StringView &buffer) noexcept
{
	return NextSplit(buffer, '\n');
}

Request::Request(StringView payload)
{
	auto line = NextLine(payload);

	const auto _command = line;
	CheckCommand(_command);
	command.assign(_command.data, _command.size);
}
