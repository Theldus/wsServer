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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <ws.h>

/* Registered events. */
struct ws_events events;

/* Client socks. */
int client_socks[MAX_CLIENTS];

/* Global mutex. */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define panic(s)     \
do                   \
{                    \
	perror(s);       \
	exit(-1);        \
} while (0);

/**
 * Gets the IP address relative to a
 * file descriptor opened by the server.
 *
 * @param fd File descriptor target.
 *
 * @return Pointer the ip address.
 */
char* ws_getaddress(int fd)
{
	struct sockaddr_in addr;
	socklen_t addr_size;
	char *client;

	addr_size = sizeof(struct sockaddr_in);
	if (getpeername(fd, (struct sockaddr *)&addr, &addr_size) < 0)
		return NULL;

	client = malloc(sizeof(char) * 20);
	strcpy(client, inet_ntoa(addr.sin_addr));
	return (client);
}

/**
 * Creates and send an WebSocket frame
 * with some text message.
 *
 * @param fd        Target to be send.
 * @param msg       Message to be send.
 * @param broadcast Enable/disable broadcast.
 *
 * @return Returns the number of bytes written.
 */
int ws_sendframe(int fd, const char *msg, bool broadcast)
{
	unsigned char *response;  /* Response data.  */
	unsigned char frame[10];  /* Frame.          */
	uint8_t idx_first_rData;  /* Index data.     */
	uint64_t length;          /* Message length. */
	int idx_response;         /* Index response. */
	int output;               /* Bytes sent.     */
	int sock;                 /* File Descript.  */
	int i;                    /* Loop index.     */

	/* Text data. */
	length   = strlen( (const char *) msg);
	frame[0] = (WS_FIN | WS_FR_OP_TXT);

	/* Split the size between octects. */
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
		frame[2] = (unsigned char) ((length >> 56) & 255);
		frame[3] = (unsigned char) ((length >> 48) & 255);
		frame[4] = (unsigned char) ((length >> 40) & 255);
		frame[5] = (unsigned char) ((length >> 32) & 255);
		frame[6] = (unsigned char) ((length >> 24) & 255);
		frame[7] = (unsigned char) ((length >> 16) & 255);
		frame[8] = (unsigned char) ((length >> 8) & 255);
		frame[9] = (unsigned char) (length & 255);
		idx_first_rData = 10;
	}

	/* Add frame bytes. */
	idx_response = 0;
	response = malloc( sizeof(unsigned char) * (idx_first_rData + length + 1) );
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
	output = write(fd, response, idx_response);
	if (broadcast)
	{
		pthread_mutex_lock(&mutex);
			for (i = 0; i < MAX_CLIENTS; i++)
			{
				sock = client_socks[i];
				if ((sock > -1) && (sock != fd))
					output += write(sock, response, idx_response);
			}
		pthread_mutex_unlock(&mutex);
	}

	free(response);
	return (output);
}

/**
 * Receives a text frame, parse and decodes it.
 *
 * @param frame  WebSocket frame to be parsed.
 * @param length Frame length.
 * @param type   Frame type.
 *
 * @return Returns a dynamic null-terminated string that contains
 * a pointer to the received frame.
 */
static unsigned char* ws_receiveframe(unsigned char *frame, size_t length, int *type)
{
	unsigned char *msg;     /* Decoded message.        */
	uint8_t mask;           /* Payload is masked?      */
	uint8_t flength;        /* Raw length.             */
	uint8_t idx_first_mask; /* Index masking key.      */
	uint8_t idx_first_data; /* Index data.             */
	size_t  data_length;    /* Data length.            */
	uint8_t masks[4];       /* Masking key.            */
	int     i,j;            /* Loop indexes.           */

	msg = NULL;

	/* Checks the frame type and parse the frame. */
	if (frame[0] == (WS_FIN | WS_FR_OP_TXT) )
	{
		*type = WS_FR_OP_TXT;
		idx_first_mask = 2;
		mask           = frame[1];
		flength        = mask & 0x7F;

		if (flength == 126)
			idx_first_mask = 4;
		else if (flength == 127)
			idx_first_mask = 10;

		idx_first_data = idx_first_mask + 4;
		data_length = length - idx_first_data;

		masks[0] = frame[idx_first_mask+0];
		masks[1] = frame[idx_first_mask+1];
		masks[2] = frame[idx_first_mask+2];
		masks[3] = frame[idx_first_mask+3];

		msg = malloc(sizeof(unsigned char) * (data_length+1) );
		for (i = idx_first_data, j = 0; i < length; i++, j++)
			msg[j] = frame[i] ^ masks[j % 4];

		msg[j] = '\0';
	}

	/* Close frame. */
	else if (frame[0] == (WS_FIN | WS_FR_OP_CLSE) )
		*type = WS_FR_OP_CLSE;

	/* Not supported frame yet. */
	else
		*type = frame[0] & 0x0F;

	return (msg);
}

/**
 * Establishes to connection with the client and trigger
 * events when occurs one.
 *
 * @param vsock Client file descriptor.
 *
 * @return Returns vsock if success and a negative
 * number otherwise.
 *
 * @note This will be run on a different thread.
 */
static void* ws_establishconnection(void *vsock)
{
	int sock;                           /* File descriptor.               */
	size_t n;                           /* Number of bytes sent/received. */
	unsigned char frm[MESSAGE_LENGTH];  /* Frame.                         */
	unsigned char *msg;                 /* Message.                       */
	char *response;                     /* Response frame.                */
	int  handshaked;                    /* Handshake state.               */
	int  type;                          /* Frame type.                    */
	int  i;                             /* Loop index.                    */
	int  ret;

	handshaked = 0;
	sock = (int)(intptr_t)vsock;

	/* Receives message until get some error. */
	while ((n = read(sock, frm, sizeof(unsigned char) * MESSAGE_LENGTH)) > 0)
	{
		/* If not handshaked yet. */
		if (!handshaked)
		{
			ret = getHSresponse( (char *) frm, &response);
			if (ret < 0)
				goto closed;

			handshaked = 1;
#ifdef VERBOSE_MODE
			printf("Handshaked, response: \n"
				"------------------------------------\n"
				"%s"
				"------------------------------------\n"
				,response);
#endif
			n = write(sock, response, strlen(response));
			events.onopen(sock);
			free(response);
		}

		/* Decode/check type of frame. */
		msg = ws_receiveframe(frm, n, &type);
		if (msg == NULL)
		{
#ifdef VERBOSE_MODE
			printf("Non text frame received from %d", sock);
			if (type == WS_FR_OP_CLSE)
				printf(": close frame!\n");
			else
			{
				printf(", type: %x\n", type);
				continue;
			}
#endif
		}

		/* Trigger events. */
		if (type == WS_FR_OP_TXT)
		{
			events.onmessage(sock, msg);
			free(msg);
		}
		else if (type == WS_FR_OP_CLSE)
		{
			free(msg);
			events.onclose(sock);
			goto closed;
		}
	}

closed:
	/* Removes client socket from socks list. */
	pthread_mutex_lock(&mutex);
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (client_socks[i] == sock)
			{
				client_socks[i] = -1;
				break;
			}
		}
	pthread_mutex_unlock(&mutex);
	close(sock);

	return (vsock);
}

/**
 * Main loop for the server,
 *
 * @param evs  Events structure.
 * @param port Server port.
 *
 * @return This function never returns.
 */
int ws_socket(struct ws_events *evs, uint16_t port)
{
	int sock;                  /* Current socket.        */
	int new_sock;              /* New opened connection. */
	struct sockaddr_in server; /* Server.                */
	struct sockaddr_in client; /* Client.                */
	int len;                   /* Length of sockaddr.    */
	pthread_t client_thread;   /* Client thread.         */
	int i;                     /* Loop index.            */

	/* Checks if the event list is a valid pointer. */
	if (evs == NULL)
		panic("Invalid event list!");

	/* Copy events. */
	memcpy(&events, evs, sizeof(struct ws_events));

	/* Create socket. */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		panic("Could not create socket");

	/* Reuse previous address. */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
		panic("setsockopt(SO_REUSEADDR) failed");

	/* Prepare the sockaddr_in structure. */
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	/* Bind. */
	if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
		panic("Bind failed");

	/* Listen. */
	listen(sock, MAX_CLIENTS);

	/* Wait for incoming connections. */
	printf("Waiting for incoming connections...\n");

	len = sizeof(struct sockaddr_in);
	memset(client_socks, -1, sizeof(client_socks));

	/* Accept connections. */
	while (1)
	{
		/* Accept. */
		new_sock = accept(sock, (struct sockaddr *)&client, (socklen_t*)&len);
		if (new_sock < 0)
			panic("Error on accepting connections..");

		/* Adds client socket to socks list. */
		pthread_mutex_lock(&mutex);
		    for (i = 0; i < MAX_CLIENTS; i++)
		    {
				if (client_socks[i] == -1)
				{
					client_socks[i] = new_sock;
					break;
				}
			}
		pthread_mutex_unlock(&mutex);

		if (pthread_create(&client_thread, NULL, ws_establishconnection, (void*)(intptr_t) new_sock) < 0)
			panic("Could not create the client thread!");

		pthread_detach(client_thread);
	}
	return (0);
}
