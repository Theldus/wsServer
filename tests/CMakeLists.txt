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

if(ENABLE_WSSERVER_TEST)

	find_program(SHELL sh)

	if(SHELL)
		add_test(NAME Autobahn|Testsuite
			COMMAND "${SHELL}" "${CMAKE_CURRENT_SOURCE_DIR}/run-autobahn.sh" "CMAKE"
		)
	else()
		message(FATAL_ERROR "Unable to locate shell")
	endif(SHELL)

endif(ENABLE_WSSERVER_TEST)
