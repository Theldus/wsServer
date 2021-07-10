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

# Set send_receive path accordingly with where this
# script was invoked
if [ "$#" -gt 0 ] && [ "$1" = "CMAKE" ]
then
	SR_BIN="$WSDIR/build/send_receive"
else
	SR_BIN="$WSDIR/example/send_receive"
fi

# AFL Fuzzing
if [ -z "$TRAVIS" ]
then
	if [ ! -x "$(command -v wstest)" ]
	then
		echo "Autobahn|Testsuite not found!"
		echo "You can install with something like:"
		echo "	virtualenv ~/wstest"
		echo "	source ~/wstest/bin/activate"
		echo "	pip install autobahntestsuite"
		exit 1
	fi
else
	if [ ! -x "$(command -v docker)" ]
	then
		echo "Docker not found!!"
		exit 1
	fi
fi

# send_receive should exist
if [ ! -f "$SR_BIN" ] && [ ! -f "$SR_BIN.exe" ]
then
	echo "send_receive not found! please build it first, before"
	echo "proceeding!"
	exit 1
fi

printf "\n[+] Running Autobahn...\n"

# First spawn send_receive and get its pid
if [ -f "$SR_BIN" ]
then
	"$SR_BIN" &
	SR=$!
elif [ -f "$SR_BIN.exe" ]
then
	wine64-stable "$SR_BIN" &
	SR=$!
else
	echo "Erro, send_receive[.exe] not found!"
	exit 1
fi

# Spawn Autobahn fuzzying client
if [ -z "$TRAVIS" ]
then
	cd "$CURDIR"
	wstest -m fuzzingclient --spec wsserver_autobahn/fuzzingclient.json
	cd -
else
	# Run docker image
	docker run -it --rm -v \
		"${CURDIR}/wsserver_autobahn:/wsserver_autobahn" \
		theldus/autobahn-testsuite:1.0
fi

# Kill send_receive
kill $SR

# If inside a CMake test, invoke the Python script too
if [ "$#" -gt 0 ] && [ "$1" = "CMAKE" ]
then
	cd "$CURDIR"
	python validate_output.py || python3 validate_output.py
	cd -
fi
