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

#if !defined(_WIN32)
#error "Expected Windows here!"
#endif

#include <windows.h>
#include "vtouchpad.h"

/**
 * @dir examples/vtouchpad
 * @brief Touchpad example directory
 *
 * @file mouse_win.c
 * @brief Mouse windows implementation.
 */

/**
 * Stub structure.
 */
struct mouse { int stub; };

/**
 * @brief Do nothing, stub.
 *
 * @return Returns always 0.
 */
mouse_t *mouse_new(void)
{
	return (NULL);
}

/**
 * @brief Do nothing, stub.
 *
 * @param mouse Mouse structure pointer.
 *
 * @return Returns always NULL.
 */
void *mouse_free(mouse_t *mouse)
{
	((void)mouse);
	return (NULL);
}

/**
 * @brief Moves the mouse pointed by @p mouse to the
 * offsets @p x_off and @p y_off.
 *
 * @param mouse Mouse structure pointer.
 * @param x_off X-coordinate offset.
 * @param y_off Y-coordinate offset.
 *
 * @return Returns 0 if success, 1 otherwise.
 */
int mouse_move_relative(mouse_t *mouse, int x_off, int y_off)
{
	((void)mouse);
	int ret;
	INPUT input = {0};
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_MOVE;
	input.mi.dx = x_off;
	input.mi.dy = y_off;
	ret = SendInput(1, &input, sizeof(input));
	return (ret == 0);
}

/**
 * @brief Makes a 'button press' event according to the
 * @p mouse pointer and the @p button (left or right).
 *
 * @param mouse Mouse structure pointer.
 * @param button Which button was pressed (either
 *               MOUSE_BTN_LEFT or MOUSE_BTN_RIGHT).
 *
 * @return Returns 0 if success, 1 otherwise.
 */
int mouse_down(mouse_t *mouse, int button)
{
	((void)mouse);
	int ret;
	INPUT input = {0};

	if (button == MOUSE_BTN_LEFT)
		button = MOUSEEVENTF_LEFTDOWN;
	else
		button = MOUSEEVENTF_RIGHTDOWN;

	input.type = INPUT_MOUSE;
	input.mi.dwFlags = button;
	ret = SendInput(1, &input, sizeof(input));
	return (ret == 0);
}

/**
 * @brief Makes a 'button release' event according to the
 * @p mouse pointer and the @p button (left or right).
 *
 * @param mouse Mouse structure pointer.
 * @param button Which button was released (either
 *               MOUSE_BTN_LEFT or MOUSE_BTN_RIGHT).
 *
 * @return Returns 0 if success, 1 otherwise.
 */
int mouse_up(mouse_t *mouse, int button)
{
	((void)mouse);
	int ret;
	INPUT input = {0};

	if (button == MOUSE_BTN_LEFT)
		button = MOUSEEVENTF_LEFTUP;
	else
		button = MOUSEEVENTF_RIGHTUP;

	input.type = INPUT_MOUSE;
	input.mi.dwFlags = button;
	ret = SendInput(1, &input, sizeof(input));
	return (ret == 0);
}
