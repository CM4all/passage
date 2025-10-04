Passage
=======

Author: Max Kellermann <max.kellermann@ionos.com>

*Passage* is a daemon which allows jailed/containerized processes to
talk to the host to trigger certain actions.


Configuration
-------------

.. highlight:: lua

The file :file:`/etc/cm4all/passage/config.lua` is a `Lua
<http://www.lua.org/>`_ script which is executed at startup.  It
contains at least one :samp:`passage_listen()` call, for example::

  passage_listen('/run/cm4all/passage/passage.sock', function(request)
    return m:connect('192.168.1.99')
  end)

The first parameter is the socket path to listen on.  Passing the
global variable :envvar:`systemd` (not the string literal
:samp:`"systemd"`) will listen on the sockets passed by systemd (from
unit :file:`cm4all-passage.socket`)::

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
    if request.command == 'restart' then
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


``SIGHUP``
^^^^^^^^^^

On ``systemctl reload cm4all-passage`` (i.e. ``SIGHUP``), Passage
calls the Lua function ``reload`` if one was defined.  It is up to the
Lua script to define the exact meaning of this feature.


Inspecting Incoming Requests
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following attributes can be queried:

* :samp:`command`: The command string.

* :samp:`args`: An array containing command arguments.

* :samp:`headers`: A table containing headers (name/value pairs).

* :samp:`body`: An optional string of (binary) data; may be ``nil`` if
  none was specified.

* :samp:`pid`: The client's process id.

* :samp:`uid`: The client's user id.

* :samp:`gid`: The client's group id.

* :samp:`cgroup`: The control group of the client process with the
  following attributes:

  * ``path``: the cgroup path as noted in :file:`/proc/self/cgroup`,
    e.g. :file:`/user.slice/user-1000.slice/session-42.scope`

  * ``xattr``: A table containing extended attributes of the
    control group.

  * ``parent``: Information about the parent of this cgroup; it is
    another object of this type (or ``nil`` if there is no parent
    cgroup).


Actions
^^^^^^^

The handler function shall return an object describing what to do with
the request.  The request object contains several methods which create
such action objects; they do not actually perform the action.

The following actions are possible:

* :samp:`fade_children(ADDRESS, TAG)`: send a :samp:`FADE_CHILDREN`
  control packet to the given address.  The address is either a string
  containing a (numeric) IP address, or an `address` object created by
  :samp:`control_resolve()`.  If a tag is specified, then only
  children with this tag are addressed.

* :samp:`flush_http_cache(ADDRESS, TAG)`: send a :samp:`FLUSH_HTTP_CACHE`
  control packet to the given address.  The address is either a string
  containing a (numeric) IP address, or an `address` object created by
  :samp:`control_resolve()`.  The tag selects the cache items which
  shall be flushed.

* :samp:`exec_pipe({PATH, ARG, ...}, [{OPTIONS}])`: execute the given
  program (should be an absolute path because there is no
  :envvar:`$PATH` resolution here) and connect a pipe to its standard
  output; send the pipe's reading side to the client.

  The second parameter may be a table specifying options:

  - ``stderr='pipe'``: Connect the program's ``stderr`` to a pipe and
    return the read side to the client.

* :samp:`http_request(URL)`: perform a HTTP request and send the
  response to the Passage client.  Non-successful HTTP responses
  (anything other than 2xx) cause the operation to fail.  (This works
  only with small HTTP responses because Passage responses are limited
  to one datagram.)

  Instead of a simple URL string, you can construct more complex
  requests by passing a table::

    return request:http_request({
      url='http://example.com/',
    })

  The following table keys are recognized:

  - ``url``: an absolute HTTP URL
  - ``query``: a table containing names and values of query parameters
  - ``headers``: a table containing custom request headers

* :samp:`error([MESSAGE], [HEADERS])`: send an error response to the
  client.  Takes an optional error message parameter.  If a message is
  provided (and not ``nil``), it will be included in the error
  response. This allows the handler to explicitly return an error
  status without raising a Lua exception.

  The second parameter allows setting response headers.  For example,
  the Passage client uses the "exit_status" header as its process exit
  status.

  Example::

    return request:error('Something went wrong', {exit_status='75'})

Returning without an action from the handler function (i.e. returning
:samp:`nil`) is considered a silent success.

If you encounter a problem, raise an exception by invoking the Lua
function :samp:`error()`.  The message passed to this function will be
logged.


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
  (which blocks passage startup; not recommended)
- convert a path string to a "local" socket address
- convert a name to an abstract "local" socket address (prefix '@' is
  converted to a null byte, making the address "abstract")


libsodium
^^^^^^^^^

There are some `libsodium <https://www.libsodium.org/>`__ bindings.

`Helpers <https://doc.libsodium.org/helpers>`__::

  bin = sodium.hex2bin("deadbeef") -- returns "\xde\xad\xbe\ef"
  hex = sodium.bin2hex("A\0\xff") -- returns "4100ff"

`Generating random data
<https://doc.libsodium.org/generating_random_data>`__::

  key = sodium.randombytes(32)

`Sealed boxes
<https://libsodium.gitbook.io/doc/public-key_cryptography/sealed_boxes>`__::

  pk, sk = sodium.crypto_box_keypair()
  ciphertext = sodium.crypto_box_seal('hello world', pk)
  message = sodium.crypto_box_seal_open(ciphertext, pk, sk)

`Point*scalar multiplication
<https://doc.libsodium.org/advanced/scalar_multiplication>__::

  pk = sodium.crypto_scalarmult_base(sk)


PostgreSQL Client
^^^^^^^^^^^^^^^^^

The Lua script can query a PostgreSQL database.  First, a connection
should be established during initialization::

  db = pg:new('dbname=foo', 'schemaname')

In the handler function, queries can be executed like this (the API is
similar to `LuaSQL <https://keplerproject.github.io/luasql/>`__)::

  local result = assert(db:execute('SELECT id, name FROM bar'))
  local row = result:fetch({}, "a")
  print(row.id, row.name)

Query parameters are passed to ``db:execute()`` as an array after the
SQL string::

  local result = assert(
    db:execute('SELECT name FROM bar WHERE id=$1', {42}))

The functions ``pg:encode_array()`` and ``pg:decode_array()`` support
PostgreSQL arrays; the former encodes a Lua array to a PostgreSQL
array string, and the latter decodes a PostgreSQL array string to a
Lua array.

To listen for `PostgreSQL notifications
<https://www.postgresql.org/docs/current/sql-notify.html>`__, invoke
the ``listen`` method with a callback function::

  db:listen('bar', function()
    print("Received a PostgreSQL NOTIFY")
  end)


Security
^^^^^^^^

This software and the Lua code used to configure it is very sensitive,
because untrusted processes can send arbitrary data to it.

Never trust the information from the packet payload.

Do not try to establish an authentication protocol.  If you want to
know who the client is, query those attributes which cannot be changed
by the client, such as cgroup membership and file system mounts.
Consider that the client may be able to create a new mount namespace
and change all mounts.  If you have doubts about the client's
identity, bail out (e.g. with Lua's :samp:`error()` function).


About Lua
^^^^^^^^^

`Programming in Lua <https://www.lua.org/pil/1.html>`_ (a tutorial
book), `Lua 5.3 Reference Manual <https://www.lua.org/manual/5.3/>`_.

Note that in Lua, attributes are referenced with a dot
(e.g. :samp:`m.sender`), but methods are referenced with a colon
(e.g. :samp:`m:reject()`).


Usage
-----

.. highlight:: shell

The Debian package :file:`cm4all-passage-client` contains a very
simple and generic client.  The first parameter specifies the command,
and positional argument strings can be specified after that.
Example::

  cm4all-passage-client fade_children

The option :envvar:`--header=NAME:VALUE` can be used to send headers
to the server.

By default, the client connects to :file:`/run/cm4all/passage/socket`,
but the option :envvar:`--server=PATH` can be used to change that::

  cm4all-passage-client --server=/tmp/passage.socket fade_children

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
  \0BODY

A packet consists of at least one command (request) or status
(response).  The command is an unquoted string consisting of ASCII
letters, digits or underscore.  The response status can be either
:samp:`OK` or :samp:`ERROR` (unquoted).  An error status may be
followed by a message as the first (and only) parameter.

After the command, there may be positional string parameters separated
by a space.  An unquoted parameter is a non-empty string of ASCII
letters, digits, underscores or dashes.  Parameters that contain
other characters must be enclosed in double quotes.  The double quote
and backslash may be escaped by preceding it with a backslash
character.  Control characters and null bytes (0x00..0x1f) are not
allowed.

Following lines may be headers, i.e. name-value pairs.  The last
newline character may be omitted.

Finally, a body of binary data may be appended, separated from the
rest with a null byte.  Ancillary data may contain file descriptors.

The meaning of commands, parameters, headers, body and the file
descriptors is defined by the Lua configuration script.


Common Commands
^^^^^^^^^^^^^^^

This section describes common commands, to establish a convention on
how they shall be implemented.

* :samp:`fade_children`: send a :samp:`FADE_CHILDREN` control
  packet to a configured address.  The Lua script shall determine the
  client's identity and should only fade child processes belonging to
  that user account.
