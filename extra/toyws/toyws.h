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

#ifndef TOYWS_H
#define TOYWS_H

	#include <stdint.h>

	/* Frame constants. */
	#define FRM_TXT  1
	#define FRM_BIN  2
	#define FRM_CLSE 8
	#define FRM_FIN 128
	#define FRM_MSK 128

	#define MESSAGE_LENGTH 1024

	/* Client status. */
	#define TWS_ST_DISCONNECTED 0
	#define TWS_ST_CONNECTED    1

	/* Client context. */
	struct tws_ctx
	{
		uint8_t frm[MESSAGE_LENGTH];
		size_t amt_read;
		size_t cur_pos;
		int status;
		int fd;
	};

	/* External functions. */
	extern int tws_connect(struct tws_ctx *ctx, const char *ip,
		uint16_t port);
	extern void tws_close(struct tws_ctx *ctx);
	extern int tws_sendframe(struct tws_ctx *ctx, uint8_t *msg,
		uint64_t size, int type);
	extern int tws_receiveframe(struct tws_ctx *ctx, char **buff,
		size_t *buff_size, int *frm_type);

#endif /* TOYWS_H */
