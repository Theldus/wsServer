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
#include <sha1.h>
#include <base64.h>
#include <ws.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * @dir src/handshake
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
	SHA1Context ctx;

	if (!wsKey)
		return (-1);

	char *str = malloc( sizeof(char) * (WS_KEY_LEN + WS_MS_LEN + 1) );
	unsigned char hash[SHA1HashSize];

	strcpy(str, wsKey);
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
	char *s;
	unsigned char *accept;
	int ret;

	for (s = strtok(hsrequest, "\r\n"); s != NULL; s = strtok(NULL, "\r\n") )
		if (strstr(s, WS_HS_REQ) != NULL)
			break;

	s = strtok(s,    " ");
	s = strtok(NULL, " ");
	
	ret = get_handshake_accept(s, &accept);
	if (ret < 0)
		return (ret);

	*hsresponse = malloc(sizeof(char) * WS_HS_ACCLEN);
	strcpy(*hsresponse, WS_HS_ACCEPT);
	strcat(*hsresponse, (const char *)accept);
	strcat(*hsresponse, "\r\n\r\n");

	free(accept);
	return (0);
}
