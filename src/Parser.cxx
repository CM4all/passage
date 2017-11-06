/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Parser.hxx"
#include "Entity.hxx"
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

Entity
ParseEntity(StringView payload)
{
	Entity entity;

	auto line = NextLine(payload);

	const auto command = NextUnquoted(line);
	CheckCommand(command);
	entity.command.assign(command.data, command.size);

	if (!line.empty())
		// TODO: tokenize and unquote the arguments
		entity.args.assign(line.data, line.size);

	return entity;
}
