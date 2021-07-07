#!/usr/bin/env sh

#
# Copyright (C) 2016-2021 Davidson Francis <davidsondfgl@gmail.com>
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

set -e

# Paths
CURDIR="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
export CURDIR

WSDIR="$(readlink -f "$CURDIR"/../)"
export WSDIR

# AFL Fuzzing
if [ ! -x "$(command -v wstest)" ]
then
	echo "Autobahn|Testsuite not found!"
	echo "You can install with something like:"
	echo "	virtualenv ~/wstest"
	echo "	source ~/wstest/bin/activate"
	echo "	pip install autobahntestsuite"
	exit 1
fi

# send_receive should exist
if [ ! -f "$WSDIR/example/send_receive" ]
then
	echo "send_receive not found! please build it first, before"
	echo "proceeding!"
	exit 1
fi

printf "\n[+] Running Autobahn...\n"

# First spawn send_receive and get its pid
"$WSDIR"/example/send_receive &
SR=$!

# Spawn Autobahn fuzzying client
cd "$CURDIR"
wstest -m fuzzingclient
cd -

# Kill send_receive
kill $SR
