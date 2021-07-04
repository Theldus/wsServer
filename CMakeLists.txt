# Copyright (C) 2016-2021  Davidson Francis <davidsondfgl@gmail.com>
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

cmake_minimum_required(VERSION 3.5.2)

project(ws C)

set(CMAKE_C_STANDARD 99)

include_directories(include)

add_library(ws
    src/ws.c
    src/base64/base64.c
    src/sha1/sha1.c
    src/handshake/handshake.c)

if(WIN32)
	target_link_libraries(ws pthread ws2_32)
else()
	target_link_libraries(ws pthread)
endif(WIN32)

add_executable(send_receive
    example/send_receive.c)
target_link_libraries(send_receive ws)
