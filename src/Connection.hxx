/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef CONNECTION_HXX
#define CONNECTION_HXX

#include "lua/ValuePtr.hxx"
#include "net/UdpListener.hxx"
#include "net/UdpHandler.hxx"
#include "io/Logger.hxx"
#include "util/ConstBuffer.hxx"

#include <boost/intrusive/list.hpp>

struct Action;
class EventLoop;
class UniqueSocketDescriptor;

class PassageConnection final
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>,
	UdpHandler {

	Lua::ValuePtr handler;

	const struct ucred peer_cred;
	ChildLogger logger;

	UdpListener listener;

public:
	PassageConnection(Lua::ValuePtr _handler,
			  const RootLogger &parent_logger,
			  EventLoop &event_loop,
			  UniqueSocketDescriptor &&_fd, SocketAddress address);

	static void Register(lua_State *L);

private:
	/* virtual methods from class UdpHandler */
	void OnUdpDatagram(const void *data, size_t length,
			   SocketAddress address, int uid) override;
	void OnUdpError(std::exception_ptr ep) override;
};

#endif
