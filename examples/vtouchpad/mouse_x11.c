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

#if !defined(__linux__) && !defined(__FreeBSD__)
#error "Expected Linux or FreeBSD here!"
#endif

#include <X11/Xlib.h>
#ifdef HAVE_XDOTOOL
	#include <xdo.h>
#else
	#include <X11/extensions/XTest.h>
#endif

#include "vtouchpad.h"

/**
 * @dir examples/vtouchpad
 * @brief Touchpad example directory
 *
 * @file mouse_x11.c
 * @brief Mouse X11 implementation.
 */

/**
 * Note:
 * Although the use of libxdo/xdotool seems unnecessary (since libxdo *also*
 * uses X11's XTest extension), I believe it can still be interesting: the
 * xdotool implementation may change in the future, as well as support new
 * systems, so dealing with libxdo seems to be better than using X11 directly.
 *
 * Link:
 *   If   libxdo present: -lX11 -lxdo
 *   Else               : -lX11 -lXtst
 *
 */

/**
 * Mouse pointer, hold OS-dependent data structures.
 */
struct mouse
{
#ifdef HAVE_XDOTOOL
	xdo_t *xdo;
#else
	Display *dpy;
	Window root;
#endif
};

/**
 * @brief Allocates a new mouse data structure.
 *
 * @return Returns a new mouse_t pointer object.
 */
mouse_t *mouse_new(void)
{
	struct mouse *mouse;
	mouse = calloc(1, sizeof(struct mouse));
	if (!mouse)
		return (NULL);
#ifdef HAVE_XDOTOOL
	mouse->xdo  = xdo_new(NULL);
	if (!mouse->xdo)
		return mouse_free(mouse);
#else
	mouse->dpy  = XOpenDisplay(0);
	if (!mouse->dpy)
		return mouse_free(mouse);
	mouse->root = XRootWindow(mouse->dpy, DefaultScreen(mouse->dpy));
#endif
	return (mouse);
}

/**
 * @brief Frees a previously allocated mouse_t pointer.
 *
 * @return Always NULL.
 */
void *mouse_free(mouse_t *mouse)
{
	if (!mouse)
		goto out;
#ifdef HAVE_XDOTOOL
	if (mouse->xdo)
		xdo_free(mouse->xdo);
#else
	if (mouse->dpy)
		XCloseDisplay(mouse->dpy);
#endif
	free(mouse);
out:
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
#ifdef HAVE_XDOTOOL
	return xdo_move_mouse_relative(mouse->xdo, x_off, y_off);
#else
	int ret;
	ret = XTestFakeRelativeMotionEvent(mouse->dpy, x_off, y_off, CurrentTime);
	XFlush(mouse->dpy);
	return (ret == 0);
#endif
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
#ifdef HAVE_XDOTOOL
	return xdo_mouse_down(mouse->xdo, CURRENTWINDOW, button);
#else
	int ret;
	ret = XTestFakeButtonEvent(mouse->dpy, button, 1, CurrentTime);
    XFlush(mouse->dpy);
    return (ret == 0);
#endif
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
#ifdef HAVE_XDOTOOL
	return xdo_mouse_up(mouse->xdo, CURRENTWINDOW, button);
#else
	int ret;
	ret = XTestFakeButtonEvent(mouse->dpy, button, 0, CurrentTime);
    XFlush(mouse->dpy);
    return (ret == 0);
#endif
}
