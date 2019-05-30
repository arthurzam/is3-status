/*
 * This file is part of is3-status (https://github.com/arthurzam/is3-status).
 * Copyright (C) 2019  Arthur Zamarin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "vprint.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

int vprint_walk(struct vprint *ctx) {
	const char *next = strchr(ctx->currPos, '%');
	if (next == NULL)
		return vprint_strcat(ctx, ctx->currPos);
	const size_t len = (size_t)(next - ctx->currPos);
	if (ctx->remainingSize <= len)
		return VPRINT_MISSING_SIZE;
	memcpy(ctx->buffer, ctx->currPos, len);
	ctx->buffer[len] = '\0';
	ctx->buffer += len;
	ctx->remainingSize -= len;
	ctx->currPos += len + 2;
	const char n = *(next + 1);
	if (0 != (ctx->var_options[n >> 5] & (1 << (n & 0x1F))))
		return n;
	else if (n == '%') {
		ctx->buffer[0] = '%';
		ctx->buffer[1] = '\0';
		ctx->buffer += 1;
		ctx->remainingSize -= 1;
		return vprint_walk(ctx);
	} else
		return VPRINT_UNKNOWN_OPT;
}

int vprint_strcat(struct vprint *ctx, const char *str) {
	size_t len = strlen(str);
	if (ctx->remainingSize <= len)
		return VPRINT_MISSING_SIZE;
	memcpy(ctx->buffer, str, len);
	ctx->buffer[len] = '\0';
	ctx->buffer += len;
	ctx->remainingSize -= len;
	return VPRINT_EOF;
}

static int __attribute__((format(printf, 2, 3))) vprint_snprintf(struct vprint *ctx, const char *format, ...) {
	va_list args;
	va_start (args, format);
	int len = vsnprintf(ctx->buffer, ctx->remainingSize, format, args);
	va_end (args);

	if (len < 0 || (unsigned)len >= ctx->remainingSize)
		return VPRINT_MISSING_SIZE;
	ctx->buffer[len] = '\0';
	ctx->buffer += len;
	ctx->remainingSize -= (unsigned)len;
	return VPRINT_EOF;
}

int vprint_itoa(struct vprint *ctx, int value) {
	return vprint_snprintf(ctx, "%d", value);
}

int vprint_dtoa(struct vprint *ctx, double value) {
	return vprint_snprintf(ctx, "%.02f", value);
}

void vprint_collect_used(const char *str, uint32_t var_options[8]) {
	while ((str = strchr(str, '%'))) {
		if (*(++str) == '\0')
			return;
		var_options[(*str) >> 5] |= (1 << ((*str) & 0x1F));
		++str;
	}
}
