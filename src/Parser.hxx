/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#pragma once

#include <string>

struct StringView;
class Request;

/**
 * Throws on error.
 */
Request
ParseRequest(StringView s);
