// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef LISTENER_HXX
#define LISTENER_HXX

#include "Connection.hxx"
#include "event/net/TemplateServerSocket.hxx"
#include "lua/ValuePtr.hxx"

typedef TemplateServerSocket<PassageConnection,
			     Instance &, Lua::ValuePtr,
			     RootLogger> PassageListener;

#endif
