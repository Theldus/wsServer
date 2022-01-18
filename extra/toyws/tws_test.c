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
#include "toyws.h"

int main(void)
{
	struct tws_ctx ctx;
	char msg[] = "Hello";

	/* Buffer params. */
	char *buff;
	int frm_type;
	size_t buff_size;

	buff      = NULL;
	buff_size = 0;
	frm_type  = 0;

	if (tws_connect(&ctx, "127.0.0.1", 8080) < 0)
		fprintf(stderr, "Unable to connect!\n");

	/* Send message. */
	printf("Send: %s\n",
		(tws_sendframe(&ctx, (uint8_t*)msg, strlen(msg), FRM_TXT) >= 0 ?
			"Success" : "Failed"));

	/* Blocks until receive a single message. */
	if (tws_receiveframe(&ctx, &buff, &buff_size, &frm_type) < 0)
		fprintf(stderr, "Unable to receive message!\n");

	printf("I received: (%s) (type: %s)\n", buff,
		(frm_type == FRM_TXT ? "Text" : "Binary"));

	tws_close(&ctx);

	free(buff);
	return (0);
}
