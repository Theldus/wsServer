/*
Copyright (C) 2016  Davidson Francis <davidsondfgl@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>
*/
#ifndef WS_H
#define WS_H

	#include <stdbool.h>
	#include <stdint.h>

	#define MESSAGE_LENGTH 2048
	#define MAX_CLIENTS    8

	#define WS_KEY_LEN     24
	#define WS_MS_LEN      36
	#define WS_KEYMS_LEN   (WS_KEY_LEN + WS_MS_LEN)
	#define MAGIC_STRING   "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

	#define WS_HS_REQ      "Sec-WebSocket-Key"

	#define WS_HS_ACCLEN   130
	#define WS_HS_ACCEPT                   \
	"HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n"               \
	"Connection: Upgrade\r\n"              \
	"Sec-WebSocket-Accept: "               \

	/* Frame definitions. */
	#define WS_FIN    128

	/* Frame types. */
	#define WS_FR_OP_TXT  1
	#define WS_FR_OP_CLSE 8

	#define WS_FR_OP_UNSUPPORTED 0xF

	/* Events. */
	struct ws_events
	{
		/* void onopen(int fd); */
		void (*onopen)(int);

		/* void onclose(int fd); */
		void (*onclose)(int);

		/* void onmessage(int fd, unsigned char *message); */
		void (*onmessage)(int, const unsigned char *);
	};

	extern int getHSaccept(char *wsKey, unsigned char **dest);
	extern int getHSresponse(char *hsrequest, char **hsresponse);

	extern char* ws_getaddress(int fd);
	extern int   ws_sendframe(int fd, const char *msg, bool broadcast);
	extern int   ws_socket(struct ws_events *evs, uint16_t port);

#endif /* WS_H */
