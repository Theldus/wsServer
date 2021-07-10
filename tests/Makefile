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

WSDIR   = $(CURDIR)/../
EXAMPLE = $(WSDIR)/example/send_receive
JSON    = $(CURDIR)/wsserver_autobahn/index.json

# Conflicts
.PHONY: all run_autobahn check_results clean

# Examples
all: run_autobahn
	$(MAKE) check_results

# Run autobahn
run_autobahn:
	@bash run-autobahn.sh

# Check results
check_results:
	@python validate_output.py

# Clean
clean:
	@rm -rf wsserver_autobahn/report
