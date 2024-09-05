/*
 * Copyright (C) 2016-2022  Davidson Francis <davidsondfgl@gmail.com>
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

/**
 * @dir include/
 * @brief wsServer include directory
 *
 * @file ws.h
 * @brief wsServer constants and functions.
 */
#ifndef WS_H
#define WS_H

#ifdef __cplusplus
extern "C" {
#endif

	#include <stdbool.h>
	#include <stdint.h>
	#include <inttypes.h>

	/**
	 * @name Global configurations
	 */
	/**@{*/
	/**
	 * @brief Max clients connected simultaneously.
	 */
#ifndef MAX_CLIENTS
	#define MAX_CLIENTS    8
#endif

	/**
	 * @name Key and message configurations.
	 */
	/**@{*/
	/**
	 * @brief Message buffer length.
	 */
	#define MESSAGE_LENGTH 2048
	/**
	 * @brief Maximum frame/message length.
	 */
	#define MAX_FRAME_LENGTH (16*1024*1024)
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
	#define WS_HS_ACCEPT                       \
		"HTTP/1.1 101 Switching Protocols\r\n" \
		"Upgrade: websocket\r\n"               \
		"Connection: Upgrade\r\n"              \
		"Sec-WebSocket-Accept: "
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
	 * @brief Frame FIN shift
	 */
	#define WS_FIN_SHIFT  7

	/**
	 * @brief Continuation frame.
	 */
	#define WS_FR_OP_CONT 0

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
	 * @brief Ping frame.
	 */
	#define WS_FR_OP_PING 0x9

	/**
	 * @brief Pong frame.
	 */
	#define WS_FR_OP_PONG 0xA

	/**
	 * @brief Unsupported frame.
	 */
	#define WS_FR_OP_UNSUPPORTED 0xF
	/**@}*/

	/**
	 * @name Close codes
	 */
	/**@{*/
	/**
	 * @brief Normal close
	 */
	#define WS_CLSE_NORMAL  1000
	/**
	 * @brief Protocol error
	 */
	#define WS_CLSE_PROTERR 1002
	/**@}*/
	/**
	 * @brief Inconsistent message (invalid utf-8)
	 */
	#define WS_CLSE_INVUTF8 1007

	/**
	 * @name Connection states
	 */
	/**@{*/
	/**
	 * @brief Connection not established yet.
	 */
	#define WS_STATE_CONNECTING 0
	/**
	 * @brief Communicating.
	 */
	#define WS_STATE_OPEN       1
	/**
	 * @brief Closing state.
	 */
	#define WS_STATE_CLOSING    2
	/**
	 * @brief Closed.
	 */
	#define WS_STATE_CLOSED     3
	/**@}*/

	/**
	 * @name Timeout util
	 */
	/**@{*/
	/**
	 * @brief Nanoseconds macro converter
	 */
	#define MS_TO_NS(x) ((x)*1000000)
	/**
	 * @brief Timeout in milliseconds.
	 */
	#define TIMEOUT_MS (500)
	/**@}*/

	/**
	 * @name Handshake constants.
	 */
	/**@{*/
	/**
	 * @brief Debug
	 */
	#ifdef VERBOSE_MODE
	#define DEBUG(...) fprintf(stderr, __VA_ARGS__)
	#else
	#define DEBUG(...)
	#endif
	/**@}*/

	#ifndef AFL_FUZZ
	#define SEND(client,buf,len) send_all((client), (buf), (len), MSG_NOSIGNAL)
	#define RECV(fd,buf,len) recv((fd)->client_sock, (buf), (len), 0)
	#else
	#define SEND(client,buf,len) write(fileno(stdout), (buf), (len))
	#define RECV(fd,buf,len) read((fd)->client_sock, (buf), (len))
	#endif

	/* Opaque client connection type. */
	typedef uint64_t ws_cli_conn_t;

	/* Opaque server instance type. */
	typedef struct ws_server ws_server_t;

	/**
	 * @brief Get server context.
	 * Set when initializing `.context` in `struct ws_server`.
	 */
	void *ws_get_server_context(ws_cli_conn_t client);

	/**
	 * @brief Set connection context.
	 */
	void ws_set_connection_context(ws_cli_conn_t client, void *ptr);

	/**
	 * @brief Get connection context.
	 */
	void *ws_get_connection_context(ws_cli_conn_t client);

	/**
	 * @brief events Web Socket events types.
	 */
	struct ws_events
	{
		/**
		 * @brief On open event, called when a new client connects.
		 */
		void (*onopen)(ws_cli_conn_t client);
		/**
		 * @brief On close event, called when a client disconnects.
		 */
		void (*onclose)(ws_cli_conn_t client);
		/**
		 * @brief On message event, called when a client sends a text
		 * or binary message.
		 */
		void (*onmessage)(ws_cli_conn_t client,
			const unsigned char *msg, uint64_t msg_size, int type);
	};

	/**
	 * @brief server Web Socket server parameters
	 */
	struct ws_server
	{
		/**
		 * @brief Required hostname that the wsServer will bind to
		 */
		const char *host;
		/**
		 * @brief Listening port
		 */
		uint16_t port;
		/**
		 * @brief Whether if the ws_socket() should create a new thread
		 * and be non-blocking (1) or not (0).
		 */
		int thread_loop;
		/**
		 * @brief Ping timeout in milliseconds
		 */
		uint32_t timeout_ms;
		/**
		 * @brief Server events.
		 */
		struct ws_events evs;
		/**
		 * @brief Server context.
		 * Provided by the user, can be accessed via `ws_get_server_context` from `onopen`.
		 */
		void* context;
	};

	/* Forward declarations. */

	/* Internal usage. */
	extern int get_handshake_accept(char *wsKey, unsigned char **dest);
	extern int get_handshake_response(char *hsrequest, char **hsresponse);

	/* External usage. */
	extern char *ws_getaddress(ws_cli_conn_t client);
	extern char *ws_getport(ws_cli_conn_t client);
	extern int ws_sendframe(
		ws_cli_conn_t client, const char *msg, uint64_t size, int type);
	extern int ws_sendframe_bcast(
		uint16_t port, const char *msg, uint64_t size, int type);
	extern int ws_sendframe_txt(ws_cli_conn_t client, const char *msg);
	extern int ws_sendframe_txt_bcast(uint16_t port, const char *msg);
	extern int ws_sendframe_bin(ws_cli_conn_t client, const char *msg,
		uint64_t size);
	extern int ws_sendframe_bin_bcast(uint16_t port, const char *msg,
		uint64_t size);
	extern int ws_get_state(ws_cli_conn_t client);
	extern int ws_close_client(ws_cli_conn_t client);
	extern int ws_socket(struct ws_server *ws_srv);

	/* Ping routines. */
	extern void ws_ping(ws_cli_conn_t cid, int threshold);

#ifdef AFL_FUZZ
	extern int ws_file(struct ws_events *evs, const char *file);
#endif

#ifdef __cplusplus
}
#endif

#endif /* WS_H */
