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
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* clang-format off */
#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef int socklen_t;
#endif
/* clang-format on */

/* Windows and macOS seems to not have MSG_NOSIGNAL */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#include <unistd.h>

#include <utf8.h>
#include <ws.h>

/**
 * @dir src/
 * @brief wsServer source code
 *
 * @file ws.c
 * @brief wsServer main routines.
 */

/**
 * @brief Websocket events.
 */
static struct ws_events cli_events;

/**
 * @brief Client socks.
 */
struct ws_connection
{
	int client_sock; /**< Client socket FD.        */
	int state;       /**< WebSocket current state. */

	/* Timeout thread and locks. */
	pthread_mutex_t mtx_state;
	pthread_cond_t cnd_state_close;
	pthread_t thrd_tout;
	bool close_thrd;

	/* Send lock. */
	pthread_mutex_t mtx_snd;

	/* IP address. */
	char ip[INET6_ADDRSTRLEN];

	/* Ping/Pong IDs and locks. */
	int32_t last_pong_id;
	int32_t current_ping_id;
	pthread_mutex_t mtx_ping;
};

/**
 * @brief Clients list.
 */
static struct ws_connection client_socks[MAX_CLIENTS];

/**
 * @brief Timeout to a single send().
 */
static uint32_t timeout;

/**
 * @brief Client validity macro
 */
#define CLIENT_VALID(cli)                          \
	((cli) != NULL && (cli) >= &client_socks[0] && \
		(cli) <= &client_socks[MAX_CLIENTS - 1])

/**
 * @brief WebSocket frame data
 */
struct ws_frame_data
{
	/**
	 * @brief Frame read.
	 */
	unsigned char frm[MESSAGE_LENGTH];
	/**
	 * @brief Processed message at the moment.
	 */
	unsigned char *msg;
	/**
	 * @brief Control frame payload
	 */
	unsigned char msg_ctrl[125];
	/**
	 * @brief Current byte position.
	 */
	size_t cur_pos;
	/**
	 * @brief Amount of read bytes.
	 */
	size_t amt_read;
	/**
	 * @brief Frame type, like text or binary.
	 */
	int frame_type;
	/**
	 * @brief Frame size.
	 */
	uint64_t frame_size;
	/**
	 * @brief Error flag, set when a read was not possible.
	 */
	int error;
	/**
	 * @brief Client connection structure.
	 */
	ws_cli_conn_t *client;
};

/**
 * @brief Global mutex.
 */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Issues an error message and aborts the program.
 *
 * @param s Error message.
 */
#define panic(s)   \
	do             \
	{              \
		perror(s); \
		exit(-1);  \
	} while (0);

/**
 * @brief Shutdown and close a given socket.
 *
 * @param fd Socket file descriptor to be closed.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static void close_socket(int fd)
{
#ifndef _WIN32
	shutdown(fd, SHUT_RDWR);
	close(fd);
#else
	closesocket(fd);
#endif
}

/**
 * @brief Returns the current client state for a given
 * client @p client.
 *
 * @param client Client structure.
 *
 * @return Returns the client state, -1 otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int get_client_state(ws_cli_conn_t *client)
{
	int state;

	if (!CLIENT_VALID(client))
		return (-1);

	pthread_mutex_lock(&client->mtx_state);
	state = client->state;
	pthread_mutex_unlock(&client->mtx_state);
	return (state);
}

/**
 * @brief Set a state @p state to the client index
 * @p client.
 *
 * @param client Client structure.
 * @param state State to be set.
 *
 * @return Returns 0 if success, -1 otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int set_client_state(ws_cli_conn_t *client, int state)
{
	if (!CLIENT_VALID(client))
		return (-1);

	if (state < 0 || state > 3)
		return (-1);

	pthread_mutex_lock(&client->mtx_state);
	client->state = state;
	pthread_mutex_unlock(&client->mtx_state);
	return (0);
}

/**
 * @brief Send a given message @p buf on a socket @p sockfd.
 *
 * @param client Target client.
 * @param buf Message to be sent.
 * @param len Message length.
 * @param flags Send flags.
 *
 * @return Returns 0 if success (i.e: all message was sent),
 * -1 otherwise.
 *
 * @note Technically this shouldn't be necessary, since send() should
 * block until all content is sent, since _we_ don't use 'O_NONBLOCK'.
 * However, it was reported (issue #22 on GitHub) that this was
 * happening, so just to be cautious, I will keep using this routine.
 */
static ssize_t send_all(
	ws_cli_conn_t *client, const void *buf, size_t len, int flags)
{
	const char *p;
	ssize_t ret;

	/* Sanity check. */
	if (!CLIENT_VALID(client))
		return (-1);

	p = buf;
	/* clang-format off */
	pthread_mutex_lock(&client->mtx_snd);
		while (len)
		{
			ret = send(client->client_sock, p, len, flags);
			if (ret == -1)
			{
				pthread_mutex_unlock(&client->mtx_snd);
				return (-1);
			}
			p += ret;
			len -= ret;
		}
	pthread_mutex_unlock(&client->mtx_snd);
	/* clang-format on */
	return (0);
}

/**
 * @brief Close client connection (no close handshake, this should
 * be done earlier), set appropriate state and destroy mutexes.
 *
 * @param client Client connection.
 * @param lock Should lock the global mutex?.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static void close_client(ws_cli_conn_t *client, int lock)
{
	if (!CLIENT_VALID(client))
		return;

	set_client_state(client, WS_STATE_CLOSED);

	close_socket(client->client_sock);

	/* Destroy client mutexes and clear fd 'slot'. */
	/* clang-format off */
	if (lock)
		pthread_mutex_lock(&mutex);
			client->client_sock = -1;
			pthread_cond_destroy(&client->cnd_state_close);
			pthread_mutex_destroy(&client->mtx_state);
			pthread_mutex_destroy(&client->mtx_snd);
			pthread_mutex_destroy(&client->mtx_ping);
	if (lock)
		pthread_mutex_unlock(&mutex);
	/* clang-format on */
}

/**
 * @brief Close time-out thread.
 *
 * For a given client, this routine sleeps until
 * TIMEOUT_MS and closes the connection or returns
 * sooner if already closed connection.
 *
 * @param p ws_connection/ws_cli_conn_t Structure Pointer.
 *
 * @return Always NULL.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static void *close_timeout(void *p)
{
	ws_cli_conn_t *conn = p;
	struct timespec ts;
	int state;

	pthread_mutex_lock(&conn->mtx_state);

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_nsec += MS_TO_NS(TIMEOUT_MS);

	/* Normalize the time. */
	while (ts.tv_nsec >= 1000000000)
	{
		ts.tv_sec++;
		ts.tv_nsec -= 1000000000;
	}

	while (conn->state != WS_STATE_CLOSED &&
		   pthread_cond_timedwait(&conn->cnd_state_close, &conn->mtx_state, &ts) !=
			   ETIMEDOUT)
		;

	state = conn->state;
	pthread_mutex_unlock(&conn->mtx_state);

	/* If already closed. */
	if (state == WS_STATE_CLOSED)
		goto quit;

	DEBUG("Timer expired, closing client %d\n", conn->client_sock);

	close_client(conn, 1);
quit:
	return (NULL);
}

/**
 * @brief For a valid client index @p client, starts
 * the timeout thread and set the current state
 * to 'CLOSING'.
 *
 * @param client Client connection.
 *
 * @return Returns 0 if success, -1 otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int start_close_timeout(ws_cli_conn_t *client)
{
	if (!CLIENT_VALID(client))
		return (-1);

	pthread_mutex_lock(&client->mtx_state);

	if (client->state != WS_STATE_OPEN)
		goto out;

	client->state = WS_STATE_CLOSING;

	if (pthread_create(&client->thrd_tout, NULL, close_timeout, client))
	{
		pthread_mutex_unlock(&client->mtx_state);
		panic("Unable to create timeout thread\n");
	}
	client->close_thrd = true;
out:
	pthread_mutex_unlock(&client->mtx_state);
	return (0);
}

/**
 * @brief Sets the IP address relative to a client connection opened
 * by the server and save inside the client structure.
 *
 * @param client Client connection.
 */
static void set_client_address(ws_cli_conn_t *client)
{
	struct sockaddr_in addr;
	socklen_t addr_size;

	if (!CLIENT_VALID(client))
		return;

	addr_size = sizeof(struct sockaddr_in);

	if (getpeername(client->client_sock, (struct sockaddr *)&addr, &addr_size) < 0)
		return;

	memset(client->ip, 0, sizeof(client->ip));
	inet_ntop(AF_INET, &addr.sin_addr, client->ip, INET_ADDRSTRLEN);
}

/**
 * @brief Gets the IP address relative to a client connection opened
 * by the server.
 *
 * @param client Client connection.
 *
 * @return Pointer the ip address, or NULL if fails.
 *
 * @note The returned string is static, no need to free up memory.
 */
char *ws_getaddress(ws_cli_conn_t *client)
{
	if (!CLIENT_VALID(client))
		return (NULL);

	return (client->ip);
}

/**
 * @brief Creates and send an WebSocket frame with some payload data.
 *
 * This routine is intended to be used to create a websocket frame for
 * a given type e sending to the client. For higher level routines,
 * please check @ref ws_sendframe_txt and @ref ws_sendframe_bin.
 *
 * @param client Target to be send. If NULL, broadcast the message.
 * @param msg    Message to be send.
 * @param size   Binary message size.
 * @param type   Frame type.
 *
 * @return Returns the number of bytes written, -1 if error.
 *
 * @note If @p size is -1, it is assumed that a text frame is being sent,
 * otherwise, a binary frame. In the later case, the @p size is used.
 */
int ws_sendframe(ws_cli_conn_t *client, const char *msg, uint64_t size, int type)
{
	unsigned char *response; /* Response data.     */
	unsigned char frame[10]; /* Frame.             */
	uint8_t idx_first_rData; /* Index data.        */
	uint64_t length;         /* Message length.    */
	int idx_response;        /* Index response.    */
	ssize_t output;          /* Bytes sent.        */
	ssize_t send_ret;        /* Ret send function  */
	uint64_t i;              /* Loop index.        */
	ws_cli_conn_t *cli;      /* Client.            */

	frame[0] = (WS_FIN | type);
	length = (uint64_t)size;

	/* Split the size between octets. */
	if (length <= 125)
	{
		frame[1] = length & 0x7F;
		idx_first_rData = 2;
	}

	/* Size between 126 and 65535 bytes. */
	else if (length >= 126 && length <= 65535)
	{
		frame[1] = 126;
		frame[2] = (length >> 8) & 255;
		frame[3] = length & 255;
		idx_first_rData = 4;
	}

	/* More than 65535 bytes. */
	else
	{
		frame[1] = 127;
		frame[2] = (unsigned char)((length >> 56) & 255);
		frame[3] = (unsigned char)((length >> 48) & 255);
		frame[4] = (unsigned char)((length >> 40) & 255);
		frame[5] = (unsigned char)((length >> 32) & 255);
		frame[6] = (unsigned char)((length >> 24) & 255);
		frame[7] = (unsigned char)((length >> 16) & 255);
		frame[8] = (unsigned char)((length >> 8) & 255);
		frame[9] = (unsigned char)(length & 255);
		idx_first_rData = 10;
	}

	/* Add frame bytes. */
	idx_response = 0;
	response = malloc(sizeof(unsigned char) * (idx_first_rData + length + 1));
	if (!response)
		return (-1);

	for (i = 0; i < idx_first_rData; i++)
	{
		response[i] = frame[i];
		idx_response++;
	}

	/* Add data bytes. */
	for (i = 0; i < length; i++)
	{
		response[idx_response] = msg[i];
		idx_response++;
	}

	response[idx_response] = '\0';

	/* Send to the client if there is one. */
	output = 0;
	if (client)
		output = SEND(client, response, idx_response);

	/* If no client specified, broadcast to everyone. */
	if (!client)
	{
		pthread_mutex_lock(&mutex);

		for (i = 0; i < MAX_CLIENTS; i++)
		{
			cli = &client_socks[i];
			if ((cli->client_sock > -1) && get_client_state(cli) == WS_STATE_OPEN)
			{
				if ((send_ret = SEND(cli, response, idx_response)) != -1)
					output += send_ret;
				else
				{
					output = -1;
					break;
				}
			}
		}
		pthread_mutex_unlock(&mutex);
	}

	free(response);
	return ((int)output);
}

/**
 * @brief Given a PONG message, decodes the content
 * as a int32_t number that corresponds to our
 * PONG id.
 *
 * @param msg Content to be decoded.
 *
 * @return Returns the PONG id.
 */
static inline int32_t pong_msg_to_int32(uint8_t *msg)
{
	int32_t pong_id;
	/* Decodes as big-endian. */
	pong_id = (msg[3] << 0) | (msg[2] << 8) | (msg[1] << 16) | (msg[0] << 24);
	return (pong_id);
}

/**
 * @brief Given a PING id, encodes the content to be sent
 * as payload of a PING frame.
 *
 * @param ping_id PING id to be encoded.
 * @param msg Target buffer.
 */
static inline void int32_to_ping_msg(int32_t ping_id, uint8_t *msg)
{
	/* Encodes as big-endian. */
	msg[0] = (ping_id >> 24);
	msg[1] = (ping_id >> 16);
	msg[2] = (ping_id >>  8);
	msg[3] = (ping_id >>  0);
}

/**
 * @brief Send a ping message and close if the client surpasses
 * the threshold imposed.
 *
 * @param cli Client to be sent.
 * @param threshold How many pings can miss?.
 * @param lock Should lock global mutex or not?.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static void send_ping_close(ws_cli_conn_t *cli, int threshold, int lock)
{
	uint8_t ping_msg[4];

	if (!CLIENT_VALID(cli) || get_client_state(cli) != WS_STATE_OPEN)
		return;

	/* clang-format off */
	pthread_mutex_lock(&cli->mtx_ping);

		cli->current_ping_id++;
		int32_to_ping_msg(cli->current_ping_id, ping_msg);

		/* Send PING. */
		ws_sendframe(cli, (const char*)ping_msg, sizeof(ping_msg), WS_FR_OP_PING);

		/* Check previous PONG: if greater than threshold, abort. */
		if ((cli->current_ping_id - cli->last_pong_id) > threshold)
			close_client(cli, lock);

	pthread_mutex_unlock(&cli->mtx_ping);
	/* clang-format on */
}

/**
 * @brief Sends a PING frame to the client @p cli with threshold
 * @p threshold.
 *
 * This routine sends a PING to a single client pointed to by
 * @p cli or a broadcast PING if @p cli is NULL. If the specified
 * client does not respond up to @p threshold PINGs, the connection
 * is aborted.
 *
 * ws_ping() is not automatic: the user who wants to send keep-alive
 * PINGs *must* call this routine in a timely manner, whether on
 * a different thread or inside an event.
 *
 * See examples/ping/ping.c for a minimal example usage.
 *
 * @param cli       Client to be sent, if NULL, broadcast.
 * @param threshold How many ignored PINGs should tolerate? (should be
 *                  positive and greater than 0).
 *
 * @note It should be noted that the time between each call to
 * ws_ping() is the timeout itself for receiving the PONG.
 *
 * It is also important to note that for devices with unstable
 * connections (such as a weak WiFi signal or 3/4/5G from a cell phone),
 * a threshold greater than 1 is advisable.
 */
void ws_ping(ws_cli_conn_t *cli, int threshold)
{
	int i;

	/* Sanity check. */
	if (threshold <= 0)
		return;

	/* PING a single client. */
	if (cli)
		send_ping_close(cli, threshold, 1);

	/* PING broadcast. */
	else
	{
		/* clang-format off */
		pthread_mutex_lock(&mutex);
			for (i = 0; i < MAX_CLIENTS; i++)
				send_ping_close(&client_socks[i], threshold, 0);
		pthread_mutex_unlock(&mutex);
		/* clang-format on */
	}
}

/**
 * @brief Sends a WebSocket text frame.
 *
 * @param client Target to be send. If NULL, broadcast the message.
 * @param msg    Message to be send, null terminated.
 *
 * @return Returns the number of bytes written, -1 if error.
 */
int ws_sendframe_txt(ws_cli_conn_t *client, const char *msg)
{
	return ws_sendframe(client, msg, (uint64_t)strlen(msg), WS_FR_OP_TXT);
}

/**
 * @brief Sends a WebSocket binary frame.
 *
 * @param client Target to be send. If NULL, broadcast the message.
 * @param msg    Message to be send.
 * @param size   Binary message size.
 *
 * @return Returns the number of bytes written, -1 if error.
 */
int ws_sendframe_bin(ws_cli_conn_t *client, const char *msg, uint64_t size)
{
	return ws_sendframe(client, msg, size, WS_FR_OP_BIN);
}

/**
 * @brief For a given @p client, gets the current state for
 * the connection, or -1 if invalid.
 *
 * @param client Client connection.
 *
 * @return Returns the connection state or -1 if
 * invalid @p client.
 *
 * @see WS_STATE_CONNECTING
 * @see WS_STATE_OPEN
 * @see WS_STATE_CLOSING
 * @see WS_STATE_CLOSED
 */
int ws_get_state(ws_cli_conn_t *client)
{
	return (get_client_state(client));
}

/**
 * @brief Close the client connection for the given @p
 * client with normal close code (1000) and no reason
 * string.
 *
 * @param client Client connection.
 *
 * @return Returns 0 on success, -1 otherwise.
 *
 * @note If the client did not send a close frame in
 * TIMEOUT_MS milliseconds, the server will close the
 * connection with error code (1002).
 */
int ws_close_client(ws_cli_conn_t *client)
{
	unsigned char clse_code[2];
	int cc;

	/* Check if client is a valid and connected client. */
	if (!CLIENT_VALID(client) || client->client_sock == -1)
		return (-1);

	/*
	 * Instead of using do_close(), we use this to avoid using
	 * msg_ctrl buffer from wfd and avoid a race condition
	 * if this is invoked asynchronously.
	 */
	cc = WS_CLSE_NORMAL;
	clse_code[0] = (cc >> 8);
	clse_code[1] = (cc & 0xFF);
	if (ws_sendframe(
			client, (const char *)clse_code, sizeof(char) * 2, WS_FR_OP_CLSE) < 0)
	{
		DEBUG("An error has occurred while sending closing frame!\n");
		return (-1);
	}

	/*
	 * Starts the timeout thread: if the client did not send
	 * a close frame in TIMEOUT_MS milliseconds, the server
	 * will close the connection with error code (1002).
	 */
	start_close_timeout(client);
	return (0);
}

/**
 * @brief Checks is a given opcode @p frame
 * belongs to a control frame or not.
 *
 * @param frame Frame opcode to be checked.
 *
 * @return Returns 1 if is a control frame, 0 otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static inline int is_control_frame(int frame)
{
	return (
		frame == WS_FR_OP_CLSE || frame == WS_FR_OP_PING || frame == WS_FR_OP_PONG);
}

/**
 * @brief Do the handshake process.
 *
 * @param wfd Websocket Frame Data.
 *
 * @return Returns 0 if success, a negative number otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int do_handshake(struct ws_frame_data *wfd)
{
	char *response; /* Handshake response message. */
	char *p;        /* Last request line pointer.  */
	ssize_t n;      /* Read/Write bytes.           */

	/* Read the very first client message. */
	if ((n = RECV(wfd->client, wfd->frm, sizeof(wfd->frm) - 1)) < 0)
		return (-1);

	/* Advance our pointers before the first next_byte(). */
	p = strstr((const char *)wfd->frm, "\r\n\r\n");
	if (p == NULL)
	{
		DEBUG("An empty line with \\r\\n was expected!\n");
		return (-1);
	}
	wfd->amt_read = n;
	wfd->cur_pos = (size_t)((ptrdiff_t)(p - (char *)wfd->frm)) + 4;

	/* Get response. */
	if (get_handshake_response((char *)wfd->frm, &response) < 0)
	{
		DEBUG("Cannot get handshake response, request was: %s\n", wfd->frm);
		return (-1);
	}

	/* Valid request. */
	DEBUG("Handshaked, response: \n"
		  "------------------------------------\n"
		  "%s"
		  "------------------------------------\n",
		response);

	/* Send handshake. */
	if (SEND(wfd->client, response, strlen(response)) < 0)
	{
		free(response);
		DEBUG("As error has occurred while handshaking!\n");
		return (-1);
	}

	/* Trigger events and clean up buffers. */
	cli_events.onopen(wfd->client);
	free(response);
	return (0);
}

/**
 * @brief Sends a close frame, accordingly with the @p close_code
 * or the message inside @p wfd.
 *
 * @param wfd Websocket Frame Data.
 * @param close_code Websocket close code.
 *
 * @return Returns 0 if success, a negative number otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int do_close(struct ws_frame_data *wfd, int close_code)
{
	int cc; /* Close code.           */

	/* If custom close-code. */
	if (close_code != -1)
	{
		cc = close_code;
		goto custom_close;
	}

	/* If empty or have a close reason, just re-send. */
	if (wfd->frame_size == 0 || wfd->frame_size > 2)
		goto send;

	/* Parse close code and check if valid, if not, we issue an protocol error.
	 */
	if (wfd->frame_size == 1)
		cc = wfd->msg_ctrl[0];
	else
		cc = ((int)wfd->msg_ctrl[0]) << 8 | wfd->msg_ctrl[1];

	/* Check if it's not valid, if so, we send a protocol error (1002). */
	if ((cc < 1000 || cc > 1003) && (cc < 1007 || cc > 1011) &&
		(cc < 3000 || cc > 4999))
	{
		cc = WS_CLSE_PROTERR;

	custom_close:
		wfd->msg_ctrl[0] = (cc >> 8);
		wfd->msg_ctrl[1] = (cc & 0xFF);

		if (ws_sendframe(wfd->client, (const char *)wfd->msg_ctrl, sizeof(char) * 2,
				WS_FR_OP_CLSE) < 0)
		{
			DEBUG("An error has occurred while sending closing frame!\n");
			return (-1);
		}
		return (0);
	}

	/* Send the data inside wfd->msg_ctrl. */
send:
	if (ws_sendframe(wfd->client, (const char *)wfd->msg_ctrl, wfd->frame_size,
			WS_FR_OP_CLSE) < 0)
	{
		DEBUG("An error has occurred while sending closing frame!\n");
		return (-1);
	}
	return (0);
}

/**
 * @brief Send a pong frame in response to a ping frame.
 *
 * Accordingly to the RFC, a pong frame must have the same
 * data payload as the ping frame, so we just send a
 * ordinary frame with PONG opcode.
 *
 * @param wfd Websocket frame data.
 *
 * @param frame_size Pong frame size.
 *
 * @return Returns 0 if success and a negative number
 * otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int do_pong(struct ws_frame_data *wfd, uint64_t frame_size)
{
	if (ws_sendframe(
			wfd->client, (const char *)wfd->msg_ctrl, frame_size, WS_FR_OP_PONG) < 0)
	{
		wfd->error = 1;
		DEBUG("An error has occurred while ponging!\n");
		return (-1);
	}
	return (0);
}

/**
 * @brief Read a chunk of bytes and return the next byte
 * belonging to the frame.
 *
 * @param wfd Websocket Frame Data.
 *
 * @return Returns the byte read, or -1 if error.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static inline int next_byte(struct ws_frame_data *wfd)
{
	ssize_t n;

	/* If empty or full. */
	if (wfd->cur_pos == 0 || wfd->cur_pos == wfd->amt_read)
	{
		if ((n = RECV(wfd->client, wfd->frm, sizeof(wfd->frm))) <= 0)
		{
			wfd->error = 1;
			DEBUG("An error has occurred while trying to read next byte\n");
			return (-1);
		}
		wfd->amt_read = (size_t)n;
		wfd->cur_pos = 0;
	}
	return (wfd->frm[wfd->cur_pos++]);
}

/**
 * @brief Skips @p frame_size bytes of the current frame.
 *
 * @param wfd Websocket Frame Data.
 * @param frame_size Amount of bytes to be skipped.
 *
 * @return Returns 0 if success, a negative number
 * otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int skip_frame(struct ws_frame_data *wfd, uint64_t frame_size)
{
	uint64_t i;
	for (i = 0; i < frame_size; i++)
	{
		if (next_byte(wfd) == -1)
		{
			wfd->error = 1;
			return (-1);
		}
	}
	return (0);
}

/**
 * @brief Reads the current frame isolating data from control frames.
 * The parameters are changed in order to reflect the current state.
 *
 * @param wfd Websocket Frame Data.
 * @param opcode Frame opcode.
 * @param buf Buffer to be written.
 * @param frame_length Length of the current frame.
 * @param frame_size Total size of the frame (considering CONT frames)
 *                   read until the moment.
 * @param msg_idx Message index, reflects the current buffer pointer state.
 * @param masks Masks vector.
 * @param is_fin Is FIN frame indicator.
 *
 * @return Returns 0 if success, a negative number otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int read_frame(struct ws_frame_data *wfd,
	int opcode,
	unsigned char **buf,
	uint64_t *frame_length,
	uint64_t *frame_size,
	uint64_t *msg_idx,
	uint8_t *masks,
	int is_fin)
{
	unsigned char *tmp; /* Tmp message.     */
	unsigned char *msg; /* Current message. */
	int cur_byte;       /* Curr byte read.  */
	uint64_t i;         /* Loop index.      */

	msg = *buf;

	/* Decode masks and length for 16-bit messages. */
	if (*frame_length == 126)
		*frame_length = (((uint64_t)next_byte(wfd)) << 8) | next_byte(wfd);

	/* 64-bit messages. */
	else if (*frame_length == 127)
	{
		*frame_length =
			(((uint64_t)next_byte(wfd)) << 56) | /* frame[2]. */
			(((uint64_t)next_byte(wfd)) << 48) | /* frame[3]. */
			(((uint64_t)next_byte(wfd)) << 40) | (((uint64_t)next_byte(wfd)) << 32) |
			(((uint64_t)next_byte(wfd)) << 24) | (((uint64_t)next_byte(wfd)) << 16) |
			(((uint64_t)next_byte(wfd)) << 8) |
			(((uint64_t)next_byte(wfd))); /* frame[9]. */
	}

	*frame_size += *frame_length;

	/*
	 * Check frame size
	 *
	 * We need to limit the amount supported here, since if
	 * we follow strictly to the RFC, we have to allow 2^64
	 * bytes. Also keep in mind that this is still true
	 * for continuation frames.
	 */
	if (*frame_size > MAX_FRAME_LENGTH)
	{
		DEBUG("Current frame from client %d, exceeds the maximum\n"
			  "amount of bytes allowed (%" PRId64 "/%d)!",
			wfd->client->client_sock, *frame_size + *frame_length, MAX_FRAME_LENGTH);

		wfd->error = 1;
		return (-1);
	}

	/* Read masks. */
	masks[0] = next_byte(wfd);
	masks[1] = next_byte(wfd);
	masks[2] = next_byte(wfd);
	masks[3] = next_byte(wfd);

	/*
	 * Abort if error.
	 *
	 * This is tricky: we may have multiples error codes from the
	 * previous next_bytes() calls, but, since we're only setting
	 * variables and flags, there is no major issue in setting
	 * them wrong _if_ we do not use their values, thing that
	 * we do here.
	 */
	if (wfd->error)
		return (-1);

	/*
	 * Allocate memory.
	 *
	 * The statement below will allocate a new chunk of memory
	 * if msg is NULL with size total_length. Otherwise, it will
	 * resize the total memory accordingly with the message index
	 * and if the current frame is a FIN frame or not, if so,
	 * increment the size by 1 to accommodate the line ending \0.
	 */
	if (*frame_length > 0)
	{
		if (!is_control_frame(opcode))
		{
			tmp = realloc(
				msg, sizeof(unsigned char) * (*msg_idx + *frame_length + is_fin));
			if (!tmp)
			{
				DEBUG("Cannot allocate memory, requested: % " PRId64 "\n",
					(*msg_idx + *frame_length + is_fin));

				wfd->error = 1;
				return (-1);
			}
			msg = tmp;
			*buf = msg;
		}

		/* Copy to the proper location. */
		for (i = 0; i < *frame_length; i++, (*msg_idx)++)
		{
			/* We were able to read? .*/
			cur_byte = next_byte(wfd);
			if (cur_byte == -1)
				return (-1);

			msg[*msg_idx] = cur_byte ^ masks[i % 4];
		}
	}

	/* If we're inside a FIN frame, lets... */
	if (is_fin && *frame_size > 0)
	{
		/* Increase memory if our FIN frame is of length 0. */
		if (!*frame_length && !is_control_frame(opcode))
		{
			tmp = realloc(msg, sizeof(unsigned char) * (*msg_idx + 1));
			if (!tmp)
			{
				DEBUG("Cannot allocate memory, requested: %" PRId64 "\n",
					(*msg_idx + 1));

				wfd->error = 1;
				return (-1);
			}
			msg = tmp;
			*buf = msg;
		}
		msg[*msg_idx] = '\0';
	}

	return (0);
}

/**
 * @brief Reads the next frame, whether if a TXT/BIN/CLOSE
 * of arbitrary size.
 *
 * @param wfd Websocket Frame Data.
 *
 * @return Returns 0 if success, a negative number otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int next_frame(struct ws_frame_data *wfd)
{
	unsigned char *msg_data; /* Data frame.                */
	unsigned char *msg_ctrl; /* Control frame.             */
	uint8_t masks_data[4];   /* Masks data frame array.    */
	uint8_t masks_ctrl[4];   /* Masks control frame array. */
	uint64_t msg_idx_data;   /* Current msg index.         */
	uint64_t msg_idx_ctrl;   /* Current msg index.         */
	uint64_t frame_length;   /* Frame length.              */
	uint64_t frame_size;     /* Current frame size.        */
	uint32_t utf8_state;     /* Current UTF-8 state.       */
	int32_t pong_id;         /* Current PONG id.           */
	uint8_t opcode;          /* Frame opcode.              */
	uint8_t is_fin;          /* Is FIN frame flag.         */
	uint8_t mask;            /* Mask.                      */
	int cur_byte;            /* Current frame byte.        */

	msg_data = NULL;
	msg_ctrl = wfd->msg_ctrl;
	is_fin = 0;
	frame_length = 0;
	frame_size = 0;
	msg_idx_data = 0;
	msg_idx_ctrl = 0;
	wfd->frame_size = 0;
	wfd->frame_type = -1;
	wfd->msg = NULL;
	utf8_state = UTF8_ACCEPT;

	/* Read until find a FIN or a unsupported frame. */
	do
	{
		/*
		 * Obs: next_byte() can return error if not possible to read the
		 * next frame byte, in this case, we return an error.
		 *
		 * However, please note that this check is only made here and in
		 * the subsequent next_bytes() calls this also may occur too.
		 * wsServer is assuming that the client only create right
		 * frames and we will do not have disconnections while reading
		 * the frame but just when waiting for a frame.
		 */
		cur_byte = next_byte(wfd);
		if (cur_byte == -1)
			return (-1);

		is_fin = (cur_byte & 0xFF) >> WS_FIN_SHIFT;
		opcode = (cur_byte & 0xF);

		/*
		 * Check for RSV field.
		 *
		 * Since wsServer do not negotiate extensions if we receive
		 * a RSV field, we must drop the connection.
		 */
		if (cur_byte & 0x70)
		{
			DEBUG("RSV is set while wsServer do not negotiate extensions!\n");
			wfd->error = 1;
			break;
		}

		/*
		 * Check if the current opcode makes sense:
		 * a) If we're inside a cont frame but no previous data frame
		 *
		 * b) If we're handling a data-frame and receive another data
		 *    frame. (it's expected to receive only CONT or control
		 *    frames).
		 *
		 * It is worth to note that in a), we do not need to check
		 * if the previous frame was FIN or not: if was FIN, an
		 * on_message event was triggered and this function returned;
		 * so the only possibility here is a previous non-FIN data
		 * frame, ;-).
		 */
		if ((wfd->frame_type == -1 && opcode == WS_FR_OP_CONT) ||
			(wfd->frame_type != -1 && !is_control_frame(opcode) &&
				opcode != WS_FR_OP_CONT))
		{
			DEBUG("Unexpected frame was received!, opcode: %d, previous: %d\n",
				opcode, wfd->frame_type);
			wfd->error = 1;
			break;
		}

		/* Check if one of the valid opcodes. */
		if (opcode == WS_FR_OP_TXT || opcode == WS_FR_OP_BIN ||
			opcode == WS_FR_OP_CONT || opcode == WS_FR_OP_PING ||
			opcode == WS_FR_OP_PONG || opcode == WS_FR_OP_CLSE)
		{
			/*
			 * Check our current state: if CLOSING, we only accept close
			 * frames.
			 *
			 * Since the server may, at any time, asynchronously, asks
			 * to close the client connection, we should terminate
			 * immediately.
			 */
			if (get_client_state(wfd->client) == WS_STATE_CLOSING &&
				opcode != WS_FR_OP_CLSE)
			{
				DEBUG("Unexpected frame received, expected CLOSE (%d), "
					  "received: (%d)",
					WS_FR_OP_CLSE, opcode);
				wfd->error = 1;
				break;
			}

			/* Only change frame type if not a CONT frame. */
			if (opcode != WS_FR_OP_CONT && !is_control_frame(opcode))
				wfd->frame_type = opcode;

			mask = next_byte(wfd);
			frame_length = mask & 0x7F;
			frame_size = 0;
			msg_idx_ctrl = 0;

			/*
			 * We should deny non-FIN control frames or that have
			 * more than 125 octets.
			 */
			if (is_control_frame(opcode) && (!is_fin || frame_length > 125))
			{
				DEBUG("Control frame bigger than 125 octets or not a FIN "
					  "frame!\n");
				wfd->error = 1;
				break;
			}

			/* Normal data frames. */
			if (opcode == WS_FR_OP_TXT || opcode == WS_FR_OP_BIN ||
				opcode == WS_FR_OP_CONT)
			{
				if (read_frame(wfd, opcode, &msg_data, &frame_length,
						&wfd->frame_size, &msg_idx_data, masks_data, is_fin) < 0)
					break;

#ifdef VALIDATE_UTF8
				/* UTF-8 Validate partial (or not) frame. */
				if (wfd->frame_type == WS_FR_OP_TXT)
				{
					if (is_fin)
					{
						if (is_utf8_len_state(
								msg_data + (msg_idx_data - frame_length),
								frame_length, utf8_state) != UTF8_ACCEPT)
						{
							DEBUG("Dropping invalid complete message!\n");
							wfd->error = 1;
							do_close(wfd, WS_CLSE_INVUTF8);
						}
					}

					/* Check current state for a CONT or initial TXT frame. */
					else
					{
						utf8_state = is_utf8_len_state(
							msg_data + (msg_idx_data - frame_length), frame_length,
							utf8_state);

						/* We can be in any state, except reject. */
						if (utf8_state == UTF8_REJECT)
						{
							DEBUG("Dropping invalid cont/initial frame!\n");
							wfd->error = 1;
							do_close(wfd, WS_CLSE_INVUTF8);
						}
					}
				}
#endif
			}

			/*
			 * We _may_ send a PING frame if the ws_ping() routine was invoked.
			 *
			 * If the content is invalid and/or differs the size, ignore it.
			 * (maybe unsolicited PONG).
			 */
			else if (opcode == WS_FR_OP_PONG)
			{
				if (read_frame(wfd, opcode, &msg_ctrl, &frame_length, &frame_size,
						&msg_idx_ctrl, masks_ctrl, is_fin) < 0)
					break;

				is_fin = 0;

				/* If there is no content and/or differs the size, ignore it. */
				if (frame_size != sizeof(wfd->client->last_pong_id))
					continue;

				/*
				 * Our PONG id should be positive and smaller than our
				 * current PING id. If not, ignore.
				 */
				/* clang-format off */
				pthread_mutex_lock(&wfd->client->mtx_ping);

					pong_id = pong_msg_to_int32(msg_ctrl);
					if (pong_id < 0 || pong_id > wfd->client->current_ping_id)
					{
						pthread_mutex_unlock(&wfd->client->mtx_ping);
						continue;
					}
					wfd->client->last_pong_id = pong_id;

				pthread_mutex_unlock(&wfd->client->mtx_ping);
				/* clang-format on */
				continue;
			}

			/* We should answer to a PING frame as soon as possible. */
			else if (opcode == WS_FR_OP_PING)
			{
				if (read_frame(wfd, opcode, &msg_ctrl, &frame_length, &frame_size,
						&msg_idx_ctrl, masks_ctrl, is_fin) < 0)
					break;

				if (do_pong(wfd, frame_size) < 0)
					break;

				/* Quick hack to keep our loop. */
				is_fin = 0;
			}

			/* We interrupt the loop as soon as we find a CLOSE frame. */
			else
			{
				if (read_frame(wfd, opcode, &msg_ctrl, &frame_length, &frame_size,
						&msg_idx_ctrl, masks_ctrl, is_fin) < 0)
					break;

#ifdef VALIDATE_UTF8
				/* If there is a close reason, check if it is UTF-8 valid. */
				if (frame_size > 2 && !is_utf8_len(msg_ctrl + 2, frame_size - 2))
				{
					DEBUG("Invalid close frame payload reason! (not UTF-8)\n");
					wfd->error = 1;
					break;
				}
#endif

				/* Since we're aborting, we can scratch the 'data'-related
				 * vars here. */
				wfd->frame_size = frame_size;
				wfd->frame_type = WS_FR_OP_CLSE;
				free(msg_data);
				return (0);
			}
		}

		/* Anything else (unsupported frames). */
		else
		{
			DEBUG("Unsupported frame opcode: %d\n", opcode);
			/* We should consider as error receive an unknown frame. */
			wfd->frame_type = opcode;
			wfd->error = 1;
		}

	} while (!is_fin && !wfd->error);

	/* Check for error. */
	if (wfd->error)
	{
		free(msg_data);
		wfd->msg = NULL;
		return (-1);
	}

	wfd->msg = msg_data;
	return (0);
}

/**
 * @brief Establishes to connection with the client and trigger
 * events when occurs one.
 *
 * @param vclient Client connection.
 *
 * @return Returns @p vclient.
 *
 * @note This will be run on a different thread.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static void *ws_establishconnection(void *vclient)
{
	struct ws_frame_data wfd; /* WebSocket frame data.   */
	ws_cli_conn_t *client;    /* Client structure.       */
	int clse_thrd;            /* Time-out close thread.  */

	client = vclient;

	/* Prepare frame data. */
	memset(&wfd, 0, sizeof(wfd));
	wfd.client = client;

	/* Do handshake. */
	if (do_handshake(&wfd) < 0)
		goto closed;

	/* Change state. */
	set_client_state(client, WS_STATE_OPEN);

	/* Read next frame until client disconnects or an error occur. */
	while (next_frame(&wfd) >= 0)
	{
		/* Text/binary event. */
		if ((wfd.frame_type == WS_FR_OP_TXT || wfd.frame_type == WS_FR_OP_BIN) &&
			!wfd.error)
		{
			cli_events.onmessage(client, wfd.msg, wfd.frame_size, wfd.frame_type);
		}

		/* Close event. */
		else if (wfd.frame_type == WS_FR_OP_CLSE && !wfd.error)
		{
			/*
			 * We only send a CLOSE frame once, if we're already
			 * in CLOSING state, there is no need to send.
			 */
			if (get_client_state(client) != WS_STATE_CLOSING)
			{
				set_client_state(client, WS_STATE_CLOSING);

				/* We only send a close frameSend close frame */
				do_close(&wfd, -1);
			}

			free(wfd.msg);
			break;
		}

		free(wfd.msg);
	}

	/*
	 * on_close events always occur, whether for client closure
	 * or server closure, as the server is expected to
	 * always know when the client disconnects.
	 */
	cli_events.onclose(client);

closed:
	clse_thrd = client->close_thrd;

	/* Wait for timeout thread if necessary. */
	if (clse_thrd)
	{
		pthread_cond_signal(&client->cnd_state_close);
		pthread_join(client->thrd_tout, NULL);
	}

	/* Close connectin properly. */
	if (get_client_state(client) != WS_STATE_CLOSED)
		close_client(client, 1);

	return (vclient);
}

/**
 * @brief Main loop that keeps accepting new connections.
 *
 * @param data Server socket.
 *
 * @return Returns @p data.
 *
 * @note This may be run on a different thread.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static void *ws_accept(void *data)
{
	struct sockaddr_in client; /* Client.                */
	pthread_t client_thread;   /* Client thread.         */
	struct timeval time;       /* Client socket timeout. */
	int new_sock;              /* New opened connection. */
	int sock;                  /* Server sock.           */
	int len;                   /* Length of sockaddr.    */
	int i;                     /* Loop index.            */

	sock = *(int *)data;
	len = sizeof(struct sockaddr_in);

	while (1)
	{
		/* Accept. */
		new_sock = accept(sock, (struct sockaddr *)&client, (socklen_t *)&len);

		if (timeout)
		{
			time.tv_sec = timeout / 1000;
			time.tv_usec = (timeout % 1000) * 1000;

			/*
			 * Socket timeout
			 * This feature seems to be supported on Linux, Windows,
			 * macOS and FreeBSD.
			 *
			 * See:
			 *   https://linux.die.net/man/3/setsockopt
			 */
			setsockopt(new_sock, SOL_SOCKET, SO_SNDTIMEO, &time,
				sizeof(struct timeval));
		}

		if (new_sock < 0)
			panic("Error on accepting connections..");

		/* Adds client socket to socks list. */
		pthread_mutex_lock(&mutex);
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (client_socks[i].client_sock == -1)
			{
				client_socks[i].client_sock = new_sock;
				client_socks[i].state = WS_STATE_CONNECTING;
				client_socks[i].close_thrd = false;
				client_socks[i].last_pong_id = -1;
				client_socks[i].current_ping_id = -1;
				set_client_address(&client_socks[i]);

				if (pthread_mutex_init(&client_socks[i].mtx_state, NULL))
					panic("Error on allocating close mutex");
				if (pthread_cond_init(&client_socks[i].cnd_state_close, NULL))
					panic("Error on allocating condition var\n");
				if (pthread_mutex_init(&client_socks[i].mtx_snd, NULL))
					panic("Error on allocating send mutex");
				if (pthread_mutex_init(&client_socks[i].mtx_ping, NULL))
					panic("Error on allocating ping/pong mutex");
				break;
			}
		}
		pthread_mutex_unlock(&mutex);

		/* Client socket added to socks list ? */
		if (i != MAX_CLIENTS)
		{
			if (pthread_create(
					&client_thread, NULL, ws_establishconnection, &client_socks[i]))
				panic("Could not create the client thread!");

			pthread_detach(client_thread);
		}
		else
			close_socket(new_sock);
	}
	free(data);
	return (data);
}

/**
 * @brief Main loop for the server.
 *
 * @param evs  Events structure.
 * @param port Server port.
 * @param thread_loop If any value other than zero, runs
 *                    the accept loop in another thread
 *                    and immediately returns. If 0, runs
 *                    in the same thread and blocks execution.
 *
 * @param timeout_ms  Max timeout if the client is not responding
 *                    (in milliseconds).
 *
 * @return If @p thread_loop != 0, returns 0. Otherwise, never
 * returns.
 */
int ws_socket(struct ws_events *evs, uint16_t port, int thread_loop,
	uint32_t timeout_ms)
{
	struct sockaddr_in server; /* Server.                */
	pthread_t accept_thread;   /* Accept thread.         */
	int reuse;                 /* Socket option.         */
	int *sock;                 /* Client sock.           */

	timeout = timeout_ms;

	/* Ignore 'unused functions' warnings. */
	((void)skip_frame);

	/* Checks if the event list is a valid pointer. */
	if (evs == NULL)
		panic("Invalid event list!");

	/* Allocates our sock data. */
	sock = malloc(sizeof(*sock));
	if (!sock)
		panic("Unable to allocate sock, out of memory!\n");

	/* Copy events. */
	memcpy(&cli_events, evs, sizeof(struct ws_events));

#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		panic("WSAStartup failed!");

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
	*sock = socket(AF_INET, SOCK_STREAM, 0);
	if (*sock < 0)
		panic("Could not create socket");

	/* Reuse previous address. */
	reuse = 1;
	if (setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse,
			sizeof(reuse)) < 0)
	{
		panic("setsockopt(SO_REUSEADDR) failed");
	}

	/* Prepare the sockaddr_in structure. */
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	/* Bind. */
	if (bind(*sock, (struct sockaddr *)&server, sizeof(server)) < 0)
		panic("Bind failed");

	/* Listen. */
	listen(*sock, MAX_CLIENTS);

	/* Wait for incoming connections. */
	printf("Waiting for incoming connections...\n");
	memset(client_socks, -1, sizeof(client_socks));

	/* Accept connections. */
	if (!thread_loop)
		ws_accept((void *)sock);
	else
	{
		if (pthread_create(&accept_thread, NULL, ws_accept, (void *)sock))
			panic("Could not create the client thread!");
		pthread_detach(accept_thread);
	}

	return (0);
}

#ifdef AFL_FUZZ
/**
 * @brief WebSocket fuzzy test routine
 *
 * @param evs  Events structure.
 *
 * @param file File to be read.
 *
 * @return Returns 0, or crash.
 *
 * @note This is a 'fuzzy version' of the function @ref ws_socket.
 * This routine do not listen to any port nor accept multiples
 * connections. It is intended to read a stream of frames through a
 * file and process it as if they are coming from a socket.
 *
 * This behavior enables us to test wsServer against fuzzers, like
 * AFL, and see if it crashes, hangs or behaves normally, even under
 * weird conditions.
 */
int ws_file(struct ws_events *evs, const char *file)
{
	int sock;
	sock = open(file, O_RDONLY);
	if (sock < 0)
		panic("Invalid file\n");

	/* Copy events. */
	memcpy(&cli_events, evs, sizeof(struct ws_events));

	/* Clear client socks list. */
	memset(client_socks, -1, sizeof(client_socks));

	/* Set client settings. */
	client_socks[0].client_sock = sock;
	client_socks[0].state = WS_STATE_CONNECTING;
	client_socks[0].close_thrd = false;

	/* Initialize mutexes. */
	if (pthread_mutex_init(&client_socks[0].mtx_state, NULL))
		panic("Error on allocating close mutex");
	if (pthread_cond_init(&client_socks[0].cnd_state_close, NULL))
		panic("Error on allocating condition var\n");
	if (pthread_mutex_init(&client_socks[0].mtx_snd, NULL))
		panic("Error on allocating send mutex");
	if (pthread_mutex_init(&client_socks[0].mtx_ping, NULL))
		panic("Error on allocating ping/pong mutex");

	ws_establishconnection(&client_socks[0]);
	return (0);
}
#endif
