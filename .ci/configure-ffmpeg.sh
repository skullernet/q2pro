#!/bin/sh -e

OPTS_COMMON="--disable-everything \
    --enable-decoder=theora \
    --enable-decoder=vorbis \
    --enable-decoder=idcin \
    --enable-decoder=pcm_* \
    --disable-decoder=pcm_bluray \
    --disable-decoder=pcm_dvd \
    --disable-decoder=pcm_alaw_at \
    --disable-decoder=pcm_mulaw_at \
    --enable-demuxer=ogg \
    --enable-demuxer=idcin \
    --enable-demuxer=wav \
    --enable-parser=vp3 \
    --enable-parser=vorbis \
    --disable-protocols \
    --enable-protocol=file \
    --disable-avdevice \
    --disable-avfilter \
    --disable-postproc \
    --disable-programs \
    --disable-autodetect \
    --disable-network \
    --disable-doc \
    --disable-swscale-alpha \
    --enable-small \
    --disable-pthreads \
    --disable-w32threads"

config_linux() {
    ../configure --prefix="$1" $OPTS_COMMON
}

config_win32() {
    ../configure \
        --prefix="$1" \
        --cross-prefix=i686-w64-mingw32- \
        --arch=x86 \
        --target-os=mingw32 \
        --extra-cflags='-msse2 -mfpmath=sse' \
        $OPTS_COMMON
}

config_win64() {
    ../configure \
        --prefix="$1" \
        --cross-prefix=x86_64-w64-mingw32- \
        --arch=x86 \
        --target-os=mingw64 \
        $OPTS_COMMON
}

usage() {
    echo "Usage: $0 <build type> <prefix>"
    exit 1
}

if [ -z "$2" ] ; then
    usage
fi

case "$1" in
    --win32)
        config_win32 "$2"
        ;;
    --win64)
        config_win64 "$2"
        ;;
    --linux)
        config_linux "$2"
        ;;
    *)
        usage
        ;;
esac
