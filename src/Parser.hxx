/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <string>

struct StringView;
struct Entity;

/**
 * Throws on error.
 */
Entity
ParseEntity(StringView s);
