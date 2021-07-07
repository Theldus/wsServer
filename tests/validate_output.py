#!/usr/bin/env python

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

import os
import sys
import json

# Fancy colors =)
RED="\033[0;31m"
GREEN="\033[0;32m"
NC="\033[0m"

# Return code
ret = 0

# Target file
file = "wsserver_autobahn/index.json"

#
# 'NOT PASSED' list
#
# Here we implement them as a dictionary as the lookup time
# is so much better than an ordinary 'list'.
#
# Although they seem like a lot of errors, they are all related to 'invalid
# UTF-8 sequences validation'. Since wsServer does not validate UTF-8 sequences,
# it is natural that all tests related to this do not pass.
#
# However, these tests represent about ~25% (76 out of 298) of the total tests,
# and wsServer passes everything else. Note, therefore, that wsServer is also
# fully capable of sending and receiving UTF-8 strings, it just doesn't validate
# them.
#
# More info at:
#   https://github.com/Theldus/wsServer/blob/master/doc/AUTOBAHN.md
#
np_dict = {
	"6.3.1": 1,  "6.3.2": 1,
	"6.4.1": 1,  "6.4.2": 1, "6.4.3": 1, "6.4.4": 1,
	"6.6.1": 1,  "6.6.3": 1, "6.6.4": 1, "6.6.6": 1, "6.6.8": 1, "6.6.10": 1,
	"6.8.1": 1,  "6.8.2": 1,
	"6.10.1": 1, "6.10.2": 1, "6.10.3": 1,
	"6.11.5": 1,
	"6.12.1": 1, "6.12.2": 1, "6.12.3": 1, "6.12.4": 1, "6.12.5": 1, "6.12.6": 1,
	"6.12.7": 1, "6.12.8": 1,
	"6.13.1": 1, "6.13.2": 1, "6.13.3": 1, "6.13.4": 1, "6.13.5": 1,
	"6.14.1": 1, "6.14.2": 1, "6.14.3": 1, "6.14.4": 1, "6.14.5": 1, "6.14.6": 1,
	"6.14.7": 1, "6.14.8": 1, "6.14.9": 1, "6.14.10": 1,
	"6.15.1": 1,
	"6.16.1": 1, "6.16.2": 1, "6.16.3": 1,
	"6.17.1": 1, "6.17.2": 1, "6.17.3": 1, "6.17.4": 1, "6.17.5": 1,
	"6.18.1": 1, "6.18.2": 1, "6.18.3": 1, "6.18.4": 1, "6.18.5": 1,
	"6.19.1": 1, "6.19.2": 1, "6.19.3": 1, "6.19.4": 1, "6.19.5": 1,
	"6.20.1": 1, "6.20.2": 1, "6.20.3": 1, "6.20.4": 1, "6.20.5": 1, "6.20.6": 1,
	"6.20.7": 1,
	"6.21.1": 1, "6.21.2": 1, "6.21.3": 1, "6.21.4": 1, "6.21.5": 1, "6.21.6": 1,
	"6.21.7": 1, "6.21.8": 1,
	"7.5.1": 1
}

def check_results():
	ret = 0

	# Check if file exists
	if not os.path.isfile(file):
		sys.stderr.write("{} do not exist!".format(file))
		return 1

	try:
		with open(file, "r") as json_file:
			json_parsed = json.load(json_file)
	except:
		sys.stderr.write("Cannot read {}!".format(file))
		return 1

	json_parsed = json_parsed["wsServer"]

	for test in json_parsed:
		status = json_parsed[test]["behavior"]

		# We must known that failure, otherwise, we're not passing the tests
		# and we must inform the user.
		if status == "FAILED":
			if not test in np_dict:
				sys.stderr.write("Test {} was not expected to fail!\n".format(test))
				ret = 1
	return ret

print("[+] Checking output...")
sys.stdout.write("Autobahn|Testsuite tests... ")

ret = check_results()

if not ret:
	print("[{}PASSED{}]".format(GREEN, NC))
else:
	print("[{}NOT PASSED{}]".format(RED, NC))

sys.exit(ret)
