# Makefile for Q2PRO.app, requires http://macdylibbundler.sourceforge.net/

DATA = ../../src/client/ui
TARGET = target
Q2PRO = $(TARGET)/Q2PRO.app
LIBRARIES = $(Q2PRO)/Contents/Libraries
BINARIES = $(Q2PRO)/Contents/MacOS
RESOURCES = $(Q2PRO)/Contents/Resources
DMG = $(TARGET)/Q2PRO.dmg
VOLUME_NAME = Q2PRO
HTTP_REPOSITORY = quake2world.net:/var/www/quake2world/files

all: bundle

pre-install:
	install -d $(TARGET)
	cp -r Q2PRO.app $(Q2PRO)

install: pre-install
	install ../../q2pro $(BINARIES)/q2pro
	install ../../q2proded $(BINARIES)/q2proded
	install ../../gamex86_64.so $(BINARIES)/baseq2/gamex86_64.so

install-data:
	install -d  $(RESOURCES)/baseq2
	install -m 0644 $(DATA)/q2pro.menu $(RESOURCES)/baseq2/q2pro.menu

bundle: install install-data
	dylibbundler -b \
		-x "$(BINARIES)/q2pro" \
	-d "$(LIBRARIES)" -of -p @executable_path/../Libraries

image: bundle
	ln -f -s /Applications $(TARGET)/Applications
	hdiutil create $(DMG) -srcfolder $(TARGET) -volname $(VOLUME_NAME)

release-image: image
	scp $(DMG) $(HTTP_REPOSITORY)

clean:
	rm -rf $(TARGET)
