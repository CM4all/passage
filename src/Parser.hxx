// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string_view>

struct Entity;

/**
 * Throws on error.
 */
Entity
ParseEntity(std::string_view s);
