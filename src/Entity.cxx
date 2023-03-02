// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Entity.hxx"

std::string
Entity::Serialize() const noexcept
{
	std::string result = command;

	for (const auto &i : args) {
		// TODO: quote argument value
		result.push_back(' ');
		result.append(i);
	}

	result.push_back('\n');

	for (const auto &i : headers) {
		result.append(i.first);
		result.push_back(':');
		result.append(i.second);
		result.push_back('\n');
	}

	result.push_back('\n');

	return result;
}
