/*
 * author: Max Kellermann <mk@cm4all.com>
 */

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
	std::map<std::string, std::string> headers;

	std::string Serialize() const noexcept;
};
