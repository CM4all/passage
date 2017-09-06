Passage
=======

Author: Max Kellermann <mk@cm4all.com>

*Passage* is a daemon which allows jailed/containerized processes to
talk to the host to trigger certain actions.


Configuration
-------------

The file :file:`/etc/cm4all/passage/config.lua` is a `Lua
<http://www.lua.org/>`_ script which is executed at startup.  It
contains at least one :samp:`passage_listen()` call, for example::

  passage_listen('/run/cm4all/passage/passage.sock', function(request)
    return m:connect('192.168.1.99')
  end)

The first parameter is the socket path to listen on.  Passing the
global variable :envvar:`systemd` (not the string literal
:samp:`"systemd"`) will listen on the sockets passed by systemd::

  passage_listen(systemd, function(request) ...

To use this socket from within a container, move it to a dedicated
directory and bind-mount this directory into the container.  Mounting
just the socket doesn't work because a daemon restart must create a
new socket, but the bind mount cannot be refreshed.

The second parameter is a callback function which shall decide what to
do with an incoming request.  This function receives a request object
which can be inspected.  Multiple listeners can share the same handler
by declaring the function explicitly::

  function handler(request)
    if request.command == 'restart'
      return request:fade_children(control_address)
    else
      error("Unknown command")
    end
  end

  passage_listen('/foo', handler)
  passage_listen('/bar', handler)

It is important that the function finishes quickly.  It must never
block, because this would block the whole daemon process.  This means
it must not do any network I/O, launch child processes, and should
avoid anything but querying the request's parameters.


Inspecting Incoming Requests
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following attributes can be queried:

* :samp:`command`: The command string.

* :samp:`pid`: The client's process id.

* :samp:`uid`: The client's user id.

* :samp:`gid`: The client's group id.

The following methods can access more data:

* :samp:`get_cgroup('CONTROLLERNAME')`: Obtain the cgroup membership
  path for the given controller.

* :samp:`get_mount_info('MOUNTPOINT')`: Obtain information about a
  mount point in the client's filesystem namespace.  The return value
  is :samp:`nil` if the given path is not a mount point, or a table
  containing the items :envvar:`root` (root of the mount within the
  filesystem), :envvar:`filesystem` (the name of the filesystem) and
  :envvar:`source` (the device which is mounted here)


Actions
^^^^^^^

The handler function shall return an object describing what to do with
the request.  The request object contains several methods which create
such action objects; they do not actually perform the action.

The following actions are possible:

* :samp:`fade_children(ADDRESS)`: send a :samp:`FADE_CHILDREN` control
  packet to the given address.  The address is either a string
  containing a (numeric) IP address, or an `address` object created by
  :samp:`control_resolve()`.


Addresses
^^^^^^^^^

It is recommended to create all `address` objects during startup, to
avoid putting unnecessary pressure on the Lua garbage collector, and
to reduce the overhead for invoking the system resolver (which blocks
*Passage* execution).  The function `control_resolve()` creates such an
`address` object::

  server1 = control_resolve('192.168.0.2')
  server2 = control_resolve('[::1]:4321')
  server3 = control_resolve('server1.local:1234')
  server4 = control_resolve('/run/server5.sock')
  server5 = control_resolve('@server4')

These examples do the following:

- convert a numeric IPv4 address to an `address` object (port defaults
  to 5478, the *beng-proxy* control standard port)
- convert a numeric IPv6 address with a non-standard port to an
  `address` object
- invoke the system resolver to resolve a host name to an IP address
  (which blocks qrelay startup; not recommended)
- convert a path string to a "local" socket address
- convert a name to an abstract "local" socket address (prefix '@' is
  converted to a null byte, making the address "abstract")


About Lua
^^^^^^^^^

`Programming in Lua <https://www.lua.org/pil/1.html>`_ (a tutorial
book), `Lua 5.3 Reference Manual <https://www.lua.org/manual/5.3/>`_.

Note that in Lua, attributes are referenced with a dot
(e.g. :samp:`m.sender`), but methods are referenced with a colon
(e.g. :samp:`m:reject()`).


Usage
-----

The Debian package :file:`cm4all-passage-client` contains a very
simple and generic client.  There are two parameters: the socket path
to connect to and the command.  Example::

  cm4all-passage-client /run/cm4all/passage/socket restart


Protocol
--------

The daemon listens on a local "sequential packet" socket
(:envvar:`AF_LOCAL` / :envvar:`SOCK_SEQPACKET`).

The client sends a request in one packet, and each packet gets
acknowledged by the server in a response packet.  Both request and
response share the same general structure::

  COMMAND/STATUS [PARAM1 PARAM2 ...]\n
  HEADER1: VALUE1\n
  HEADER2: VALUE2\n
  \0BINARY

A packet consists of at least one command (request) or status
(response).  There may be positional string parameters, and named
headers.  The last newline character may be omitted.  Finally, binary
data may be appended, separated from the rest with a null byte.
Ancillary data may contain file descriptors.

The meaning of commands, parameters, headers, binary data and the file
descriptors is defined by the Lua configuration script.
