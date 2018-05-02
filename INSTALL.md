Prerequisities
--------------

Q2PRO can be built on Linux, BSD and similar platfroms using a recent version
of GCC or Clang.

Cross-compiling Q2PRO for Windows is also possible, see the MinGW-w64 section
below.

Q2PRO client requires libSDL 2 for video and sound output. Both client and
dedicated server require zlib support for full compatibility at network
protocol level. The rest of dependencies are optional.

To install the *full* set of dependencies for building Q2PRO on Debian or
Ubuntu use the following command:

    apt-get install build-essential libsdl2-dev libopenal-dev \
                    libpng-dev libjpeg-dev zlib1g-dev mesa-common-dev \
                    liblircclient-dev libcurl4-gnutls-dev

Users of other distributions should look for equivalent development packages
and install them.


Building
--------

Q2PRO uses a simple build system consisting of a single top-level Makefile and
a build-time configuration file. Configuration file is optional; if there is no
one, Q2PRO will be built with minimal subset of dependencies, but some features
will be unavailable.

Copy an example configuration file from `doc/examples/buildconfig` to `.config`
and modify it to suit your needs. Enable needed features by uncommenting them.
There is no autodetection of installed dependencies and by default *nothing*
optional is enabled.

Type `make` to build a client, dedicated server and baseq2 game library. Type
`make strip` to strip off debugging symbols from resulting executables. Type
`make clean` to remove all generated executables, object files and
dependencies.

To enable verbose output during the build, set the V variable, e.g. `make V=1`.


Installation
------------

You need to have either full version of Quake 2 unpacked somewhere, or a demo.
Both should be patched to 3.20 point release.

Run the following commands to do a per-user installation of Q2PRO into your
home directory.

    mkdir -p ~/.q2pro/baseq2
    cp -a /path/to/quake2/baseq2/pak*.pak ~/.q2pro/baseq2/
    cp -a /path/to/quake2/baseq2/players ~/.q2pro/baseq2/
    cp -a src/client/ui/q2pro.menu ~/.q2pro/baseq2/
    cp -a game*.so ~/.q2pro/baseq2/
    cp -a q2pro ~/.q2pro/

Then change directory to `~/.q2pro` and run `./q2pro` from there.

Alternatively, you can do a system-wide installation (see documentation for
`CONFIG_PATH_*` variables in `doc/examples/buildconfig`).


MinGW-w64
---------

MinGW-w64 cross-compiler is available in recent versions of all major Linux
distributions. A Q2PRO SDK for MinGW-w64 has been prepared that includes all
additional libraries Q2PRO uses in precompiled form. It also contains scripts
for rebuilding those libraries from source, should you ever need that.

To install MinGW-w64 on Debian or Ubuntu, use the following command:

    apt-get install mingw-w64

Download the latest pre-built SDK archive from here:

    https://github.com/skullernet/q2pro-mgw-sdk/releases

Extract it alongside Q2PRO source directory and copy `config` (or `config64`)
file to `.config`, then change to Q2PRO source directory and type `make` or `make
strip`.
