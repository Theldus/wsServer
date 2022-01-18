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
    src/handshake/handshake.c
    src/utf8/utf8.c
)

if(WIN32)
	target_link_libraries(ws pthread ws2_32 -static)
else()
	target_link_libraries(ws pthread)
endif(WIN32)

add_executable(send_receive
    example/send_receive.c)
target_link_libraries(send_receive ws)

add_executable(toyws_test
    extra/toyws/tws_test.c
    extra/toyws/toyws.c)

if(WIN32)
    target_link_libraries(toyws_test ws2_32 -static)
endif(WIN32)

option(ENABLE_WSSERVER_TEST "Enable wsServer testing (requires Autobahn)" OFF)
if(ENABLE_WSSERVER_TEST)
	enable_testing()
	# Disable verbose output of send_receive
	target_compile_definitions(send_receive PRIVATE DISABLE_VERBOSE)
	add_subdirectory(tests)
endif(ENABLE_WSSERVER_TEST)

option(VALIDATE_UTF8 "Enable UTF-8 validation (default ON)" ON)
if(VALIDATE_UTF8)
	target_compile_definitions(ws PRIVATE VALIDATE_UTF8)
endif(VALIDATE_UTF8)
