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

# Set echo path accordingly with where this
# script was invoked
if [ "$#" -gt 0 ] && [ "$1" = "CMAKE" ]
then
	ECHO_BIN="$WSDIR/build/examples/echo/echo"
else
	ECHO_BIN="$WSDIR/examples/echo/echo"
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

# echo should exist
if [ ! -f "$ECHO_BIN" ] && [ ! -f "$ECHO_BIN.exe" ]
then
	echo "echo not found! please build it first, before"
	echo "proceeding!"
	exit 1
fi

printf "\n[+] Running Autobahn...\n"

# First spawn echo and get its pid
if [ -f "$ECHO_BIN" ]
then
	"$ECHO_BIN" &
	SR=$!
elif [ -f "$ECHO_BIN.exe" ]
then
	wine64-stable "$ECHO_BIN" &
	SR=$!
else
	echo "Error, echo[.exe] not found!"
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

# Kill echo
kill $SR

# If inside a CMake test, invoke the Python script too
if [ "$#" -gt 0 ] && [ "$1" = "CMAKE" ]
then
	cd "$CURDIR"
	python validate_output.py || python3 validate_output.py
	cd -
fi
