/*
 * Copyright (C) 2016-2023  Davidson Francis <davidsondfgl@gmail.com>
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
#include <netdb.h>
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
 * @brief Client socks.
 */
struct ws_connection
{
	int client_sock; /**< Client socket FD.        */
	int state;       /**< WebSocket current state. */

	/* wsServer structure copy. */
	struct ws_server ws_srv;

	/* Timeout thread and locks. */
	pthread_mutex_t mtx_state;
	pthread_cond_t cnd_state_close;
	pthread_t thrd_tout;
	bool close_thrd;

	/* Send lock. */
	pthread_mutex_t mtx_snd;

	/* IP address and port. */
	char ip[1025]; /* NI_MAXHOST. */
	char port[32]; /* NI_MAXSERV. */

	/* Ping/Pong IDs and locks. */
	int32_t last_pong_id;
	int32_t current_ping_id;
	pthread_mutex_t mtx_ping;

	/* Connection context */
	void *connection_context;

	ws_cli_conn_t client_id;
};

static struct ws_connection *get_client_by_cid(ws_cli_conn_t cid);

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
		(cli) <= &client_socks[MAX_CLIENTS - 1] && \
		(cli)->client_sock > -1)


/**
 * @brief Get server context.
 * Assumed to be set once, when initializing `.context` in `struct ws_server`.
 */
void *ws_get_server_context(ws_cli_conn_t cli)
{
	struct ws_connection *client = get_client_by_cid(cli);
	if (!CLIENT_VALID(client))
		return NULL;
	return client->ws_srv.context;
}

/**
 * @brief Set connection context.
 */
void ws_set_connection_context(ws_cli_conn_t client, void *ptr)
{
	struct ws_connection *cli = get_client_by_cid(client);
	if (!CLIENT_VALID(cli))
		return;
	cli->connection_context = ptr;
}

/**
 * @brief Get connection context.
 */
void *ws_get_connection_context(ws_cli_conn_t client)
{
	struct ws_connection *cli = get_client_by_cid(client);
	if (!CLIENT_VALID(cli))
		return NULL;
	return cli->connection_context;
}

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
	struct ws_connection *client;
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

static struct ws_connection *get_client_by_cid(ws_cli_conn_t cid)
{
	pthread_mutex_lock(&mutex);
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (client_socks[i].client_id == cid)
		{
			pthread_mutex_unlock(&mutex);
			return &client_socks[i];
		}
	}
	pthread_mutex_unlock(&mutex);
	return NULL;
}
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


static uint64_t cid_generator = 1;

static pthread_mutex_t cid_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint64_t get_next_cid()
{
	uint64_t next_cid;

	pthread_mutex_lock(&cid_mutex);
	next_cid = cid_generator++;
	pthread_mutex_unlock(&cid_mutex);

	return next_cid;
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
static int get_client_state(struct ws_connection *client)
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
static int set_client_state(struct ws_connection *client, int state)
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
 * @return If success (i.e: all message was sent), returns
 * the amount of bytes sent. Otherwise, -1.
 *
 * @note Technically this shouldn't be necessary, since send() should
 * block until all content is sent, since _we_ don't use 'O_NONBLOCK'.
 * However, it was reported (issue #22 on GitHub) that this was
 * happening, so just to be cautious, I will keep using this routine.
 */
static ssize_t send_all(
	struct ws_connection *client, const void *buf, size_t len, int flags)
{
	const char *p;
	ssize_t ret;
	ssize_t r;

	ret = 0;

	/* Sanity check. */
	if (!CLIENT_VALID(client))
		return (-1);

	p = buf;
	/* clang-format off */
	pthread_mutex_lock(&client->mtx_snd);
		while (len)
		{
			r = send(client->client_sock, p, len, flags);
			if (r == -1)
			{
				pthread_mutex_unlock(&client->mtx_snd);
				return (-1);
			}
			p   += r;
			len -= r;
			ret += r;
		}
	pthread_mutex_unlock(&client->mtx_snd);
	/* clang-format on */
	return (ret);
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
static void close_client(struct ws_connection *client, int lock)
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
	struct ws_connection *conn = p;
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
static int start_close_timeout(struct ws_connection *client)
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
static void set_client_address(struct ws_connection *client)
{
	struct sockaddr_storage addr;
	socklen_t hlen = sizeof(addr);

	if (!CLIENT_VALID(client))
		return;

	memset(client->ip,   0, sizeof(client->ip));
	memset(client->port, 0, sizeof(client->port));

	if (getpeername(client->client_sock, (struct sockaddr *)&addr, &hlen) < 0)
		return;

	getnameinfo((struct sockaddr *)&addr, hlen,
		client->ip,   sizeof(client->ip),
		client->port, sizeof(client->port),
		NI_NUMERICHOST|NI_NUMERICSERV);
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
char *ws_getaddress(ws_cli_conn_t client)
{
	struct ws_connection *cli = get_client_by_cid(client);
	if (!CLIENT_VALID(cli))
		return (NULL);

	return (cli->ip);
}

/**
 * @brief Gets the IP port relative to a client connection opened
 * by the server.
 *
 * @param client Client connection.
 *
 * @return Pointer the port, or NULL if fails.
 *
 * @note The returned string is static, no need to free up memory.
 */
char *ws_getport(ws_cli_conn_t client)
{
	struct ws_connection *cli = get_client_by_cid(client);
	if (!CLIENT_VALID(cli))
		return (NULL);

	return (cli->port);
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
 * @param port   Server listen port to broadcast message (if any).
 *
 * @return Returns the number of bytes written, -1 if error.
 *
 * @note If @p size is -1, it is assumed that a text frame is being sent,
 * otherwise, a binary frame. In the later case, the @p size is used.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int ws_sendframe_internal(struct ws_connection *client, const char *msg,
	uint64_t size, int type, uint16_t port)
{
	unsigned char *response;   /* Response data.     */
	unsigned char frame[10];   /* Frame.             */
	uint8_t idx_first_rData;   /* Index data.        */
	struct ws_connection *cli; /* Client.            */
	int idx_response;          /* Index response.    */
	ssize_t send_ret;          /* Ret send function  */
	uint64_t length;           /* Message length.    */
	ssize_t output;            /* Bytes sent.        */
	uint64_t i;                /* Loop index.        */

	/*
	 * Check if there is a valid condition before proceeding.
	 *
	 * Valid ones:
	 * client == true  && port == 0
	 *     -> send to single client
	 *
	 * client == false && port != 0
	 *      -> send to all clients within single port
	 *
	 * Other options are invalid!
	 */

	if (client)
	{
		if (port != 0)
			return (-1);
	}
	else
	{
		if (port == 0)
			return (-1);
	}

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
	if (client && port == 0)
	{
		output = SEND(client, response, idx_response);
		goto skip_broadcast;
	}

	/* clang-format off */
	pthread_mutex_lock(&mutex);
		/* Do broadcast. */
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			cli = &client_socks[i];

			if ((cli->client_sock > -1) &&
				get_client_state(cli) == WS_STATE_OPEN &&
				(cli->ws_srv.port == port))
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
	/* clang-format on */

skip_broadcast:
	free(response);
	return ((int)output);
}

/**
 * @brief Send an WebSocket frame with some payload data.
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
int ws_sendframe(ws_cli_conn_t client, const char *msg, uint64_t size, int type)
{
	struct ws_connection *cli = get_client_by_cid(client);
	if (!CLIENT_VALID(cli))
		return (-1);
	return ws_sendframe_internal(cli, msg, size, type, 0);
}

/**
 * @brief Send an WebSocket frame with some payload data to all clients
 * connected into the same port.
 *
 * @param port   Server listen port to broadcast message.
 * @param msg    Message to be send.
 * @param size   Binary message size.
 * @param type   Frame type.
 *
 * @return Returns the number of bytes written, -1 if error.
 *
 * @note If @p size is -1, it is assumed that a text frame is being sent,
 * otherwise, a binary frame. In the later case, the @p size is used.
 */
int ws_sendframe_bcast(uint16_t port, const char *msg, uint64_t size, int type)
{
	return ws_sendframe_internal(NULL, msg, size, type, port);
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
static void send_ping_close(struct ws_connection *cli, int threshold, int lock)
{
	uint8_t ping_msg[4];

	if (!CLIENT_VALID(cli) || get_client_state(cli) != WS_STATE_OPEN)
		return;

	/* clang-format off */
	pthread_mutex_lock(&cli->mtx_ping);

		cli->current_ping_id++;
		int32_to_ping_msg(cli->current_ping_id, ping_msg);

		/* Send PING. */
		ws_sendframe_internal(cli, (const char*)ping_msg, sizeof(ping_msg),
			WS_FR_OP_PING, 0);

		/* Check previous PONG: if greater than threshold, abort. */
		if ((cli->current_ping_id - cli->last_pong_id) > threshold) {
			DEBUG("Closing, reason: many unanswered PINGs\n");
			close_client(cli, lock);
		}

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
void ws_ping(ws_cli_conn_t client, int threshold)
{
	struct ws_connection *cli = get_client_by_cid(client);
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
 * @param client Target to be send.
 * @param msg    Message to be send, null terminated.
 *
 * @return Returns the number of bytes written, -1 if error.
 */
int ws_sendframe_txt(ws_cli_conn_t client, const char *msg)
{
	return ws_sendframe(client, msg, (uint64_t)strlen(msg), WS_FR_OP_TXT);
}

/**
 * @brief Sends a broadcast WebSocket text frame.
 *
 * @param port Server listen port to broadcast message.
 * @param msg  Message to be send, null terminated.
 *
 * @return Returns the number of bytes written, -1 if error.
 */
int ws_sendframe_txt_bcast(uint16_t port, const char *msg)
{
	return ws_sendframe_bcast(port, msg, (uint64_t)strlen(msg), WS_FR_OP_TXT);
}

/**
 * @brief Sends a WebSocket binary frame.
 *
 * @param client Target to be send.
 * @param msg    Message to be send.
 * @param size   Binary message size.
 *
 * @return Returns the number of bytes written, -1 if error.
 */
int ws_sendframe_bin(ws_cli_conn_t client, const char *msg, uint64_t size)
{
	return ws_sendframe(client, msg, size, WS_FR_OP_BIN);
}

/**
 * @brief Sends a broadcast WebSocket binary frame.
 *
 * @param port Server listen port to broadcast message.
 * @param msg  Message to be send.
 * @param size Binary message size.
 *
 * @return Returns the number of bytes written, -1 if error.
 */
int ws_sendframe_bin_bcast(uint16_t port, const char *msg, uint64_t size)
{
	return ws_sendframe_bcast(port, msg, size, WS_FR_OP_BIN);
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
int ws_get_state(ws_cli_conn_t client)
{
	struct ws_connection *cli = get_client_by_cid(client);
	if (!CLIENT_VALID(cli))
		return -1;
	return (get_client_state(cli));
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
int ws_close_client(ws_cli_conn_t client)
{
	struct ws_connection *cli = get_client_by_cid(client);

	unsigned char clse_code[2];
	int cc;

	/* Check if client is a valid and connected client. */
	if (!CLIENT_VALID(cli) || cli->client_sock == -1)
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
	start_close_timeout(cli);
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
 * @brief Checks is a given opcode @p opcode is valid or not.
 *
 * @param opcode Frame opcode to be checked.
 *
 * @return Returns 1 if valid, 0 otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static inline int is_valid_frame(int opcode)
{
	return (
		opcode == WS_FR_OP_TXT  || opcode == WS_FR_OP_BIN  ||
		opcode == WS_FR_OP_CONT || opcode == WS_FR_OP_PING ||
		opcode == WS_FR_OP_PONG || opcode == WS_FR_OP_CLSE
	);
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

	/* Change state. */
	set_client_state(wfd->client, WS_STATE_OPEN);

	/* Trigger events and clean up buffers. */
	wfd->client->ws_srv.evs.onopen(wfd->client->client_id);
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

		if (ws_sendframe(wfd->client->client_id, (const char *)wfd->msg_ctrl, sizeof(char) * 2,
				WS_FR_OP_CLSE) < 0)
		{
			DEBUG("An error has occurred while sending closing frame!\n");
			return (-1);
		}
		return (0);
	}

	/* Send the data inside wfd->msg_ctrl. */
send:
	if (ws_sendframe(wfd->client->client_id, (const char *)wfd->msg_ctrl, wfd->frame_size,
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
			wfd->client->client_id, (const char *)wfd->msg_ctrl, frame_size, WS_FR_OP_PONG) < 0)
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
 * Frame state data
 *
 * This structure holds the current data for handling the
 * received frames.
 */
struct frame_state_data
{
	unsigned char *msg_data; /* Data frame.                */
	unsigned char *msg_ctrl; /* Control frame.             */
	uint8_t masks_data[4];   /* Masks data frame array.    */
	uint8_t masks_ctrl[4];   /* Masks control frame array. */
	uint64_t msg_idx_data;   /* Current msg index.         */
	uint64_t msg_idx_ctrl;   /* Current msg index.         */
	uint64_t frame_length;   /* Frame length.              */
	uint64_t frame_size;     /* Current frame size.        */
#ifdef VALIDATE_UTF8
	uint32_t utf8_state;     /* Current UTF-8 state.       */
#endif
	int32_t pong_id;         /* Current PONG id.           */
	uint8_t opcode;          /* Frame opcode.              */
	uint8_t is_fin;          /* Is FIN frame flag.         */
	uint8_t mask;            /* Mask.                      */
	int cur_byte;            /* Current frame byte.        */
};

/**
 * @brief Validates TXT frames if UTF8 validation is enabled.
 * If the content is not valid, the connection is aborted.
 *
 * @param wfd WebSocket frame data.
 * @param fsd Frame state data.
 *
 * @return Always 0.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int validate_utf8_txt(struct ws_frame_data *wfd,
	struct frame_state_data *fsd)
{
#ifdef VALIDATE_UTF8
	/* UTF-8 Validate partial (or not) frame. */
	if (wfd->frame_type != WS_FR_OP_TXT)
		return (0);

	if (fsd->is_fin)
	{
		if (is_utf8_len_state(
			fsd->msg_data + (fsd->msg_idx_data - fsd->frame_length),
			fsd->frame_length, fsd->utf8_state) != UTF8_ACCEPT)
		{
			DEBUG("Dropping invalid complete message!\n");
			wfd->error = 1;
			do_close(wfd, WS_CLSE_INVUTF8);
		}

		return (0);
	}

	/* Check current state for a CONT or initial TXT frame. */
	fsd->utf8_state =
		is_utf8_len_state(fsd->msg_data +
			(fsd->msg_idx_data - fsd->frame_length),
			fsd->frame_length, fsd->utf8_state);

	/* We can be in any state, except reject. */
	if (fsd->utf8_state == UTF8_REJECT)
	{
		DEBUG("Dropping invalid cont/initial frame!\n");
		wfd->error = 1;
		do_close(wfd, WS_CLSE_INVUTF8);
	}
#endif
	return (0);
}

/**
 * @brief Handle PONG frames in response to our PING
 * (or not, unsolicited is possible too).
 *
 * @param wfd WebSocket frame data.
 * @param fsd Frame state data.
 *
 * @return Always 0.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int handle_pong_frame(struct ws_frame_data *wfd,
	struct frame_state_data *fsd)
{
	fsd->is_fin = 0;

	/* If there is no content and/or differs the size, ignore it. */
	if (fsd->frame_size != sizeof(wfd->client->last_pong_id))
		return (0);

	/*
	 * Our PONG id should be positive and smaller than our
	 * current PING id. If not, ignore.
	 */
	/* clang-format off */
	pthread_mutex_lock(&wfd->client->mtx_ping);
		fsd->pong_id = pong_msg_to_int32(fsd->msg_ctrl);
		if (fsd->pong_id < 0 || fsd->pong_id > wfd->client->current_ping_id)
		{
			pthread_mutex_unlock(&wfd->client->mtx_ping);
			return (0);
		}
		wfd->client->last_pong_id = fsd->pong_id;
	pthread_mutex_unlock(&wfd->client->mtx_ping);
	/* clang-format on */

	return (0);
}

/**
 * @brief Handle PING frames sending a PONG response.
 *
 * @param wfd WebSocket frame data.
 * @param fsd Frame state data.
 *
 * @return Returns 0 if success, -1 otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int handle_ping_frame(struct ws_frame_data *wfd,
	struct frame_state_data *fsd)
{
	if (do_pong(wfd, fsd->frame_size) < 0)
		return (-1);

	/* Quick hack to keep our loop. */
	fsd->is_fin = 0;
	return (0);
}

/**
 * @brief Handle close frames while checking for UTF8
 * in the close reason.
 *
 * @param wfd WebSocket frame data.
 * @param fsd Frame state data.
 *
 * @return Returns 0 if success, -1 otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int handle_close_frame(struct ws_frame_data *wfd,
	struct frame_state_data *fsd)
{
#ifdef VALIDATE_UTF8
	/* If there is a close reason, check if it is UTF-8 valid. */
	if (fsd->frame_size > 2 &&
		!is_utf8_len(fsd->msg_ctrl + 2, fsd->frame_size - 2))
	{
		DEBUG("Invalid close frame payload reason! (not UTF-8)\n");
		wfd->error = 1;
		return (-1);
	}
#endif

	/*
	 * Since we're aborting, we can scratch the 'data'-related
	 * vars here.
	 */
	wfd->frame_size = fsd->frame_size;
	wfd->frame_type = WS_FR_OP_CLSE;
	free(fsd->msg_data);
	return (0);
}

/**
 * @brief Reads the current frame isolating data from control frames.
 * The parameters are changed in order to reflect the current state.
 *
 * @param wfd Websocket Frame Data.
 * @param fsd Frame state data.
 *
 * @return Returns 0 if success, a negative number otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
static int read_single_frame(struct ws_frame_data *wfd,
	struct frame_state_data *fsd)
{
	uint64_t *frame_size; /* Curr frame size. */
	unsigned char *tmp; /* Tmp message.     */
	unsigned char *msg; /* Current message. */
	uint64_t *msg_idx;  /* Message index.   */
	uint8_t *masks;     /* Current mask.    */
	int cur_byte;       /* Curr byte read.  */
	uint64_t i;         /* Loop index.      */

	/* Decide which mask and msg to use. */
	if (is_control_frame(fsd->opcode)) {
		frame_size = &fsd->frame_size;
		msg_idx = &fsd->msg_idx_ctrl;
		masks   = fsd->masks_ctrl;
		msg     = fsd->msg_ctrl;
	}
	else {
		frame_size = &wfd->frame_size;
		msg_idx = &fsd->msg_idx_data;
		masks   = fsd->masks_data;
		msg     = fsd->msg_data;
	}

	/* Decode masks and length for 16-bit messages. */
	if (fsd->frame_length == 126)
		fsd->frame_length = (((uint64_t)next_byte(wfd)) << 8) | next_byte(wfd);

	/* 64-bit messages. */
	else if (fsd->frame_length == 127)
	{
		fsd->frame_length =
			(((uint64_t)next_byte(wfd)) << 56) | /* frame[2]. */
			(((uint64_t)next_byte(wfd)) << 48) | /* frame[3]. */
			(((uint64_t)next_byte(wfd)) << 40) |
			(((uint64_t)next_byte(wfd)) << 32) |
			(((uint64_t)next_byte(wfd)) << 24) |
			(((uint64_t)next_byte(wfd)) << 16) |
			(((uint64_t)next_byte(wfd)) << 8)  |
			(((uint64_t)next_byte(wfd))); /* frame[9]. */
	}

	*frame_size += fsd->frame_length;

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
			wfd->client->client_sock, *frame_size + fsd->frame_length,
			MAX_FRAME_LENGTH);

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
	if (fsd->frame_length > 0)
	{
		if (!is_control_frame(fsd->opcode))
		{
			tmp = realloc(msg, *msg_idx + fsd->frame_length + fsd->is_fin);
			if (!tmp)
			{
				DEBUG("Cannot allocate memory, requested: % " PRId64 "\n",
					(*msg_idx + fsd->frame_length + fsd->is_fin));

				wfd->error = 1;
				return (-1);
			}
			msg = tmp;
			fsd->msg_data = msg;
		}

		/* Copy to the proper location. */
		for (i = 0; i < fsd->frame_length; i++, (*msg_idx)++)
		{
			/* We were able to read? .*/
			cur_byte = next_byte(wfd);
			if (cur_byte == -1)
				return (-1);

			msg[*msg_idx] = cur_byte ^ masks[i % 4];
		}
	}

	/* If we're inside a FIN frame, lets... */
	if (fsd->is_fin && *frame_size > 0)
	{
		/* Increase memory if our FIN frame is of length 0. */
		if (!fsd->frame_length && !is_control_frame(fsd->opcode))
		{
			tmp = realloc(msg, *msg_idx + 1);
			if (!tmp)
			{
				DEBUG("Cannot allocate memory, requested: %" PRId64 "\n",
					(*msg_idx + 1));

				wfd->error = 1;
				return (-1);
			}
			msg = tmp;
			fsd->msg_data = msg;
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
static int next_complete_frame(struct ws_frame_data *wfd)
{
	struct frame_state_data fsd = {0};
	fsd.msg_data = NULL;
	fsd.msg_ctrl = wfd->msg_ctrl;

#ifdef VALIDATE_UTF8
	fsd.utf8_state = UTF8_ACCEPT;
#endif

	wfd->frame_size =  0;
	wfd->frame_type = -1;
	wfd->msg = NULL;

	/* Read until find a FIN or a unsupported frame. */
	do
	{
		fsd.cur_byte = next_byte(wfd);
		if (fsd.cur_byte == -1)
			return (-1);

		fsd.is_fin = (fsd.cur_byte & 0xFF) >> WS_FIN_SHIFT;
		fsd.opcode = (fsd.cur_byte & 0xF);

		/* Check for RSV field. */
		if (fsd.cur_byte & 0x70)
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
		if ((wfd->frame_type == -1 && fsd.opcode == WS_FR_OP_CONT) ||
			(wfd->frame_type != -1 && !is_control_frame(fsd.opcode) &&
				fsd.opcode != WS_FR_OP_CONT))
		{
			DEBUG("Unexpected frame was received!, opcode: %d, previous: %d\n",
				fsd.opcode, wfd->frame_type);
			wfd->error = 1;
			break;
		}

		/* Check if one of the valid opcodes. */
		if (!is_valid_frame(fsd.opcode))
		{
			DEBUG("Unsupported frame opcode: %d\n", fsd.opcode);
			/* We should consider as error receive an unknown frame. */
			wfd->frame_type = fsd.opcode;
			wfd->error = 1;
			break;
		}

		/* Check our current state: if CLOSING, we only accept close frames. */
		if (get_client_state(wfd->client) == WS_STATE_CLOSING &&
			fsd.opcode != WS_FR_OP_CLSE)
		{
			DEBUG("Unexpected frame received, expected CLOSE (%d), "
				  "received: (%d)",
				WS_FR_OP_CLSE, fsd.opcode);
			wfd->error = 1;
			break;
		}

		/* Only change frame type if not a CONT frame. */
		if (fsd.opcode != WS_FR_OP_CONT && !is_control_frame(fsd.opcode))
			wfd->frame_type = fsd.opcode;

		fsd.mask         = next_byte(wfd);
		fsd.frame_length = fsd.mask & 0x7F;
		fsd.frame_size   = 0;
		fsd.msg_idx_ctrl = 0;

		/*
		 * We should deny non-FIN control frames or that have
		 * more than 125 octets.
		 */
		if (is_control_frame(fsd.opcode) &&
			(!fsd.is_fin || fsd.frame_length > 125))
		{
			DEBUG("Control frame bigger than 125 octets or not a FIN "
				  "frame!\n");
			wfd->error = 1;
			break;
		}

		/* Read a single frame, and then handle accordingly. */
		if (read_single_frame(wfd, &fsd) < 0)
			break;

		/* Handle each frame
		 * Obs: If BIN, nothing should be done unless we got
		 * a FIN-frame.
		 */
		switch (fsd.opcode) {
			/* UTF-8 Validate partial (or not) frame. */
			case WS_FR_OP_CONT:
			case WS_FR_OP_TXT: {
				validate_utf8_txt(wfd, &fsd);
				break;
			}
			/*
			 * We _may_ send a PING frame if the ws_ping() routine was invoked.
			 *
			 * If the content is invalid and/or differs the size, ignore it.
			 * (maybe unsolicited PONG).
			 */
			case WS_FR_OP_PONG: {
				handle_pong_frame(wfd, &fsd);
				goto next_it;
				break;
			}
			/* We should answer to a PING frame as soon as possible. */
			case WS_FR_OP_PING: {
				if (handle_ping_frame(wfd, &fsd) < 0)
					goto done;
				break;
			}
			/* We interrupt the loop as soon as we find a CLOSE frame. */
			case WS_FR_OP_CLSE: {
				if (handle_close_frame(wfd, &fsd) < 0)
					goto done;
				return (0);
				break;
			}
		}

next_it:;

	} while (!fsd.is_fin && !wfd->error);

done:
	/* Check for error. */
	if (wfd->error)
	{
		free(fsd.msg_data);
		wfd->msg = NULL;
		return (-1);
	}

	wfd->msg = fsd.msg_data;
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
	struct ws_frame_data wfd;      /* WebSocket frame data.   */
	struct ws_connection *client;  /* Client structure.       */
	int clse_thrd;                 /* Time-out close thread.  */

	client = vclient;

	/* Prepare frame data. */
	memset(&wfd, 0, sizeof(wfd));
	wfd.client = client;

	/* Do handshake. */
	if (do_handshake(&wfd) < 0)
		goto closed;

	/* Read next frame until client disconnects or an error occur. */
	while (next_complete_frame(&wfd) >= 0)
	{
		/* Text/binary event. */
		if ((wfd.frame_type == WS_FR_OP_TXT ||
			wfd.frame_type == WS_FR_OP_BIN) && !wfd.error)
		{
			client->ws_srv.evs.onmessage(client->client_id, wfd.msg, wfd.frame_size,
				wfd.frame_type);
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
	client->ws_srv.evs.onclose(client->client_id);

closed:
	clse_thrd = client->close_thrd;

	/* Wait for timeout thread if necessary. */
	if (clse_thrd)
	{
		pthread_cond_signal(&client->cnd_state_close);
		pthread_join(client->thrd_tout, NULL);
	}

	/* Close connectin properly. */
	if (get_client_state(client) != WS_STATE_CLOSED) {
		DEBUG("Closing: normal close\n");
		close_client(client, 1);
	}

	return (vclient);
}

/**
 * Accept parameters.
 */
struct ws_accept_params
{
	int sock;
	struct ws_server ws_srv;
};

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
	struct ws_accept_params *ws_prm; /* wsServer parameters. */
	struct sockaddr_storage sa; /* Client.                */
	pthread_t client_thread;    /* Client thread.         */
	struct timeval time;        /* Client socket timeout. */
	socklen_t salen;            /* Length of sockaddr.    */
	int new_sock;               /* New opened connection. */
	int sock;                   /* Server sock.           */
	int i;                      /* Loop index.            */

	ws_prm = data;
	sock   = ws_prm->sock;
	salen  = sizeof(sa);

	while (1)
	{
		/* Accept. */
		new_sock = accept(sock, (struct sockaddr *)&sa, &salen);
		if (new_sock < 0)
			panic("Error on accepting connections..");

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
			setsockopt(new_sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&time,
				sizeof(struct timeval));
		}

		/* Adds client socket to socks list. */
		pthread_mutex_lock(&mutex);
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (client_socks[i].client_sock == -1)
			{
				memcpy(&client_socks[i].ws_srv, &ws_prm->ws_srv,
					sizeof(struct ws_server));

				client_socks[i].client_sock  = new_sock;
				client_socks[i].state        = WS_STATE_CONNECTING;
				client_socks[i].close_thrd   = false;
				client_socks[i].last_pong_id = -1;
				client_socks[i].current_ping_id = -1;
				client_socks[i].client_id = get_next_cid();
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
 * @brief By using the server parameters provided in @p ws_srv,
 * create a socket and bind it accordingly with the server
 * configurations.
 *
 * @param ws_srv Web Socket configurations.
 *
 * @return Returns the socket file descriptor.
 */
static int do_bind_socket(struct ws_server *ws_srv)
{
	struct addrinfo hints, *results, *try;
	char port[8] = {0};
	int reuse;
	int sock;

	reuse = 1;

	/* Prepare the getaddrinfo structure. */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* Port. */
	snprintf(port, sizeof port - 1, "%d", ws_srv->port);

	if (getaddrinfo(ws_srv->host, port, &hints, &results) != 0)
		panic("getaddrinfo() failed");

	/* Try to create a socket with one of the returned addresses. */
	for (try = results; try != NULL; try = try->ai_next)
	{
		/* try to make a socket with this setup */
		if ((sock = socket(try->ai_family, try->ai_socktype,
			try->ai_protocol)) < 0)
		{
			continue;
		}

		/* Reuse previous address. */
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse,
			sizeof(reuse)) < 0)
		{
			panic("setsockopt(SO_REUSEADDR) failed");
		}

		/* Bind. */
		if (bind(sock, try->ai_addr, try->ai_addrlen) < 0)
			panic("Bind failed");

		/* if it worked, we're done. */
		break;
	}

	freeaddrinfo(results);

	/* Check if binded with success. */
	if (try == NULL)
		panic("couldn't find a port to bind to");

	return (sock);
}

/**
 * @brief Main loop for the server.
 *
 * @param ws_srv Web Socket server parameters.
 *
 * @return If @p thread_loop != 0, returns 0. Otherwise, never
 * returns.
 */
int ws_socket(struct ws_server *ws_srv)
{
	struct ws_accept_params *ws_prm; /* Accept parameters. */
	pthread_t accept_thread;   /* Accept thread.         */
	int sock;                 /* Client sock.           */

	timeout = ws_srv->timeout_ms;

	/* Ignore 'unused functions' warnings. */
	((void)skip_frame);

	/* Allocates our parameters data and copy the ws_server structure. */
	ws_prm = malloc(sizeof(*ws_prm));
	if (!ws_prm)
		panic("Unable to allocate ws parameters, out of memory!\n");

	memcpy(&ws_prm->ws_srv, ws_srv, sizeof(*ws_srv));

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

	/* Create socket and bind. */
	sock = do_bind_socket(ws_srv);

	/* Listen. */
	if (listen(sock, MAX_CLIENTS) < 0)
		panic("Unable to listen!\n");

	/* Wait for incoming connections. */
	printf("Waiting for incoming connections...\n");
	memset(client_socks, -1, sizeof(client_socks));

	/* Accept connections. */
	ws_prm->sock = sock;

	if (!ws_srv->thread_loop)
		ws_accept(ws_prm);
	else
	{
		if (pthread_create(&accept_thread, NULL, ws_accept, (void *)ws_prm))
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
