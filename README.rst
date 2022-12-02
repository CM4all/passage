Passage
=======

*Passage* is a daemon which allows jailed/containerized processes to
talk to the host to trigger certain actions.


Building Passage
----------------

You need:

- a C++20 compliant compiler (e.g. gcc or clang)
- `libfmt <https://fmt.dev/>`__
- `LuaJIT <http://luajit.org/>`__
- `systemd <https://www.freedesktop.org/wiki/Software/systemd/>`__
- `Meson 0.47 <http://mesonbuild.com/>`__ and `Ninja <https://ninja-build.org/>`__

Get the source code::

 git clone --recursive https://github.com/CM4all/passage.git

Run ``meson``::

 meson . output

Compile and install::

 ninja -C output
 ninja -C output install


Building the Debian package
---------------------------

After installing the build dependencies, run::

 dpkg-buildpackage -rfakeroot -b -uc -us
