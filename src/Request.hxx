/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <string>

struct Request {
	std::string command;
	std::string args;
};
