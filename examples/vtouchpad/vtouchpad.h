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

#ifndef VTOUCHPAD_H
#define VTOUCHPAD_H

	/* Debug. */
	#ifndef DISABLE_VERBOSE
	#define VTOUCH_DEBUG(x) printf x
	#else
	#define VTOUCH_DEBUG(x)
	#endif

	/* Mouse definitions. */
	#define MOUSE_MOVE 1
	#define MOUSE_BTN_LEFT_DOWN   2
	#define MOUSE_BTN_LEFT_UP     4
	#define MOUSE_BTN_RIGHT_DOWN  8
	#define MOUSE_BTN_RIGHT_UP   16
	#define EVENT_DELIMITER "; "

	/* Mouse buttons. */
	#define MOUSE_BTN_LEFT  1
	#define MOUSE_BTN_RIGHT 3

	/* Mouse event structure. */
	struct mouse_event
	{
		int event;
		int x_off;
		int y_off;
	};

	/* Opaque type to 'struct mouse'. */
	typedef struct mouse mouse_t;

	/* External declarations. */
	extern mouse_t *mouse_new(void);
	extern void* mouse_free(mouse_t *mouse);
	extern int mouse_move_relative(mouse_t *mouse, int x_off, int y_off);
	extern int mouse_down(mouse_t *mouse, int button);
	extern int mouse_up(mouse_t *mouse, int button);

#endif /* MAIN_H */
