PREFIX := i686-w64-mingw32-
PREFIX64 := x86_64-w64-mingw32-

ZLIB_VER := 1.2.11
JPEG_VER := 9c
PNG_VER := 1.6.35
CURL_VER := 7.61.1
OPENAL_VER := 1.19.0

ZLIB := zlib-$(ZLIB_VER)
JPEG := jpeg-$(JPEG_VER)
PNG := libpng-$(PNG_VER)
CURL := curl-$(CURL_VER)
OPENAL := openal-soft-$(OPENAL_VER)

ZLIB_TAR := $(ZLIB).tar.gz
JPEG_TAR := jpegsrc.v$(JPEG_VER).tar.gz
PNG_TAR := $(PNG).tar.xz
CURL_TAR := $(CURL).tar.xz
OPENAL_TAR := $(OPENAL).tar.bz2

CURL_CFLAG_EXTRAS := -DCURL_STATICLIB -DHTTP_ONLY -DCURL_DISABLE_CRYPTO_AUTH

INC := $(DESTDIR)inc
LIB := $(DESTDIR)lib
LIB64 := $(DESTDIR)lib64

all: zlib jpeg png curl zlib64 jpeg64 png64 curl64

default: all

.PHONY: clean install

$(ZLIB_TAR):
	wget http://www.zlib.net/$(ZLIB_TAR)

$(JPEG_TAR):
	wget http://ijg.org/files/$(JPEG_TAR)

$(PNG_TAR):
	wget http://downloads.sourceforge.net/sourceforge/libpng/$(PNG_TAR)

$(CURL_TAR):
	wget https://curl.haxx.se/download/$(CURL_TAR)

$(OPENAL_TAR):
	wget http://kcat.strangesoft.net/openal-releases/$(OPENAL_TAR)

glext.h:
	wget https://www.khronos.org/registry/OpenGL/api/GL/glext.h

wglext.h:
	wget https://www.khronos.org/registry/OpenGL/api/GL/wglext.h

khrplatform.h:
	wget https://www.khronos.org/registry/EGL/api/KHR/khrplatform.h

fetch: fetch-stamp
fetch-stamp: $(ZLIB_TAR) $(JPEG_TAR) $(PNG_TAR) $(CURL_TAR) $(OPENAL_TAR) glext.h wglext.h khrplatform.h
	sha256sum -c checksum
	touch $@

extract: extract-stamp
extract-stamp: fetch-stamp
	mkdir -p build build64
	tar -C build -xf $(ZLIB_TAR)
	tar -C build -xf $(JPEG_TAR)
	tar -C build -xf $(PNG_TAR)
	tar -C build -xf $(CURL_TAR)
	tar -C build -xf $(OPENAL_TAR) $(OPENAL)/include/AL
	tar -C build64 -xf $(ZLIB_TAR)
	tar -C build64 -xf $(JPEG_TAR)
	tar -C build64 -xf $(PNG_TAR)
	tar -C build64 -xf $(CURL_TAR)
	cp -a build/$(JPEG)/jconfig.vc build/$(JPEG)/jconfig.h
	cp -a build64/$(JPEG)/jconfig.vc build64/$(JPEG)/jconfig.h
	touch $@

zlib: zlib-stamp
zlib-stamp: extract-stamp
	$(MAKE) -C build/$(ZLIB) -f win32/Makefile.gcc \
		PREFIX=$(PREFIX) \
		libz.a
	touch $@

jpeg: jpeg-stamp
jpeg-stamp: extract-stamp
	$(MAKE) -C build/$(JPEG) -f makefile.ansi \
		CC="$(PREFIX)gcc" \
		CFLAGS=-O2 \
		AR="$(PREFIX)ar rc" \
		AR2="$(PREFIX)ranlib" \
		libjpeg.a
	touch $@

png: png-stamp
png-stamp: zlib-stamp
	$(MAKE) -C build/$(PNG) -f scripts/makefile.gcc \
		CC="$(PREFIX)gcc" \
		AR_RC="$(PREFIX)ar rcs" \
		RANLIB="$(PREFIX)ranlib" \
		RC="$(PREFIX)windres" \
		STRIP="$(PREFIX)strip" \
		ZLIBINC=../$(ZLIB) \
		ZLINLIB=../$(ZLIB) \
		libpng.a
	touch $@

curl: curl-stamp
curl-stamp: zlib-stamp
	ZLIB_PATH=../../$(ZLIB) ZLIB=1 IPV6=1 \
	$(MAKE) -C build/$(CURL)/lib -f Makefile.m32 \
		CROSSPREFIX="$(PREFIX)" \
		CURL_CFLAG_EXTRAS="$(CURL_CFLAG_EXTRAS)" \
		libcurl.a
	touch $@

zlib64: zlib64-stamp
zlib64-stamp: extract-stamp
	$(MAKE) -C build64/$(ZLIB) -f win32/Makefile.gcc \
		PREFIX=$(PREFIX64) \
		libz.a
	touch $@

jpeg64: jpeg64-stamp
jpeg64-stamp: extract-stamp
	$(MAKE) -C build64/$(JPEG) -f makefile.ansi \
		CC="$(PREFIX64)gcc" \
		CFLAGS=-O2 \
		AR="$(PREFIX64)ar rc" \
		AR2="$(PREFIX64)ranlib" \
		libjpeg.a
	touch $@

png64: png64-stamp
png64-stamp: zlib64-stamp
	$(MAKE) -C build64/$(PNG) -f scripts/makefile.gcc \
		CC="$(PREFIX64)gcc" \
		AR_RC="$(PREFIX64)ar rcs" \
		RANLIB="$(PREFIX64)ranlib" \
		RC="$(PREFIX64)windres" \
		STRIP="$(PREFIX64)strip" \
		ZLIBINC=../$(ZLIB) \
		ZLINLIB=../$(ZLIB) \
		libpng.a
	touch $@

curl64: curl64-stamp
curl64-stamp: zlib64-stamp
	ZLIB_PATH=../../$(ZLIB) ZLIB=1 IPV6=1 ARCH=w64 \
	$(MAKE) -C build64/$(CURL)/lib -f Makefile.m32 \
		CROSSPREFIX="$(PREFIX64)" \
		CURL_CFLAG_EXTRAS="$(CURL_CFLAG_EXTRAS)" \
		libcurl.a
	touch $@

clean:
	rm -rf build build64
	rm -f *-stamp

install: all
	install -d $(INC) $(LIB) $(LIB64) $(INC)/curl $(INC)/AL $(INC)/GL $(INC)/KHR
	install -m 644 build/$(ZLIB)/zconf.h $(INC)/zconf.h
	install -m 644 build/$(ZLIB)/zlib.h $(INC)/zlib.h
	install -m 644 build/$(ZLIB)/libz.a $(LIB)/libz.a
	install -m 644 build/$(JPEG)/jpeglib.h $(INC)/jpeglib.h
	install -m 644 build/$(JPEG)/jconfig.h $(INC)/jconfig.h
	install -m 644 build/$(JPEG)/jmorecfg.h $(INC)/jmorecfg.h
	install -m 644 build/$(JPEG)/libjpeg.a $(LIB)/libjpeg.a
	install -m 644 build/$(PNG)/pngconf.h $(INC)/pngconf.h
	install -m 644 build/$(PNG)/png.h $(INC)/png.h
	install -m 644 build/$(PNG)/pnglibconf.h $(INC)/pnglibconf.h
	install -m 644 build/$(PNG)/libpng.a $(LIB)/libpng.a
	install -m 644 build/$(CURL)/include/curl/*.h $(INC)/curl
	install -m 644 build/$(CURL)/lib/libcurl.a $(LIB)/libcurl.a
	install -m 644 build64/$(ZLIB)/libz.a $(LIB64)/libz.a
	install -m 644 build64/$(JPEG)/libjpeg.a $(LIB64)/libjpeg.a
	install -m 644 build64/$(PNG)/libpng.a $(LIB64)/libpng.a
	install -m 644 build64/$(CURL)/lib/libcurl.a $(LIB64)/libcurl.a
	install -m 644 glext.h wglext.h $(INC)/GL
	install -m 644 khrplatform.h $(INC)/KHR
	install -m 644 build/$(OPENAL)/include/AL/* $(INC)/AL
