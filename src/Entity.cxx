// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Entity.hxx"
#include "Verify.hxx"

static void
AppendQuotedParameter(std::string &dest, std::string_view value) noexcept
{
	dest.push_back('"');

	for (char ch : value) {
		if (ch == '\\' || ch == '"')
			dest.push_back('\\');
		else if (IsWhitespaceOrNull(ch))
			/* TODO these characters are not legal, we
			   should probably better throw an
			   exception */
			ch = ' ';

		dest.push_back(ch);
	}

	dest.push_back('"');
}

static void
AppendParameter(std::string &dest, std::string_view value) noexcept
{
	if (IsValidUnquotedParameter(value))
		dest.append(value);
	else
		AppendQuotedParameter(dest, value);
}

std::string
Entity::Serialize() const noexcept
{
	std::string result = command;

	for (const auto &i : args) {
		// TODO: quote argument value
		result.push_back(' ');
		AppendParameter(result, i);
	}

	if (!headers.empty())
		result.push_back('\n');

	for (const auto &i : headers) {
		result.append(i.first);
		result.push_back(':');
		result.append(i.second);
		result.push_back('\n');
	}

	if (!body.empty()) {
		result.push_back('\0');
		result.append(body);
	}

	return result;
}
