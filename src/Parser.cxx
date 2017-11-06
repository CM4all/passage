/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Parser.hxx"
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

static StringView
NextUnquoted(StringView &buffer) noexcept
{
	return NextSplit(buffer, ' ');
}

Request
ParseRequest(StringView payload)
{
	Request request;

	auto line = NextLine(payload);

	const auto command = NextUnquoted(line);
	CheckCommand(command);
	request.command.assign(command.data, command.size);

	if (!line.empty())
		// TODO: tokenize and unquote the arguments
		request.args.assign(line.data, line.size);

	return request;
}
