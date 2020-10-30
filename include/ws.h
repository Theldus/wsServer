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

/**
 * @dir include/
 * @brief wsServer include directory
 *
 * @file ws.h
 * @brief wsServer constants and functions.
 */
#ifndef WS_H
#define WS_H

	#include <stdbool.h>
	#include <stdint.h>

	/**
	 * @name Global configurations
	 */
	/**@{*/
	/**
	 * @brief Max clients connected simultaneously.
	 */
	#define MAX_CLIENTS    8
	
	/**
	 * @brief Max number of `ws_server` instances running
	 * at the same time.
	 */
	#define MAX_PORTS      16
	/**@}*/

	/**
	 * @name Key and message configurations.
	 */
	/**@{*/
	/**
	 * @brief Message buffer length.
	 */
	#define MESSAGE_LENGTH 2048
	/**
	 * @brief WebSocket key length.
	 */
	#define WS_KEY_LEN     24
	/**
	 * @brief Magic string length.
	 */
	#define WS_MS_LEN      36
	/**
	 * @brief Accept message response length.
	 */
	#define WS_KEYMS_LEN   (WS_KEY_LEN + WS_MS_LEN)
	/**
	 * @brief Magic string.
	 */
	#define MAGIC_STRING   "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
	/**@}*/

	/**
	 * @name Handshake constants.
	 */
	/**@{*/
	/**
	 * @brief Alias for 'Sec-WebSocket-Key'.
	 */
	#define WS_HS_REQ      "Sec-WebSocket-Key"
	
	/**
	 * @brief Handshake accept message length.
	 */
	#define WS_HS_ACCLEN   130
	
	/**
	 * @brief Handshake accept message.
	 */
	#define WS_HS_ACCEPT                   \
	"HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n"               \
	"Connection: Upgrade\r\n"              \
	"Sec-WebSocket-Accept: "               \
	/**@}*/

	/**
	 * @name Frame types.
	 */
	/**@{*/
	/**
	 * @brief Frame FIN.
	 */
	#define WS_FIN      128
	
	/**
	 * @brief Text frame.
	 */
	#define WS_FR_OP_TXT  1
	
	/**
	 * @brief Binary frame.
	 */
	#define WS_FR_OP_BIN  2
	
	/**
	 * @brief Close frame.
	 */
	#define WS_FR_OP_CLSE 8
	
	/**
	 * @brief Unsupported frame.
	 */
	#define WS_FR_OP_UNSUPPORTED 0xF
	/**@}*/

	
	/**
	 * @brief events Web Socket events types.
	 */
	struct ws_events
	{
		/**
		 * @brief On open event, called when a new client connects.
		 */
		void (*onopen)(int);

		/**
		 * @brief On close event, called when a client disconnects.
		 */
		void (*onclose)(int);

		/**
		 * @brief On message event, called when a client sends a text
		 * or binary message.
		 */
		void (*onmessage)(int, const unsigned char *);
	};

	/* Forward declarations. */
	extern int get_handshake_accept(char *wsKey, unsigned char **dest);
	extern int get_handshake_response(char *hsrequest, char **hsresponse);
	extern char* ws_getaddress(int fd);
	extern int   ws_sendframe(int fd, const char *msg, int size, bool broadcast);
	extern int   ws_socket(struct ws_events *evs, uint16_t port);

#endif /* WS_H */
