// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

class SocketAddress;

void
FadeChildren(SocketAddress address, const char *tag);

void
FlushHttpCache(SocketAddress address, const char *tag);
