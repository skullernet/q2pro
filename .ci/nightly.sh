#!/bin/sh -ex

MESON_OPTS_COMMON="--auto-features=enabled --fatal-meson-warnings \
    -Dwerror=true -Dwrap_mode=forcefallback"

MESON_OPTS="$MESON_OPTS_COMMON \
    -Dgame-build-options=optimization=s,b_lto=true \
    -Dsdl2=disabled -Dwayland=disabled -Dx11=disabled"

SRC_DIR=`pwd`
CI=$SRC_DIR/.ci

TMP_DIR=$SRC_DIR/q2pro-build
mkdir $TMP_DIR

export MESON_PACKAGE_CACHE_DIR=$SRC_DIR/subprojects/packagecache

### Source ###

REV=$(git rev-list --count HEAD)
SHA=$(git rev-parse --short HEAD)
VER="r$REV~$SHA"
SRC="q2pro-r$REV"

cd $TMP_DIR
GIT_DIR=$SRC_DIR/.git git archive --format=tar --prefix=$SRC/ HEAD | tar x
echo "$VER" > $SRC/VERSION
rm -rf $SRC/.gitignore $SRC/.ci $SRC/.github
fakeroot tar czf q2pro-source.tar.gz $SRC

sed -e "s/##VER##/$VER/" -e "s/##DATE##/`date -R`/" $CI/readme-template.txt    > README
sed -e "s/##VER##/$VER/" -e "s/##DATE##/`date -R`/" $CI/readme-template-rr.txt > README.rr

### FFmpeg ###

cd $TMP_DIR
git clone --depth=1 https://github.com/FFmpeg/FFmpeg.git ffmpeg
cd ffmpeg

mkdir build-mingw-32
cd build-mingw-32
$CI/configure-ffmpeg.sh --win32 $TMP_DIR/ffmpeg-prefix-32
make -j4 install
cd ..

mkdir build-mingw-64
cd build-mingw-64
$CI/configure-ffmpeg.sh --win64 $TMP_DIR/ffmpeg-prefix-64
make -j4 install
cd ..

### Win32 ###

export PKG_CONFIG_SYSROOT_DIR="$TMP_DIR/ffmpeg-prefix-32"
export PKG_CONFIG_LIBDIR="$PKG_CONFIG_SYSROOT_DIR/lib/pkgconfig"

cd $TMP_DIR
meson setup --cross-file $CI/i686-w64-mingw32.txt $MESON_OPTS build-mingw-32 $SRC
cd build-mingw-32
ninja
i686-w64-mingw32-strip q2pro.exe q2proded.exe gamex86.dll

unix2dos -k -n ../$SRC/LICENSE LICENSE.txt ../$SRC/doc/client.asciidoc MANUAL.txt ../README README.txt
mkdir baseq2
cp -a ../$SRC/src/client/ui/q2pro.menu baseq2/
mv gamex86.dll baseq2/

zip -9 ../q2pro-client_win32_x86.zip \
    q2pro.exe \
    LICENSE.txt \
    MANUAL.txt \
    README.txt \
    baseq2/q2pro.menu \
    baseq2/gamex86.dll

unix2dos -k -n ../$SRC/doc/server.asciidoc MANUAL.txt
zip -9 ../q2pro-server_win32_x86.zip \
    q2proded.exe \
    LICENSE.txt \
    MANUAL.txt \
    README.txt

### Win64 ###

export PKG_CONFIG_SYSROOT_DIR="$TMP_DIR/ffmpeg-prefix-64"
export PKG_CONFIG_LIBDIR="$PKG_CONFIG_SYSROOT_DIR/lib/pkgconfig"

cd $TMP_DIR
meson setup --cross-file $CI/x86_64-w64-mingw32.txt $MESON_OPTS build-mingw-64 $SRC
cd build-mingw-64
ninja
x86_64-w64-mingw32-strip q2pro.exe q2proded.exe gamex86_64.dll

unix2dos -k -n ../$SRC/LICENSE LICENSE.txt ../$SRC/doc/client.asciidoc MANUAL.txt ../README README.txt
mkdir baseq2
cp -a ../$SRC/src/client/ui/q2pro.menu baseq2/
mv gamex86_64.dll baseq2/
mv q2pro.exe q2pro64.exe
mv q2proded.exe q2proded64.exe

zip -9 ../q2pro-client_win64_x64.zip \
    q2pro64.exe \
    LICENSE.txt \
    MANUAL.txt \
    README.txt \
    baseq2/q2pro.menu \
    baseq2/gamex86_64.dll

unix2dos -k -n ../$SRC/doc/server.asciidoc MANUAL.txt
zip -9 ../q2pro-server_win64_x64.zip \
    q2proded64.exe \
    LICENSE.txt \
    MANUAL.txt \
    README.txt

### Win64-rerelease ###

cd $TMP_DIR
git clone https://github.com/skullernet/q2pro-rerelease-dll.git
cd q2pro-rerelease-dll
meson setup --cross-file $CI/x86_64-w64-mingw32.txt $MESON_OPTS_COMMON build-mingw
ninja -C build-mingw
x86_64-w64-mingw32-strip build-mingw/gamex86_64.dll
cd etc
zip -9 ../build-mingw/q2pro.pkz default.cfg q2pro.menu

cd $TMP_DIR/build-mingw-64

mv q2pro64.exe q2pro.exe
cp -a ../q2pro-rerelease-dll/build-mingw/q2pro.pkz baseq2/
cp -a ../q2pro-rerelease-dll/build-mingw/gamex86_64.dll baseq2/
unix2dos -k -n ../$SRC/doc/client.asciidoc MANUAL.txt ../README.rr README.txt

zip -9 ../q2pro-rerelease-client_win64_x64.zip \
    q2pro.exe \
    LICENSE.txt \
    MANUAL.txt \
    README.txt \
    baseq2/q2pro.pkz \
    baseq2/gamex86_64.dll

### Version ###

cd $TMP_DIR
echo $VER > version.txt
