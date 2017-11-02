/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "net/AllocatedSocketAddress.hxx"

#include <string>
#include <forward_list>

struct Action {
	enum class Type {
		UNDEFINED,
		FADE_CHILDREN,
		EXEC_PIPE,
	};

	Type type = Type::UNDEFINED;

	AllocatedSocketAddress address;

	std::string param;

	std::forward_list<std::string> args;

	bool IsDefined() const {
		return type != Type::UNDEFINED;
	}
};
