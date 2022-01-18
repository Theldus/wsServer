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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.	 If not, see <http://www.gnu.org/licenses/>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <inttypes.h>
#include <unistd.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef unsigned long in_addr_t;
#endif

/* Windows and macOS seems to not have MSG_NOSIGNAL */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#include "toyws.h"

/*
 * This is a WebSocket 'toy client', made exclusively to work with wsServer.
 *
 * Although it is independent of the wsServer src tree, it is highly limited
 * and therefore not recommended for use with other servers.
 *
 * There are the following restrictions (not limited to):
 * - Fixed handshake header
 *
 * - Fixed frame mask (ideally it should be random)
 *
 * - No PING/PONG frame support
 *
 * - No close handshake support: although it can identify CLOSE frames, it
 *   does not send the response, it just aborts the connection.
 *
 * - No support for CONT frames, that is, the entire content of a frame (TXT
 *   or BIN) must be contained in a single message.
 *
 * - Possibly other things too.
 *
 * Other than that, this client supports sending and receiving frames and
 * should work 'ok' with wsServer. Since there was some demand for client
 * support, this mini-project is a response to those requests =).
*/

/* Dummy/constant request. */
static const char request[] =
	"GET / HTTP/1.1\r\n"
	"Host: localhost:8080\r\n"
	"Connection: Upgrade\r\n"
	"Upgrade: websocket\r\n"
	"Sec-WebSocket-Version: 13\r\n"
	"Sec-WebSocket-Key: uaGPoPbZRzHcWDXiNQ5dyg==\r\n\r\n";

/**
 * @brief Connect to a given @p ip address and @p port.
 *
 * @param ctx WebSocket client context.
 * @param ip Target IP address to connect.
 * @param port Server port.
 *
 * @return Returns the socket fd if success, otherwise, returns
 * a negative number.
 */
int tws_connect(struct tws_ctx *ctx, const char *ip, uint16_t port)
{
	char *p;
	int sock;
	ssize_t ret;
	in_addr_t ip_addr;
	struct sockaddr_in sock_addr;

	memset(ctx, 0, sizeof(*ctx));

#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		fprintf(stderr, "WSAStartup failed!");
		return (-1);
	}

	/**
	 * Sets stdout to be non-buffered.
	 *
	 * According to the docs from MSDN (setvbuf page), Windows do not
	 * really supports line buffering but full-buffering instead.
	 *
	 * Quote from the docs:
	 * "... _IOLBF For some systems, this provides line buffering.
	 *  However, for Win32, the behavior is the same as _IOFBF"
	 */
	setvbuf(stdout, NULL, _IONBF, 0);
#endif

	/* Create socket. */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return (-1);

	memset((void*)&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;

	if ((ip_addr = inet_addr(ip)) == INADDR_NONE)
		return (-1);

	sock_addr.sin_addr.s_addr = ip_addr;
	sock_addr.sin_port = htons(port);

	/* Connect. */
	if (connect(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0)
		return (-1);

	/* Do handhshake. */
	if (send(sock, request, strlen(request), MSG_NOSIGNAL) < 0)
		return (-1);

	/* Wait for 'switching protocols'. */
	if ((ret = recv(sock, ctx->frm, sizeof(ctx->frm), 0)) < 0)
		return (-1);

	/* Advance our pointers before the first next_byte(). */
	p = strstr((const char *)ctx->frm, "\r\n\r\n");
	ctx->amt_read = ret;
	ctx->cur_pos  = (size_t)((ptrdiff_t)(p - (char *)ctx->frm)) + 4;
	ctx->fd       = sock;
	ctx->status   = TWS_ST_CONNECTED;

	return (sock);
}

/**
 * @brief Close the connection for the given @p ctx.
 *
 * @param ctx WebSocket client context.
 */
void tws_close(struct tws_ctx *ctx)
{
	if (ctx->status == TWS_ST_DISCONNECTED)
		return;
#ifndef _WIN32
	shutdown(ctx->fd, SHUT_RDWR);
	close(ctx->fd);
#else
	closesocket(ctx->fd);
	WSACleanup();
#endif
	ctx->status = TWS_ST_DISCONNECTED;
}

/**
 * @brief Send a frame of type @p type with content @p msg and
 * size @p size for a given context @p ctx.
 *
 * @param ctx WebSocket client context.
 * @param msg Frame message.
 * @param size Frame size.
 * @param type Frame type (e.g: FRM_TXT, FRM_BIN...)
 *
 * @return Returns 0 if success, a negative number otherwise.
 */
int tws_sendframe(struct tws_ctx *ctx, uint8_t *msg, uint64_t size,
	int type)
{
	uint8_t frame[10] = {0};
	uint8_t masks[4];
	uint64_t length;
	uint8_t hdr_len;
	size_t count;
	uint8_t *tmp;
	uint8_t *p;

	frame[0]  = FRM_FIN | type;
	frame[1] |= FRM_MSK;
	length	  = (uint64_t)size;

	/* Split the size between octets. */
	if (length <= 125)
	{
		frame[1] |= length & 0x7F;
		hdr_len = 2;
	}

	/* Size between 126 and 65535 bytes. */
	else if (length >= 126 && length <= 65535)
	{
		frame[1] |= 126;
		frame[2]  = (length >> 8) & 255;
		frame[3]  = length & 255;
		hdr_len = 4;
	}

	/* More than 65535 bytes. */
	else
	{
		frame[1] |= 127;
		frame[2]  = (uint8_t)((length >> 56) & 255);
		frame[3]  = (uint8_t)((length >> 48) & 255);
		frame[4]  = (uint8_t)((length >> 40) & 255);
		frame[5]  = (uint8_t)((length >> 32) & 255);
		frame[6]  = (uint8_t)((length >> 24) & 255);
		frame[7]  = (uint8_t)((length >> 16) & 255);
		frame[8]  = (uint8_t)((length >> 8) & 255);
		frame[9]  = (uint8_t)(length & 255);
		hdr_len = 10;
	}

	/* Send header. */
	if (send(ctx->fd, frame, hdr_len, MSG_NOSIGNAL) < 0)
		return (-1);

	/* Send dummy masks. */
	masks[0] = masks[1] = masks[2] = masks[3] = 0xAA;
	if (send(ctx->fd, masks, 4, MSG_NOSIGNAL) < 0)
		return (-2);

	/* Mask message and send it. */
	p = calloc(1, size);
	if (!p)
		return (-3);

	memcpy(p, msg, size);

	for (tmp = p, count = 0; count < size; tmp++, count++)
		*tmp ^= masks[count % 4];

	if (send(ctx->fd, p, size, MSG_NOSIGNAL) < 0)
	{
		free(p);
		return (-4);
	}

	free(p);
	return (0);
}

/**
 * @brief Read a chunk of bytes and return the next byte
 * belonging to the frame.
 *
 * @param ctx Websocket Context.
 * @param ret Optional return code.
 *
 * @return Returns the byte read, or -1 if error.
 */
static inline int next_byte(struct tws_ctx *ctx, int *ret)
{
	ssize_t n;

	/* If empty or full. */
	if (ctx->cur_pos == 0 || ctx->cur_pos == ctx->amt_read)
	{
		if ((n = recv(ctx->fd, ctx->frm, sizeof(ctx->frm), 0)) <= 0)
		{
			if (ret)
				*ret = 1;
			return (-1);
		}

		ctx->amt_read = (size_t)n;
		ctx->cur_pos  = 0;
	}
	return (ctx->frm[ctx->cur_pos++]);
}

/**
 * @brief Skips @p frame_size bytes of the current frame.
 *
 * @param ctx Websocket Context.
 * @param frame_size Amount of bytes to be skipped.
 *
 * @return Returns 0 if success, a negative number
 * otherwise.
 */
static int skip_frame(struct tws_ctx *ctx, uint64_t frame_size)
{
	uint64_t i;
	for (i = 0; i < frame_size; i++)
		if (next_byte(ctx, NULL) == -1)
			return (-1);
	return (0);
}

/**
 * @brief Receive a frame and save it on @p buff.
 *
 * If @p buff is NULL, this function will allocate a new
 * buffer and save the new size in @p buff_size. If not
 * NULL and if @p buff_size is greater than the frame,
 * the buffer is used, otherwise, the address will be
 * reallocated.
 *
 * @param ctx WebSocket Context.
 * @param buff Buffer pointer.
 * @param buff_size Buffer size.
 * @param frm_type Frame type received.
 *
 * @return Returns 0 if success, a negative number
 * otherwise.
 */
int tws_receiveframe(struct tws_ctx *ctx, char **buff,
	size_t *buff_size, int *frm_type)
{
	int ret;
	char *buf;
	uint64_t i;
	int cur_byte;
	uint8_t opcode;
	uint64_t frame_length;

	/* Buffer should be valid. */
	if (!buff || (*buff && !buff_size))
		return (-1);

	ret = 0;
	cur_byte = next_byte(ctx, &ret);
	opcode = (cur_byte & 0xF);

	/* If CLOSE, lets close, abruptly, because why not?. */
	if (opcode == FRM_CLSE)
	{
		tws_close(ctx);
		return (-1);
	}

	frame_length = next_byte(ctx, &ret) & 0x7F;

	/* Read remaining length bytes, if any. */
	if (frame_length == 126)
	{
		frame_length = (((uint64_t)next_byte(ctx, &ret)) << 8) |
			next_byte(ctx, &ret);
	}

	else if (frame_length == 127)
	{
		frame_length =
			(((uint64_t)next_byte(ctx, &ret)) << 56) | /* frame[2]. */
			(((uint64_t)next_byte(ctx, &ret)) << 48) | /* frame[3]. */
			(((uint64_t)next_byte(ctx, &ret)) << 40) |
			(((uint64_t)next_byte(ctx, &ret)) << 32) |
			(((uint64_t)next_byte(ctx, &ret)) << 24) |
			(((uint64_t)next_byte(ctx, &ret)) << 16) |
			(((uint64_t)next_byte(ctx, &ret)) <<  8) |
			(((uint64_t)next_byte(ctx, &ret))); /* frame[9]. */
	}

	/* Check if any error before proceed. */
	if (ret)
		return (ret);

	/*
	 * If frame is something other than CLSE and TXT/BIN (like PONG
	 * or CONT), skip.
	 */
	if (opcode != FRM_TXT && opcode != FRM_BIN)
		if (skip_frame(ctx, frame_length) < 0)
			return (-1);

	/* Allocate memory, if needed. */
	if (*buff_size < frame_length)
	{
		buf = realloc(*buff, frame_length + 1);
		if (!buf)
			return (-1);
		*buff = buf;
		*buff_size = frame_length + 1;
	}
	else
		buf = *buff;

	/* Receive frame. */
	for (i = 0; i < frame_length; i++, buf++)
	{
		cur_byte = next_byte(ctx, &ret);
		if (cur_byte < 0)
			return (ret);

		*buf = cur_byte;
	}
	*buf = '\0';

	/* Fill other infos. */
	*frm_type = opcode;

	return (ret);
}
