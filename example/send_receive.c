/*
 * Copyright (C) 2016-2019 Davidson Francis <davidsondfgl@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <ws.h>

void onopen(int fd)
{
	char *cli;
	cli = ws_getaddress(fd);
	printf("Connection opened, client: %d | addr: %s\n", fd, cli);
	free(cli);
}

void onclose(int fd)
{
	char *cli;
	cli = ws_getaddress(fd);
	printf("Connection closed, client: %d | addr: %s\n", fd, cli);
	free(cli);
}

void onmessage(int fd, const unsigned char *msg)
{
	char *cli;
	cli = ws_getaddress(fd);
	printf("I receive a message: %s, from: %s/%d\n", msg, cli, fd);
	ws_sendframe(fd, (char *)msg, -1, true);
	free(cli);
}

int main()
{
	struct ws_events evs;
	evs.onopen    = &onopen;
	evs.onclose   = &onclose;
	evs.onmessage = &onmessage;
	ws_socket(&evs, 8080);

	return 0;
}
