// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Parser.hxx"
#include "Entity.hxx"
#include "Protocol.hxx"
#include "util/StringSplit.hxx"
#include "util/StringStrip.hxx"

#include <stdexcept>

static std::string_view
NextSplit(std::string_view &buffer, char separator) noexcept
{
	auto [line, rest] = Split(buffer, separator);
	buffer = rest;
	return line;
}

static std::string_view
NextLine(std::string_view &buffer) noexcept
{
	return NextSplit(buffer, '\n');
}

static std::string_view
NextUnquoted(std::string_view &buffer) noexcept
{
	return NextSplit(buffer, ' ');
}

Entity
ParseEntity(std::string_view payload)
{
	Entity entity;

	const auto [_payload, body] = Split(payload, '\0');
	if (!body.empty())
		entity.body = std::string{body};
	payload = _payload;

	auto line = NextLine(payload);

	const auto command = NextUnquoted(line);
	CheckCommand(command);
	entity.command = command;

	auto args_tail = entity.args.before_begin();
	while (true) {
		line = StripLeft(line);
		if (line.empty())
			break;

		// TODO unquote
		auto value = NextUnquoted(line);
		args_tail = entity.args.emplace_after(args_tail, value);
	}

	while (!(line = NextLine(payload)).empty()) {
		auto [name, value] = Split(line, ':');
		if (value.data() == nullptr || name.empty())
			throw std::runtime_error("Bad header syntax");

		value = StripLeft(value);

		entity.headers.emplace(name, value);
	}

	return entity;
}
