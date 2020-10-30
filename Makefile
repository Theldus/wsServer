#Copyright (C) 2016  Davidson Francis <davidsondfgl@gmail.com>
#
#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation, either version 3 of the License, or
#(at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program.  If not, see <http://www.gnu.org/licenses/>

CC=gcc
AR=ar
INCLUDE   = $(CURDIR)/include
SRC       = $(CURDIR)/src
CFLAGS    =  -Wall -Werror -g
CFLAGS   +=  -I $(INCLUDE) -std=c99
ARFLAGS   =  cru
LIB       =  libws.a
MCSS_DIR ?= /usr/bin/

C_SRC = $(wildcard $(SRC)/base64/*.c)    \
		$(wildcard $(SRC)/handshake/*.c) \
		$(wildcard $(SRC)/sha1/*.c)      \
		$(wildcard $(SRC)/*.c)

OBJ = $(C_SRC:.c=.o)

# Conflicts
.PHONY: doc

# General objects
%.o: %.c
	$(CC) $< $(CFLAGS) -c -o $@

# All
all: libws.a examples

# Library
libws.a: $(OBJ)
	$(AR) $(ARFLAGS) $(LIB) $^

# Examples
examples: libws.a
	$(MAKE) -C example/

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
