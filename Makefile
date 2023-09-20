# Copyright (C) 2016-2022 Davidson Francis <davidsondfgl@gmail.com>
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
INCLUDE   = $(CURDIR)/include
SRC       = $(CURDIR)/src
CFLAGS   +=  -Wall -Wextra -O2
CFLAGS   +=  -I $(INCLUDE) -std=c99 -pedantic
ARFLAGS   =  cru
LIB       =  libws.a
MCSS_DIR ?= /usr/bin/
MANPAGES  = $(CURDIR)/doc/man/man3
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

# Source
C_SRC = $(SRC)/base64.c \
	$(SRC)/handshake.c \
	$(SRC)/sha1.c \
	$(SRC)/utf8.c \
	$(SRC)/ws.c

OBJ = $(C_SRC:.c=.o)

# Conflicts
.PHONY: doc fuzzy

# Paths
INCDIR = $(PREFIX)/include
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man
PKGDIR = $(LIBDIR)/pkgconfig

# Extra paths
TOYWS  = $(CURDIR)/extra/toyws

# General objects
%.o: %.c
	$(CC) $< $(CFLAGS) -c -o $@

# All
ifeq ($(AFL_FUZZ),no)
all: libws.a examples
else
all: libws.a fuzzy
endif

# Library
libws.a: $(OBJ)
	$(AR) $(ARFLAGS) $(LIB) $^

# Examples
examples: libws.a
	$(MAKE) -C examples/

# Autobahn tests
tests: examples
	$(MAKE) all -C tests/ VERBOSE_EXAMPLES="$(VERBOSE_EXAMPLES)"
tests_check:
	$(MAKE) check_results -C tests/

# Fuzzing tests
fuzzy: libws.a
	$(MAKE) -C tests/fuzzy

# ToyWS client
toyws_test: $(TOYWS)/tws_test.o $(TOYWS)/toyws.o
	$(CC) $^ $(CFLAGS) -I $(TOYWS) -o $@

# Install rules
install: libws.a wsserver.pc
	@#Library
	install -d $(DESTDIR)$(LIBDIR)
	install -m 644 $(LIB) $(DESTDIR)$(LIBDIR)
	@#Headers
	install -d $(DESTDIR)$(INCDIR)/wsserver
	install -m 644 $(INCLUDE)/*.h $(DESTDIR)$(INCDIR)/wsserver
	@#Manpages
	install -d $(DESTDIR)$(MANDIR)/man3
	install -m 0644 $(MANPAGES)/*.3 $(DESTDIR)$(MANDIR)/man3/

# Uninstall rules
uninstall:
	rm -f  $(DESTDIR)$(LIBDIR)/$(LIB)
	rm -rf $(DESTDIR)$(INCDIR)/wsserver
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_getaddress.3
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_sendframe.3
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_sendframe_bin.3
	rm -f  $(DESTDIR)$(MANDIR)/man3/ws_sendframe_txt.3
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
	@rm -f $(OBJ)
	@rm -f $(LIB)
	@rm -f $(TOYWS)/toyws.o $(TOYWS)/tws_test.o toyws_test
	@$(MAKE) clean -C examples/
	@$(MAKE) clean -C tests/
	@$(MAKE) clean -C tests/fuzzy
