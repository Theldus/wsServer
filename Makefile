# Copyright (C) 2016-2023 Davidson Francis <davidsondfgl@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>

CC       ?= gcc
AR        = ar
LIB_WS    = libws.a
INCLUDE   = include
CFLAGS   += -Wall -Wextra -O2
CFLAGS   += -I $(INCLUDE) -std=c99 -pedantic
LDLIBS    = $(LIB_WS) -pthread
ARFLAGS   =  cru
MCSS_DIR ?= /usr/bin/
MANPAGES  = doc/man/man3
AFL_FUZZ ?= no
VERBOSE_EXAMPLES ?= yes
VALIDATE_UTF8 ?= yes

# Prefix
ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

# Detect machine type
MACHINE := $(shell uname -m)
ifeq ($(MACHINE), x86_64)
	LIBDIR = $(PREFIX)/lib64
else
	LIBDIR = $(PREFIX)/lib
endif

# Check if AFL fuzzing enabled
ifeq ($(AFL_FUZZ), yes)
    CC = afl-gcc
    CFLAGS := $(filter-out -O2,$(CFLAGS))
    CFLAGS += -DVERBOSE_MODE -DAFL_FUZZ -g
    $(info [+] AFL Fuzzing build enabled)
endif

# Check if UTF-8 validation is enabled
ifeq ($(VALIDATE_UTF8), yes)
	CFLAGS += -DVALIDATE_UTF8
endif

# Pretty print
Q := @
ifeq ($(V), 1)
	Q :=
endif

# Conflicts
.PHONY: all examples tests fuzzy install uninstall doc clean

# Paths
INCDIR = $(PREFIX)/include
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man
PKGDIR = $(LIBDIR)/pkgconfig

# Extra paths
TOYWS  = extra/toyws

# All
ifeq ($(AFL_FUZZ),no)
all: Makefile libws.a examples $(TOYWS)/toyws_test
else
all: Makefile libws.a fuzzy
endif

#
# Library
#

%.o: %.c
	@echo "  CC      $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

# Source
WS_OBJ = src/base64.o \
	src/handshake.o   \
	src/sha1.o        \
	src/utf8.o        \
	src/ws.o

# Headers
src/ws.o: include/ws.h include/utf8.h
src/base.o: include/base64.h
src/handshake.o: include/base64.h include/ws.h include/sha1.h
src/sha1.o: include/sha1.h
src/utf8.o: include/utf8.h

# Lib
$(LIB_WS): $(WS_OBJ)
	@echo "  AR      $@"
	$(Q)$(AR) $(ARFLAGS) $(LIB_WS) $^

# Examples
examples: examples/echo/echo examples/ping/ping
examples/echo/echo: examples/echo/echo.o $(LIB_WS)
	@echo "  LINK    $@"
	$(Q)$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)
examples/ping/ping: examples/ping/ping.o $(LIB_WS)
	@echo "  LINK    $@"
	$(Q)$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

# Autobahn tests
tests: examples
	$(MAKE) all -C tests/ VERBOSE_EXAMPLES="$(VERBOSE_EXAMPLES)"
tests_check:
	$(MAKE) check_results -C tests/

# Fuzzing tests
fuzzy: libws.a
	$(MAKE) -C tests/fuzzy

# ToyWS client
$(TOYWS)/toyws_test: $(TOYWS)/tws_test.o $(TOYWS)/toyws.o
	@echo "  LINK    $@"
	$(Q)$(CC) $(CFLAGS) $^ -o $@

# Install rules
install: libws.a wsserver.pc
	@echo "  INSTALL      $@"
	@#Library
	install -d $(DESTDIR)$(LIBDIR)
	install -m 644 $(LIB_WS) $(DESTDIR)$(LIBDIR)
	@#Headers
	install -d $(DESTDIR)$(INCDIR)/wsserver
	install -m 644 $(INCLUDE)/*.h $(DESTDIR)$(INCDIR)/wsserver
	@#Manpages
	install -d $(DESTDIR)$(MANDIR)/man3
	install -m 0644 $(MANPAGES)/*.3 $(DESTDIR)$(MANDIR)/man3/

# Uninstall rules
uninstall:
	@echo "  UNINSTALL      $@"
	rm -f  $(DESTDIR)$(LIBDIR)/$(LIB_WS)
	rm -rf $(DESTDIR)$(INCDIR)/wsserver
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_getaddress.3
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_getport.3
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_sendframe.3
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_sendframe_bcast.3
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_sendframe_bin.3
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_sendframe_bin_bcast.3
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_sendframe_txt.3
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_sendframe_txt_bcast.3
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_socket.3
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_close_client.3
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_get_state.3
	rm -f  $(DESTDIR)$(PKGDIR)/wsserver.pc

# Generate wsserver.pc
wsserver.pc:
	@install -d $(DESTDIR)$(PKGDIR)
	@echo 'prefix='$(PREFIX)    >  $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'libdir='$(LIBDIR)    >> $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'includedir=$${prefix}/include' >> $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'Name: wsServer'                >> $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'Description: Tiny WebSocket Server Library' >> $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'Version: 1.0'                  >> $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'Libs: -L$${libdir} -lws -pthread' >> $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'Libs.private:'                 >> $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'Cflags: -I$${includedir}/wsserver' >> $(DESTDIR)$(PKGDIR)/wsserver.pc

# Documentation, requires Doxygen and m.css
#   -> https://mcss.mosra.cz/documentation/doxygen/
#
# If for some reason m.css usage is not possible,
# change the following lines in doxy.conf to:
#   GENERATE_HTML = no  -> GENERATE_HTML = yes
#   GENERATE_XML  = yes -> GENERATE_XML  = no
#
doc:
	@echo "Generating docs..."
	@doxygen doc/doxygen/doxy.conf
	@$(MCSS_DIR)/doxygen.py doc/doxygen/Doxyfile-mcss --no-doxygen\
		--templates doc/doxygen/templates/

# Clean
clean:
	@rm -f $(WS_OBJ)
	@rm -f $(LIB_WS)
	@rm -f $(TOYWS)/toyws.o $(TOYWS)/tws_test.o $(TOYWS)toyws_test
	@rm -f examples/echo/{echo,echo.o}
	@rm -f examples/ping/{ping,ping.o}
	@$(MAKE) clean -C tests/
	@$(MAKE) clean -C tests/fuzzy
