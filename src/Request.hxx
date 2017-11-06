/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <string>

struct StringView;

struct Request {
	std::string command;
	std::string args;

	explicit Request(StringView payload);
};
