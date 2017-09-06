/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef LISTENER_HXX
#define LISTENER_HXX

#include "Connection.hxx"
#include "net/TemplateServerSocket.hxx"
#include "lua/ValuePtr.hxx"

typedef TemplateServerSocket<PassageConnection,
                             Lua::ValuePtr,
                             RootLogger, EventLoop &> PassageListener;

#endif
