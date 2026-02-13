// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"
#include "config.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <forward_list>

enum class HttpMethod : uint_least8_t;

enum class StderrOption : uint_least8_t {
	JOURNAL,
	PIPE,
};

struct Action {
	enum class Type : uint_least8_t {
		UNDEFINED,
		ERROR,

		// deprecated: use control_client.build().fade_children()
		FADE_CHILDREN,

		// deprecated: use control_client.build().flush_http_cache()
		FLUSH_HTTP_CACHE,

		EXEC_PIPE,
#ifdef HAVE_CURL
		HTTP_REQUEST,
#endif
	};

	std::map<std::string, std::string, std::less<>> response_headers;

	AllocatedSocketAddress address;

	std::string param;

	std::forward_list<std::string> args;

#ifdef HAVE_CURL
	std::map<std::string, std::string, std::less<>> request_headers;
	std::optional<std::string> body;

	HttpMethod http_method;
#endif

	Type type = Type::UNDEFINED;

	StderrOption stderr = StderrOption::JOURNAL;

	bool cgroup_client = false;

	constexpr bool IsDefined() const noexcept {
		return type != Type::UNDEFINED;
	}
};
