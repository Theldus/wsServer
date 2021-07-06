#!/usr/bin/env bash

#
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
#

export CURDIR="$(cd "$(dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd)"
export WSDIR="$(readlink -f $CURDIR/../)"

# AFL Fuzzing
if [ ! -x "$(command -v afl-fuzz)" ]
then
	echo "AFL not found! please set afl-fuzz in your PATH"
	echo "and AFL_HOME too"
	exit 1
fi

# ws_file should exist
if [ ! -f "$CURDIR/ws_file" ]
then
	echo "ws_file not found! please build it first, before"
	echo "proceeding!"
	exit 1
fi

echo -e "\n[+] Fuzzing wsServer..."

# AFL output
if [ -z "$AFL_OUT" ]
then
	echo "Please note that is recommended to use it inside a ramdisk..."
	echo "You can set an output folder with: AFL_OUT env var"
	AFL_OUT=$CURDIR/out
fi

echo -e " -> Output dir: ($AFL_OUT)\n"
afl-fuzz -i "$CURDIR/in" -o "$AFL_OUT" "$CURDIR/ws_file" @@
