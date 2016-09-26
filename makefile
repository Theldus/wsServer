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
INCLUDE  = $(CURDIR)/include
CFLAGS   =  -Wall -Werror
CFLAGS  +=  -I $(INCLUDE) -std=c99
ARFLAGS  =  cru
LIB      =  libws.a

C_SRC = $(wildcard base64/*.c)    \
		$(wildcard handshake/*.c) \
		$(wildcard sha1/*.c)      \
		$(wildcard *.c)

OBJ = $(C_SRC:.c=.o)

all: $(OBJ)
	$(AR) $(ARFLAGS) $(LIB) $^

%.o: %.c
	$(CC) $< $(CFLAGS) -c -o $@

clean:
	@rm -f base64/*.o
	@rm -f handshake/*.o
	@rm -f sha1/*.o
	@rm -f *.o
	@rm -f $(LIB)
