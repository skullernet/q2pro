Prerequisities
--------------

Q2PRO can be built on Linux, BSD and similar platfroms using a recent version
of GCC or Clang.

Q2PRO client requires either SDL2 or OpenAL for sound output. For video output,
native X11 and Wayland backends are available, as well as generic SDL2 backend.

Note that SDL2 is optional if using native X11 and Wayland backends and OpenAL,
which is preferred configuration.

Both client and dedicated server require zlib support for full compatibility at
network protocol level. The rest of dependencies are optional.

For JPEG support libjpeg-turbo is required, plain libjpeg will not work. Most
Linux distributions already provide libjpeg-turbo in place of libjpeg.

For playing back cinematics in Ogg Theora format and music in Ogg Vorbis format
FFmpeg libraries are required.

To install the *full* set of dependencies for building Q2PRO on Debian or
Ubuntu use the following command:

    apt-get install meson gcc libc6-dev libsdl2-dev libopenal-dev \
                    libpng-dev libjpeg-dev zlib1g-dev mesa-common-dev \
                    libcurl4-gnutls-dev libx11-dev libxi-dev \
                    libwayland-dev wayland-protocols libdecor-0-dev \
                    libavcodec-dev libavformat-dev libavutil-dev \
                    libswresample-dev libswscale-dev

Users of other distributions should look for equivalent development packages
and install them.


Building
--------

Q2PRO uses Meson build system for its build process.

Setup build directory (arbitrary name can be used instead of `builddir`):

    meson setup builddir

Review and configure options:

    meson configure builddir

Q2PRO specific options are listed in `Project options` section. They are
defined in `meson_options.txt` file.

E.g. to install to different prefix:

    meson configure -Dprefix=/usr builddir

Finally, invoke build command:

    meson compile -C builddir

To enable verbose output during the build, use `meson compile -C builddir -v`.


Installation
------------

You need to have either full version of Quake 2 unpacked somewhere, or a demo.
Both should be patched to 3.20 point release.

Run `sudo ninja -C builddir install` to install Q2PRO system-wide into
configured prefix (`/usr/local` by default).

Copy `baseq2/pak*.pak` files and `baseq2/players` directory from unpacked
Quake 2 data into `/usr/local/share/q2pro/baseq2` to complete the
installation.

Alternatively, configure with `-Dsystem-wide=false` to build a ‘portable’
version that expects to be launched from the root of Quake 2 data tree (this
is default when building for Windows).

On Windows, Q2PRO automatically sets current directory to the directory Q2PRO
executable is in. On other platforms current directory must be set before
launching Q2PRO executable if portable version is built.


Music support
-------------

Q2PRO supports playback of background music ripped off original CD in Ogg
Vorbis format. Music files should be placed in `music` subdirectory of the game
directory in format `music/trackNN.ogg`, where `NN` corresponds to CD track
number. `NN` should be typically in range 02-11 (track 01 is data track on
original CD and should never be used).

Note that so-called ‘GOG’ naming convention where music tracks are named
‘Track01’ to ‘Track21’ is not supported.


MinGW-w64
---------

MinGW-w64 cross-compiler is available in recent versions of all major Linux
distributions.

Library dependencies that Q2PRO uses have been prepared as Meson subprojects
and will be automatically downloaded and built by Meson.

To install MinGW-w64 on Debian or Ubuntu, use the following command:

    apt-get install mingw-w64

It is recommended to also install nasm, which is needed to build libjpeg-turbo
with SIMD support:

    apt-get install nasm

Meson needs correct cross build definition file for compilation. Example
cross-files can be found in `.ci` subdirectory (available in git
repository, but not source tarball).

Setup build directory:

    meson setup --cross-file .ci/x86_64-w64-mingw32.txt -Dwrap_mode=forcefallback builddir

Build:

    meson compile -C builddir


Visual Studio
-------------

It is possible to build Q2PRO on Windows using Visual Studio 2022 and Meson.

Install Visual Studio and Meson using official installers.

Optionally, download and install nasm executable. The easiest way to add it
into PATH is to put it into `Program Files/Meson`.

The build needs to be launched from appropriate Visual Studio command line
shell, e.g. `x64 Native Tools Command Prompt`.

Change to Q2PRO source directory, then setup build directory:

    meson setup -Dwrap_mode=forcefallback builddir

Build:

    meson compile -C builddir
