/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <string>

/**
 * A request or response.
 */
struct Entity {
	std::string command;
	std::string args;
};
