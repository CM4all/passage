/*
 * Copyright 2017-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Parser.hxx"
#include "Entity.hxx"
#include "Protocol.hxx"
#include "util/StringSplit.hxx"
#include "util/StringStrip.hxx"
#include "util/StringView.hxx"

#include <stdexcept>

static StringView
NextSplit(StringView &buffer, char separator) noexcept
{
	auto [line, rest] = Split(std::string_view{buffer}, separator);
	buffer = rest;
	return line;
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

	auto args_tail = entity.args.before_begin();
	while (true) {
		line.StripLeft();
		if (line.empty())
			break;

		// TODO unquote
		auto value = NextUnquoted(line);
		args_tail = entity.args.emplace_after(args_tail,
						      value.data, value.size);
	}

	while (!(line = NextLine(payload)).empty()) {
		auto [name, value] = Split(std::string_view{line}, ':');
		if (value.data() == nullptr || name.empty())
			throw std::runtime_error("Bad header syntax");

		value = StripLeft(value);

		entity.headers.emplace(name, value);
	}

	return entity;
}
