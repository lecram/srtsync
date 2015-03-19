PREFIX=/usr/local
MANPREFIX=$(PREFIX)/man
INSTALL=install -D

all: srtsync

srtsync: srtsync.c
	$(CC) $(CFLAGS) -o $@ $<

install: srtsync
	$(INSTALL) srtsync $(DESTDIR)$(PREFIX)/bin/srtsync
	$(INSTALL) srtsync.1 $(DESTDIR)$(MANPREFIX)/man1/srtsync.1

clean:
	$(RM) srtsync
