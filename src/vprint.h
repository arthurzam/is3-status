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

#ifndef VAR_PRINT_H
#define VAR_PRINT_H

#include <stdint.h>
#include <stdlib.h>

struct vprint {
	const uint32_t *const var_options;
	const char *currPos;
	char *buffer;
	size_t remainingSize;
};
#define VPRINT_OPTS(name, ...) static const uint32_t name[8] = __VA_ARGS__

enum vprint_output {
	VPRINT_EOF = -1,
	VPRINT_ERROR = -2,
	VPRINT_MISSING_SIZE = -3,
	VPRINT_UNKNOWN_OPT = -4
};

int vprint_walk(struct vprint *ctx);
int vprint_strcat(struct vprint *ctx, const char *str);
int vprint_itoa(struct vprint *ctx, int value);
int vprint_dtoa(struct vprint *ctx, double value);
int vprint_time(struct vprint *ctx, int value);
void vprint_collect_used(const char *str, uint32_t var_options[8]);

#endif // INI_PARSER_H
