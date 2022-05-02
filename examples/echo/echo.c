/*
 * Copyright (C) 2016-2021 Davidson Francis <davidsondfgl@gmail.com>
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

/**
 * @dir examples/
 * @brief wsServer examples folder
 */

/*
 * @dir examples/echo
 * @brief Echo example directory.
 * @file echo.c
 * @brief Simple echo example.
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
#ifndef DISABLE_VERBOSE
	printf("Connection opened, addr: %s\n", cli);
#endif
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
#ifndef DISABLE_VERBOSE
	printf("Connection closed, addr: %s\n", cli);
#endif
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
	char *cli;
	cli = ws_getaddress(client);
#ifndef DISABLE_VERBOSE
	printf("I receive a message: %s (size: %" PRId64 ", type: %d), from: %s\n",
		msg, size, type, cli);
#endif

	/**
	 * Mimicks the same frame type received and re-send it again
	 *
	 * Please note that we could just use a ws_sendframe_txt()
	 * or ws_sendframe_bin() here, but we're just being safe
	 * and re-sending the very same frame type and content
	 * again.
	 *
	 * Client equals to NULL: broadcast
	 */
	ws_sendframe(NULL, (char *)msg, size, type);
}

/**
 * @brief Main routine.
 *
 * @note After invoking @ref ws_socket, this routine never returns,
 * unless if invoked from a different thread.
 */
int main(void)
{
	struct ws_events evs;
	evs.onopen    = &onopen;
	evs.onclose   = &onclose;
	evs.onmessage = &onmessage;
	ws_socket(&evs, 8080, 0, 1000); /* Never returns. */

	/*
	 * If you want to execute code past ws_socket, invoke it like:
	 *   ws_socket(&evs, 8080, 1, 1000)
	 */

	return (0);
}
