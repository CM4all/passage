/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <string>

struct StringView;

class Request {
	std::string command;

public:
	explicit Request(StringView payload);

	const std::string &GetCommand() const noexcept {
		return command;
	}
};
