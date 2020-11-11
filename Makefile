# Copyright (C) 2016-2020 Davidson Francis <davidsondfgl@gmail.com>
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

# Source
C_SRC = $(wildcard $(SRC)/base64/*.c)    \
		$(wildcard $(SRC)/handshake/*.c) \
		$(wildcard $(SRC)/sha1/*.c)      \
		$(wildcard $(SRC)/*.c)

OBJ = $(C_SRC:.c=.o)

# Conflicts
.PHONY: doc
.PHONY: tests

# Paths
INCDIR = $(PREFIX)/include
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man
PKGDIR = $(LIBDIR)/pkgconfig

# General objects
%.o: %.c
	$(CC) $< $(CFLAGS) -c -o $@

# All
ifeq ($(AFL_FUZZ),no)
all: libws.a examples
else
all: libws.a tests
endif

# Library
libws.a: $(OBJ)
	$(AR) $(ARFLAGS) $(LIB) $^

# Examples
examples: libws.a
	$(MAKE) -C example/

# Fuzzing tests
tests: libws.a
	$(MAKE) -C tests/

# Install rules
install: all wsserver.pc
	@#Library
	install -d $(DESTDIR)$(LIBDIR)
	install -m 644 $(LIB) $(DESTDIR)$(LIBDIR)
	@#Headers
	install -d $(DESTDIR)$(INCDIR)/wsserver
	install -m 644 $(INCLUDE)/*.h $(DESTDIR)$(INCDIR)/wsserver
	@#Manpages
	install -d $(DESTDIR)$(MANDIR)/man3
	install -m 0644 $(MANPAGES)/ws_getaddress.3 $(DESTDIR)$(MANDIR)/man3/
	install -m 0644 $(MANPAGES)/ws_sendframe.3 $(DESTDIR)$(MANDIR)/man3/
	install -m 0644 $(MANPAGES)/ws_sendframe_bin.3 $(DESTDIR)$(MANDIR)/man3/
	install -m 0644 $(MANPAGES)/ws_sendframe_txt.3 $(DESTDIR)$(MANDIR)/man3/
	install -m 0644 $(MANPAGES)/ws_socket.3 $(DESTDIR)$(MANDIR)/man3/

# Uninstall rules
uninstall:
	rm -f  $(DESTDIR)$(LIBDIR)/$(LIB)
	rf -rf $(DESTDIR)$(INCDIR)/wsserver
	rm -f  $(DESTDIR)$(MANDIR)/man3/{ws_getaddress.3, ws_sendframe.3}
	rm -f  $(DESTDIR)$(MANDIR)/man3/{ws_sendframe_bin.3, ws_sendframe_txt.3}
	rm -f  $(DESTDIR)$(MANDIR)/man3/{ws_socket.3}
	rm -f  $(DESTDIR)$(PKGDIR)/wsserver.pc

# Generate wsserver.pc
wsserver.pc:
	@install -d $(DESTDIR)$(PKGDIR)
	@echo 'prefix='$(DESTDIR)$(PREFIX)    >  $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'libdir='$(DESTDIR)$(LIBDIR)    >> $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'includedir=$${prefix}/include' >> $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'Name: wsServer'                >> $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'Description: Tiny WebSocket Server Library' >> $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'Version: 1.0'                  >> $(DESTDIR)$(PKGDIR)/wsserver.pc
	@echo 'Libs: -L$${libdir} -lws'       >> $(DESTDIR)$(PKGDIR)/wsserver.pc
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
	@rm -f $(SRC)/base64/*.o
	@rm -f $(SRC)/handshake/*.o
	@rm -f $(SRC)/sha1/*.o
	@rm -f $(SRC)/*.o
	@rm -f $(LIB)
	@$(MAKE) clean -C example/
	@$(MAKE) clean -C tests/
