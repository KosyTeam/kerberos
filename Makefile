#Flags & Co.
CC = gcc
PKGFLAGS = $(shell pkg-config --libs --cflags glib-2.0 gtk4 wayland-client gtk-layer-shell-0)
CFLAGS = -Wall -rdynamic -lpam -lpam_misc
SOURCES = src/main.c src/actions.c src/wlrp-handler.c src/input-inhibitor.c pgresources.c
PREFIX = /usr/local
PGDATA = /usr/local/share #set to tmpdata if running without installing
LANGS = de

ifeq (, $(shell which glib-compile-resources))
$(warning "glib-compile-resources is not available. Build will fail.")
endif

ifeq (, $(shell which glib-compile-schemas))
$(warning "glib-compile-schemas is not available. Install will fail.")
endif

all: $(SOURCES) tmpgen
	./configure.sh $(PREFIX) $(PGDATA)
	$(CC) $(SOURCES) config.h $(CFLAGS) $(PKGFLAGS) -Isrc/ -o kerberos
	
pgresources.c:
	glib-compile-resources data/pgresources.gresource.xml --sourcedir=data --generate-source --target pgresources.c
	
.PHONY: tmpgen
tmpgen:
	mkdir -p tmpdata/glib-2.0/schemas
	cp data/eu.kosy.Kerberos.gschema.xml tmpdata/glib-2.0/schemas/
	glib-compile-schemas tmpdata/glib-2.0/schemas/
	mkdir -p tmpdata/kerberos
	cp data/default.png tmpdata/kerberos/default.png

run:
	$(information "Using make run is only intened for testing purposes.")
	XDG_DATA_DIRS=$$XDG_DATA_DIRS:tmpdata ./kerberos
	
update-locale:
	$(information "This only updates exsisting locales for new ones use msginit and THEN add it to LANGS list.")
	xgettext --keyword=_ --sort-output -o po/eu.kosy.Kerberos.pot src/main.c src/actions.c data/locker.ui
	for file in  $(LANGS) ; \
	do \
		msgmerge --update po/$$file.po po/eu.kosy.Kerberos.pot; \
		mkdir -p tmpdata/locale/$$file/LC_MESSAGES/; \
		msgfmt --output-file=tmpdata/locale/$$file/LC_MESSAGES/eu.kosy.Kerberos.mo po/$$file.po; \
	done 

appimage:
	$(error not implemented)

install:
	mkdir -p $(PREFIX)/bin
	cp kerberos $(PREFIX)/bin/kerberos
	mkdir -p $(PREFIX)/share/glib-2.0/schemas/
	cp data/eu.kosy.Kerberos.gschema.xml $(PREFIX)/share/glib-2.0/schemas/
	glib-compile-schemas $(PREFIX)/share/glib-2.0/schemas/
	mkdir -p $(PREFIX)/share/kerberos
	cp data/default.png $(PREFIX)/share/kerberos/
	cp -r tmpdata/locale $(PREFIX)/share/
	
uninstall:
	rm -f $(PREFIX)/bin/kerberos
	rm -f $(PREFIX)/share/glib-2.0/schemas/eu.kosy.Kerberos.gschema.xml
	glib-compile-schemas $(PREFIX)/share/glib-2.0/schemas/
	rm -rf $(PREFIX)/share/kerberos
	for file in $(LANGS); \
	do \
		rm -f $(PREFIX)/share/locale/$$file/LC_MESSAGES/eu.kosy.Kerberos.mo; \
	done 

.PHONY: clean
clean:
	rm -f pgresources.c
	rm -f kerberos
	rm -rf tmpdata
	rm -f config.h
