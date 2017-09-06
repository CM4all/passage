/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include "net/AllocatedSocketAddress.hxx"

struct Action {
	enum class Type {
		UNDEFINED,
	};

	Type type = Type::UNDEFINED;

	bool IsDefined() const {
		return type != Type::UNDEFINED;
	}
};
