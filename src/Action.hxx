/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "net/AllocatedSocketAddress.hxx"

#include <string>

struct Action {
	enum class Type {
		UNDEFINED,
		FADE_CHILDREN,
	};

	Type type = Type::UNDEFINED;

	AllocatedSocketAddress address;

	std::string param;

	bool IsDefined() const {
		return type != Type::UNDEFINED;
	}
};
