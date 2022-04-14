# Copyright (C) 2022  Davidson Francis <davidsondfgl@gmail.com>
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

include(CheckIncludeFiles)

add_executable(vtouchpad vtouchpad.c)
target_compile_definitions(vtouchpad PRIVATE DISABLE_VERBOSE)

if (CMAKE_SYSTEM_NAME MATCHES "Linux" OR CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
	target_sources(vtouchpad PRIVATE mouse_x11.c)

	#
	# If FreeBSD, add /usr/local/include to the search path too,
	# see: https://wiki.freebsd.org/WarnerLosh/UsrLocal
	#
	if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
		set(CMAKE_REQUIRED_INCLUDES "/usr/local/include;/usr/include")
		target_link_directories(vtouchpad PUBLIC /usr/local/lib)
		target_include_directories(vtouchpad PUBLIC /usr/local/include)
	endif()

	CHECK_INCLUDE_FILES("xdo.h" HAVE_XDO_H)
	if (NOT HAVE_XDO_H)

		message(WARNING "libxdo/xdotool not found, trying to use XTest exts...")
		CHECK_INCLUDE_FILES("X11/extensions/XTest.h" HAVE_XTEST_H)

		if (NOT HAVE_XTEST_H)
			message(WARNING "Unable to find XTest.h, vtouchpad is unable "
				"to work in your system, disabling build...")
			set_target_properties(vtouchpad
				PROPERTIES EXCLUDE_FROM_ALL True)
		endif ()

		target_link_libraries(vtouchpad X11 Xtst ws pthread)
	else()
		target_compile_definitions(vtouchpad PUBLIC HAVE_XDOTOOL)
		target_link_libraries(vtouchpad X11 xdo ws pthread)
	endif()

elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
	target_sources(vtouchpad PRIVATE mouse_win.c)
	target_link_libraries(vtouchpad ws ws2_32)
else()
	message(WARNING "Operating System '${CMAKE_SYSTEM_NAME}' not supported!")
	set_target_properties(vtouchpad
		PROPERTIES EXCLUDE_FROM_ALL True)
endif ()
