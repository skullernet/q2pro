# Makefile for Q2PRO Linux distributable

DATA = ../../src/client/ui
TARGET = target
Q2PRO = $(TARGET)/q2pro
LIBRARIES = $(Q2PRO)/lib
BINARIES = $(Q2PRO)/bin
RESOURCES = $(Q2PRO)/share
TGZ = $(TARGET)/q2pro-x86_64.tar.gz
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files

all: bundle

pre-install:
	install -d $(TARGET)
	cp -r q2pro $(TARGET)
	install -d $(LIBRARIES)
	install -d $(BINARIES)
	install -d $(RESOURCES)

install: pre-install
	install ../../q2pro $(BINARIES)/q2pro
	install ../../q2proded $(BINARIES)/q2proded
	install ../../gamex86_64.so $(LIBRARIES)/baseq2/gamex86_64.so

install-data:
	install -d  $(RESOURCES)/baseq2
	install -m 0644 $(DATA)/q2pro.menu $(RESOURCES)/baseq2/q2pro.menu

bundle: install install-data

image: bundle
	tar -czvf $(TGZ) -C $(Q2PRO)

release-image: image
	scp $(TGZ) $(HTTP_REPOSITORY)

clean:
	rm -rf $(TARGET)
