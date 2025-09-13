// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"
#include "config.h"

#include <cstdint>
#include <string>
#include <forward_list>

enum class StderrOption : uint_least8_t {
	JOURNAL,
	PIPE,
};

struct Action {
	enum class Type : uint_least8_t {
		UNDEFINED,
		ERROR,
		FADE_CHILDREN,
		FLUSH_HTTP_CACHE,
		EXEC_PIPE,
#ifdef HAVE_CURL
		HTTP_GET,
#endif
	};

	AllocatedSocketAddress address;

	std::string param;

	std::forward_list<std::string> args;

	Type type = Type::UNDEFINED;

	StderrOption stderr = StderrOption::JOURNAL;

	constexpr bool IsDefined() const noexcept {
		return type != Type::UNDEFINED;
	}
};
