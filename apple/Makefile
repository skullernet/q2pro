# Makefile for Q2Pro.app, requires http://macdylibbundler.sourceforge.net/

TARGET = target
Q2PRO = $(TARGET)/Q2Pro.app
BINDIR = $(Q2PRO)/Contents/MacOS
DATADIR = $(Q2PRO)/Contents/Resources
DIST = $(TARGET)/Q2Pro.dmg
VOLUME_NAME = Q2Pro
REMOTE_USER = $(shell whoami)
HTTP_REPOSITORY = quetoo.org:/var/www/quetoo.org/files
HTTP_TARGET = $(REMOTE_USER)@$(HTTP_REPOSITORY)

all: install

pre-install:
	install -d $(TARGET)
	cp -R Q2Pro.app $(Q2PRO)
	find $(Q2PRO) -name .turd -delete

install-bin: pre-install
	install ../q2pro $(BINDIR)
	install ../q2proded $(BINDIR)
	install ../gamex86_64.so $(BINDIR)/baseq2

install-lib: install-bin
	install -d $(TARGET)/lib
	dylibbundler -b \
		-x "$(BINDIR)/q2pro" \
		-x "$(BINDIR)/q2proded" \
		-d $(TARGET)/lib -of -p @executable_path/lib
	mv $(TARGET)/lib $(BINDIR)/lib

install-share:
	install -m 0644 ../INSTALL $(DATADIR)
	install -m 0644 ../LICENSE $(DATADIR)
	install -m 0644 ../README $(DATADIR)
	install -m 0644 ../src/client/ui/q2pro.menu $(DATADIR)/baseq2

install: pre-install install-bin install-lib install-share

dist:
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create $(DIST) -srcfolder $(TARGET) -volname $(VOLUME_NAME)

dist-release:
	rsync -rzhP $(DIST) $(HTTP_TARGET)

clean:
	rm -rf $(TARGET)/*
