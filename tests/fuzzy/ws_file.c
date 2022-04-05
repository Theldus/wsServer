/*
 * Copyright (C) 2016-2020 Davidson Francis <davidsondfgl@gmail.com>
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ws.h>

#ifndef AFL_FUZZ
#error "This file is intended to be used for fuzzing"
#error "please enable -DAFL_FUZZ on CFLAGS"
#endif

/**
 * @dir tests/
 * @brief wsServer tests folder
 *
 * @file ws_file.c
 * @brief Read a given stream of frames from a file and parse them.
 */

/**
 * @brief Called when a client connects to the server.
 *
 * @param client Client connection. The @p client parameter is used
 * in order to send messages and retrieve informations about the
 * client.
 */
void onopen(ws_cli_conn_t *client)
{
	char *cli;
	cli = ws_getaddress(client);
	printf("Connection opened, addr: %s\n", cli);
}

/**
 * @brief Called when a client disconnects to the server.
 *
 * @param client Client connection. The @p client parameter is used
 * in order to send messages and retrieve informations about the
 * client.
 */
void onclose(ws_cli_conn_t *client)
{
	char *cli;
	cli = ws_getaddress(client);
	printf("Connection closed, addr: %s\n", cli);
}

/**
 * @brief Called when a client connects to the server.
 *
 * @param client Client connection. The @p client parameter is used
 * in order to send messages and retrieve informations about the
 * client.
 *
 * @param msg Received message, this message can be a text
 * or binary message.
 *
 * @param size Message size (in bytes).
 *
 * @param type Message type.
 */
void onmessage(ws_cli_conn_t *client,
	const unsigned char *msg, uint64_t size, int type)
{
	printf("I receive a message: (%.*s) (size: %" PRId64 ", type: %d)\n",
		(int)size, msg, size, type);
	ws_sendframe(NULL, (char *)msg, size, type);
}

/**
 * @brief Main routine.
 */
int main(int argc, char **argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "Please: %s <file>\n", argv[0]);
		return (-1);
	}

	struct ws_events evs;
	evs.onopen    = &onopen;
	evs.onclose   = &onclose;
	evs.onmessage = &onmessage;
	ws_file(&evs, argv[1]);
	return (0);
}
