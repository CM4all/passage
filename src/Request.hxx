/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <string>

struct StringView;

class Request {
	std::string command;
	std::string args;

public:
	explicit Request(StringView payload);

	const std::string &GetCommand() const noexcept {
		return command;
	}

	const std::string &GetArgs() const noexcept {
		return args;
	}
};
