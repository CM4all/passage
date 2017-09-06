/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Instance.hxx"
#include "Request.hxx"
#include "LRequest.hxx"
#include "lua/Error.hxx"
#include "net/SocketAddress.hxx"
#include "util/PrintException.hxx"
#include "util/StringView.hxx"
#include "util/ScopeExit.hxx"

static std::string
MakeLoggerDomain(const struct ucred &cred, SocketAddress)
{
	if (cred.pid < 0)
		return "connection";

	char buffer[128];
	snprintf(buffer, sizeof(buffer), "pid=%d uid=%d",
		 int(cred.pid), int(cred.uid));
	return buffer;
}

PassageConnection::PassageConnection(Lua::ValuePtr _handler,
				     const RootLogger &parent_logger,
				     EventLoop &event_loop,
				     UniqueSocketDescriptor &&_fd,
				     SocketAddress address)
	:handler(std::move(_handler)),
	 peer_cred(_fd.GetPeerCredentials()),
	 logger(parent_logger, MakeLoggerDomain(peer_cred, address).c_str()),
	 listener(event_loop, std::move(_fd), *this) {}

void
PassageConnection::Register(lua_State *L)
{
    RegisterLuaRequest(L);
}

void
PassageConnection::OnUdpDatagram(const void *data, size_t length,
				 SocketAddress, int)
try {
	if (length == 0) {
		delete this;
		return;
	}

	Request request(StringView((const char *)data, length));

	handler->Push();

	const auto L = handler->GetState();
	NewLuaRequest(L, std::move(request), peer_cred);
	if (lua_pcall(L, 1, 1, 0))
		throw Lua::PopError(L);

	AtScopeExit(L) { lua_pop(L, 1); };

} catch (...) {
	logger(1, std::current_exception());
	delete this;
}

void
PassageConnection::OnUdpError(std::exception_ptr ep)
{
	PrintException(ep);
	delete this;
}
