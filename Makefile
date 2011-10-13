# This Makefile is based on LuaSec's Makefile. Thanks to the LuaSec developers.
# Inform the location to intall the modules
LUAPATH  = /usr/share/lua/5.1
LUACPATH = /usr/lib/lua/5.1
INCDIR   = -I/usr/include/lua5.1
LIBDIR   = -L/usr/lib

# For Mac OS X: set the system version
MACOSX_VERSION = 10.4

PLAT = none
DEFS = -DLB_SUBBUFFER
MEXT = .so
CMOD = buffer$(MEXT)
OBJS = lbuffer.o lb_interface.o

LIBS = -llua
WARN = -Wall -pedantic

BSD_CFLAGS  = -O2 -fPIC
BSD_LDFLAGS = -O -shared -fPIC $(LIBDIR)

LNX_CFLAGS  = -O2 -fPIC
LNX_LDFLAGS = -O -shared -fPIC $(LIBDIR)

MAC_ENV     = env MACOSX_DEPLOYMENT_TARGET='$(MACVER)'
MAC_CFLAGS  = -O2 -fPIC -fno-common
MAC_LDFLAGS = -bundle -undefined dynamic_lookup -fPIC $(LIBDIR)

MINGW_INCDIR = -Id:/lua/include
MINGW_LIBS = d:/lua/lua51.dll
MINGW_MEXT = .dll
MINGW_CFLAGS = -O2 -mdll -DLUA_BUILD_AS_DLL
MINGW_LDFLAGS = -mdll

CC = gcc
LD = $(MYENV) gcc
CFLAGS  = $(MYCFLAGS) $(WARN) $(INCDIR) $(DEFS)
LDFLAGS = $(MYLDFLAGS) $(LIBDIR)

.PHONY: test clean install none linux bsd macosx

none:
	@echo "Usage: $(MAKE) <platform>"
	@echo "  * linux"
	@echo "  * bsd"
	@echo "  * macosx"
	@echo "  * mingw"

install: $(CMOD)
	cp $(CMOD) $(LUACPATH)

uninstall:
	-rm $(LUACPATH)/$(CMOD)

linux:
	@$(MAKE) $(CMOD) PLAT=linux MYCFLAGS="$(LNX_CFLAGS)" MYLDFLAGS="$(LNX_LDFLAGS)" INCDIR="$(INCDIR)" LIBDIR="$(LIBDIR)" DEFS="$(DEFS)"

bsd:
	@$(MAKE) $(CMOD) PLAT=bsd MYCFLAGS="$(BSD_CFLAGS)" MYLDFLAGS="$(BSD_LDFLAGS)" INCDIR="$(INCDIR)" LIBDIR="$(LIBDIR)" DEFS="$(DEFS)"

macosx:
	@$(MAKE) $(CMOD) PLAT=macosx MYCFLAGS="$(MAC_CFLAGS)" MYLDFLAGS="$(MAC_LDFLAGS)" MYENV="$(MAC_ENV)" INCDIR="$(INCDIR)" LIBDIR="$(LIBDIR)" DEFS="$(DEFS)"

mingw:
	@$(MAKE) $(MINGW_CMOD) PLAT=mingw MYCFLAGS="$(MINGW_CFLAGS)" MYLDFLAGS="$(MINGW_LDFLAGS)" INCDIR="$(MINGW_INCDIR)" LIBS="$(MINGW_LIBS)" MEXT="$(MINGW_MEXT)"

test: $(PLAT)
	lua ./test.lua

clean:
	-rm -f $(OBJS) $(CMOD) $(MINGW_CMOD)

.c.o:
	$(CC) $(CFLAGS) $< -c -o $@

$(CMOD): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o $@
