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
#include <string.h>

static void vprint_ch(struct vprint *ctx, char ch) {
	ctx->buffer[0] = ch;
	ctx->buffer[1] = '\0';
	ctx->buffer += 1;
	ctx->remainingSize -= 1;
}

int vprint_walk(struct vprint *ctx) {
	const char *next = strchr(ctx->curr_pos, '%');
	if (next == NULL)
		return (void)vprint_strcat(ctx, ctx->curr_pos), VPRINT_EOF;
	const size_t len = (size_t)(next - ctx->curr_pos);
	if (ctx->remainingSize <= len)
		return VPRINT_MISSING_SIZE;
	memcpy(ctx->buffer, ctx->curr_pos, len);
	ctx->buffer[len] = '\0';
	ctx->buffer += len;
	ctx->remainingSize -= len;
	ctx->curr_pos += len + 2;
	const uint8_t n = (uint8_t)(*(next + 1));
	if ((n < 0x80) && (ctx->var_options[n >> 5] & (1 << (n & 0x1F))))
		return n;
	if (n == '%') {
		vprint_ch(ctx, '%');
		return vprint_walk(ctx);
	}
	return VPRINT_UNKNOWN_OPT;
}

void vprint_strcat(struct vprint *ctx, const char *str) {
	size_t len = strlen(str);
	if (ctx->remainingSize <= len)
		return;
	memcpy(ctx->buffer, str, len);
	ctx->buffer[len] = '\0';
	ctx->buffer += len;
	ctx->remainingSize -= len;
}

void vprint_itoa(struct vprint *ctx, int value) {
	int len = snprintf(ctx->buffer, ctx->remainingSize, "%d", value);
	if (len < 0 || (unsigned)len >= ctx->remainingSize)
		return;
	ctx->buffer[len] = '\0';
	ctx->buffer += len;
	ctx->remainingSize -= (unsigned)len;
}

void vprint_dtoa(struct vprint *ctx, double value) {
	int len = snprintf(ctx->buffer, ctx->remainingSize, "%.02f", value);
	if (len < 0 || (unsigned)len >= ctx->remainingSize)
		return;
	ctx->buffer[len] = '\0';
	ctx->buffer += len;
	ctx->remainingSize -= (unsigned)len;
}

void vprint_time(struct vprint *ctx, int value) {
	int s = value % 60;
	value /= 60;
	int m = value % 60;
	if (value >= 60) {
		vprint_itoa(ctx, value / 60);
		vprint_ch(ctx, ':');
	}
	ctx->buffer[0] = (char)('0' + (m / 10));
	ctx->buffer[1] = (char)('0' + (m % 10));
	ctx->buffer[2] = ':';
	ctx->buffer[3] = (char)('0' + (s / 10));
	ctx->buffer[4] = (char)('0' + (s % 10));
	ctx->buffer[5] = '\0';
	ctx->buffer += 5;
	ctx->remainingSize -= 5;
}

#define DISK_PREFIX_MAX 4
static const struct disk_prefix_t {
	const char suffix[4];
	uint64_t base;
} g_disk_suffixs[2][DISK_PREFIX_MAX] = {
	{ // binary
		{"TB", 0x10000000000ULL}, // 1024^4
		{"GB", 0x40000000ULL}, // 1024^3
		{"MB", 0x100000ULL}, // 1024^2
		{"KB", 0x400ULL}, // 1024^1
	},
	{ // decimal
		{"TiB", 1000000000000ULL}, // 1000^4
		{"GiB", 1000000000ULL}, // 1000^3
		{"MiB", 1000000ULL}, // 1000^2
		{"KiB", 1000ULL}, // 1000^1
	},
};
static const struct disk_prefix_t g_disk_prefix_base = {"", 1};

void vprint_human_bytes(struct vprint *ctx, uint64_t value, uint64_t pct_base, uint64_t val_bsize, bool use_decimal) {

	uint64_t base;
	const char *suffix;
	if (pct_base != 0) {
		value *= 100;
		base = pct_base;
		suffix = "%";
	} else {
		value *= val_bsize;
		const struct disk_prefix_t *temp = &g_disk_prefix_base;
		for(unsigned i = 0; i < DISK_PREFIX_MAX; i++) {
			if (g_disk_suffixs[use_decimal][i].base < value) {
				temp = g_disk_suffixs[use_decimal] + i;
				break;
			}
		}
		base = temp->base;
		suffix = temp->suffix;
	}
	vprint_dtoa(ctx, (double)value / base);
	vprint_strcat(ctx, suffix);
}

long parse_human_bytes(const char *str) {
	const char *last = str + strlen(str) - 1;
	long base = 0;
	if (*last == '%') {
		base = -1;
	} else if (*last >= '0' && *last <= '9') {
		base = 1;
	} else {
		for(unsigned v = 0; v < 2; ++v) {
			for(unsigned i = 0; i < DISK_PREFIX_MAX; i++) {
				if (0 == memcmp(g_disk_suffixs[v][i].suffix, last - (1 + v), 2 + v)) {
					base = (long)g_disk_suffixs[v][i].base;
					goto _out;
				}
			}
		}
	}
_out:
	return base * atoll(str);
}

void vprint_collect_used(const char *str, uint32_t var_options[8]) {
	while ((str = strchr(str, '%'))) {
		if (*(++str) == '\0')
			return;
		var_options[(*str) >> 5] |= (1 << ((*str) & 0x1F));
		++str;
	}
}
