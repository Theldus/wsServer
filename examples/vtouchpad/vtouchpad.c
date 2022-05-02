/*
 * Copyright (C) 2022  Davidson Francis <davidsondfgl@gmail.com>
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ws.h>

#include "vtouchpad.h"

/**
 * @dir examples/vtouchpad
 * @brief Touchpad example directory
 *
 * @file vtouchpad.c
 * @brief Main file.
 */

/* Mouse global structure. */
mouse_t *mouse;

/**
 * @brief Given a string @p ev containing a single event,
 * parses it as expected.
 *
 * The event can be (D stands to 'delimiter', currently ';'):
 * (Mouse Movement): mouse_move D off_X D off_Y, where the offset is an
 *                   integer type that represents the amount of pixels
 *                   the mouse have to move. Example: mouse_move;10;-20.
 *
 *                   Negative offsets represents movement to the left and
 *                   to the top. Positive offsets are the opposite.
 *
 * (Mouse Button Press): Represents if a mouse button has been pressed or
 *                       released.
 *
 * Valid events are:
 * a) mouse_btn_left_down  (left button press)
 * b) mouse_btn_left_up    (left button release)
 * c) mouse_btn_right_down
 * d) mouse_btn_right_up
 *
 * @param ev String representing the event.
 * @param error Set to 1 if an error was found, 0 otherwise.
 *
 * @returns Returns a mouse_event structure.
 */
struct mouse_event parse_event(const char *ev, int *error)
{
	struct mouse_event mev = {0};
	char str[128] = {0};
	char *saveptr, *s;

	*error  = 1;
	saveptr = NULL;
	strncpy(str, ev, sizeof(str) - 1);

	s = strtok_r(str, EVENT_DELIMITER, &saveptr);
	if (!strcmp(s, "mouse_move"))
		mev.event = MOUSE_MOVE;
	else if (!strcmp(s, "mouse_btn_left_down"))
		mev.event = MOUSE_BTN_LEFT_DOWN;
	else if (!strcmp(s, "mouse_btn_left_up"))
		mev.event = MOUSE_BTN_LEFT_UP;
	else if (!strcmp(s, "mouse_btn_right_down"))
		mev.event = MOUSE_BTN_RIGHT_DOWN;
	else if (!strcmp(s, "mouse_btn_right_up"))
		mev.event = MOUSE_BTN_RIGHT_UP;
	else
	{
		fprintf(stderr, "Unknown event: (%s)\n", s);
		goto out0;
	}

	if (mev.event != MOUSE_MOVE)
		goto out1;

	/* X offset. */
	s = strtok_r(NULL, EVENT_DELIMITER, &saveptr);
	if (!s)
	{
		fprintf(stderr, "A movement event requires a X/Y offsets, X not found!\n");
		goto out0;
	}
	mev.x_off = atoi(s);

	/* Y offset. */
	s = strtok_r(NULL, EVENT_DELIMITER, &saveptr);
	if (!s)
	{
		fprintf(stderr, "A movement event requires a X/Y offsets, Y not found!\n");
		goto out0;
	}
	mev.y_off = atoi(s);

out1:
	*error = 0;
out0:
	return (mev);
}

/**
 * @brief On open event, just signals that a new client has
 * been connected.
 *
 * @param client Client connection.
 */
void onopen(ws_cli_conn_t *client) {
	(void)client;
	printf("Connected!\n");
}

/**
 * @brief On close event, just signals that a client has
 * been disconnected.
 *
 * @param client Client connection.
 */
void onclose(ws_cli_conn_t *client) {
	(void)client;
	printf("Disconnected!\n");
}

/**
 * @brief For each new event, parses the string as expected all call
 * the appropriate routines.
 *
 * @param client Client connection. (ignored)
 *
 * @param msg Received message/event.
 *
 * @param size Message size (in bytes). (ignored)
 *
 * @param type Message type. (ignored)
 */
void onmessage(ws_cli_conn_t *client,
    const unsigned char *msg, uint64_t size, int type)
{
	((void)client);
	((void)size);
	((void)type);

	struct mouse_event mev;
	int err;

	/* Parse the event. */
	mev = parse_event((const char *)msg, &err);
	if (err)
		return;

	/* Call the appropriate routine. */
	switch (mev.event)
	{
		case MOUSE_MOVE:
			VTOUCH_DEBUG(("move: %d / %d\n", mev.x_off, mev.y_off));
			mouse_move_relative(mouse, mev.x_off, mev.y_off);
			break;
		case MOUSE_BTN_LEFT_DOWN:
			VTOUCH_DEBUG(("mouse left down\n"));
			mouse_down(mouse, MOUSE_BTN_LEFT);
			break;
		case MOUSE_BTN_LEFT_UP:
			VTOUCH_DEBUG(("mouse left up\n"));
			mouse_up(mouse, MOUSE_BTN_LEFT);
			break;
		case MOUSE_BTN_RIGHT_DOWN:
			VTOUCH_DEBUG(("mouse right down\n"));
			mouse_down(mouse, MOUSE_BTN_RIGHT);
			break;
		case MOUSE_BTN_RIGHT_UP:
			VTOUCH_DEBUG(("mouse right up\n"));
			mouse_up(mouse, MOUSE_BTN_RIGHT);
			break;
	}
}

/* Main routine. */
int main(void)
{
	struct ws_events evs;

	/* Register events. */
	evs.onopen    = &onopen;
	evs.onclose   = &onclose;
	evs.onmessage = &onmessage;

	/* Mouse. */
	mouse = mouse_new();

	ws_socket(&evs, 8080, 0, 1000);

	mouse_free(mouse);

    return (0);
}
