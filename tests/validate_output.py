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
file = "wsserver_autobahn/report/index.json"

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
