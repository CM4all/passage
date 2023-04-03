// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string>
#include <forward_list>
#include <map>

/**
 * A request or response.
 */
struct Entity {
	std::string command;
	std::forward_list<std::string> args;
	std::map<std::string, std::string, std::less<>> headers;

	std::string Serialize() const noexcept;
};
