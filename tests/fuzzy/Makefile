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

CC       = afl-gcc
WSDIR    = $(CURDIR)/../../
INCLUDE  = -I $(WSDIR)/include
CFLAGS  +=  -Wall -Wextra -g -DVERBOSE_MODE -DAFL_FUZZ
CFLAGS  +=  $(INCLUDE) -std=c99 -pthread -pedantic
LIB      =  $(WSDIR)/libws.a

.PHONY: all run_fuzzy clean

# Examples
all: ws_file run_fuzzy

# ws_file
ws_file: ws_file.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) ws_file.c -o ws_file $(LIB)

# Run fuzzing tests
run_fuzzy: ws_file
	@bash run-fuzzy.sh

# Clean
clean:
	@rm -f ws_file
