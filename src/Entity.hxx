/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <string>
#include <forward_list>

/**
 * A request or response.
 */
struct Entity {
	std::string command;
	std::forward_list<std::string> args;
};
