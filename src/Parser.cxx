// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Parser.hxx"
#include "Entity.hxx"
#include "Verify.hxx"
#include "util/StringSplit.hxx"
#include "util/StringStrip.hxx"

#include <cassert>
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

static std::string
NextQuoted(std::string_view &_src)
{
	assert(!_src.empty());
	assert(_src.front() == '"');

	const auto src = _src;
	std::string dest;

	for (std::size_t i = 1; i < src.size(); ++i) {
		char ch = src[i];

		if (ch == '"') {
			if (const auto rest = src.substr(i + 1);
			    rest.empty())
				_src = rest;
			else if (rest.front() == ' ')
				_src = rest.substr(1);
			else
				throw std::runtime_error{"Garbage after closing quote"};
			return dest;
		}

		if (ch == '\\') {
			if (++i >= src.size())
				break;

			ch = src[i];
		}

		dest.push_back(ch);
	}

	throw std::runtime_error{"Closing quote missing"};
}

static std::string
NextValue(std::string_view &src)
{
	assert(!src.empty());

	if (src.front() == '"')
		return NextQuoted(src);
	else
		return std::string{NextUnquoted(src)};
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

		args_tail = entity.args.emplace_after(args_tail, NextValue(line));
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
