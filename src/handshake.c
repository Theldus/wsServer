/*
 * Copyright (C) 2016-2020  Davidson Francis <davidsondfgl@gmail.com>
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
#include <base64.h>
#include <sha1.h>
#include <ws.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @dir src/
 * @brief Handshake routines directory
 *
 * @file handshake.c
 * @brief Handshake routines.
 */

/**
 * @brief Gets the field Sec-WebSocket-Accept on response, by
 * an previously informed key.
 *
 * @param wsKey Sec-WebSocket-Key
 * @param dest source to be stored the value.
 *
 * @return Returns 0 if success and a negative number
 * otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
int get_handshake_accept(char *wsKey, unsigned char **dest)
{
	unsigned char hash[SHA1HashSize]; /* SHA-1 Hash.                   */
	SHA1Context ctx;                  /* SHA-1 Context.                */
	char *str;                        /* WebSocket key + magic string. */

	/* Invalid key. */
	if (!wsKey)
		return (-1);

	str = calloc(1, sizeof(char) * (WS_KEY_LEN + WS_MS_LEN + 1));
	if (!str)
		return (-1);

	strncpy(str, wsKey, WS_KEY_LEN);
	strcat(str, MAGIC_STRING);

	SHA1Reset(&ctx);
	SHA1Input(&ctx, (const uint8_t *)str, WS_KEYMS_LEN);
	SHA1Result(&ctx, hash);

	*dest = base64_encode(hash, SHA1HashSize, NULL);
	*(*dest + strlen((const char *)*dest) - 1) = '\0';
	free(str);
	return (0);
}

/**
 * @brief Gets the complete response to accomplish a succesfully
 * handshake.
 *
 * @param hsrequest  Client request.
 * @param hsresponse Server response.
 *
 * @return Returns 0 if success and a negative number
 * otherwise.
 *
 * @attention This is part of the internal API and is documented just
 * for completeness.
 */
int get_handshake_response(char *hsrequest, char **hsresponse)
{
	unsigned char *accept; /* Accept message.     */
	char *saveptr;         /* strtok_r() pointer. */
	char *s;               /* Current string.     */
	int ret;               /* Return value.       */

	saveptr = NULL;
	for (s = strtok_r(hsrequest, "\r\n", &saveptr); s != NULL;
		 s = strtok_r(NULL, "\r\n", &saveptr))
	{
		if (strstr(s, WS_HS_REQ) != NULL)
			break;
	}

	/* Ensure that we have a valid pointer. */
	if (s == NULL)
		return (-1);

	saveptr = NULL;
	s       = strtok_r(s, " ", &saveptr);
	s       = strtok_r(NULL, " ", &saveptr);

	ret = get_handshake_accept(s, &accept);
	if (ret < 0)
		return (ret);

	*hsresponse = malloc(sizeof(char) * WS_HS_ACCLEN);
	if (*hsresponse == NULL)
		return (-1);

	strcpy(*hsresponse, WS_HS_ACCEPT);
	strcat(*hsresponse, (const char *)accept);
	strcat(*hsresponse, "\r\n\r\n");

	free(accept);
	return (0);
}
