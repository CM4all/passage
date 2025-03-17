// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"
#include "config.h"

#include <string>
#include <forward_list>

struct Action {
	enum class Type {
		UNDEFINED,
		FADE_CHILDREN,
		FLUSH_HTTP_CACHE,
		EXEC_PIPE,
#ifdef HAVE_CURL
		HTTP_GET,
#endif
	};

	Type type = Type::UNDEFINED;

	AllocatedSocketAddress address;

	std::string param;

	std::forward_list<std::string> args;

	bool IsDefined() const {
		return type != Type::UNDEFINED;
	}
};
