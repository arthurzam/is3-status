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

#include "main.h"
#include "vprint.h"

#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

struct cmd_load_data {
	struct cmd_data_base base;
	char *format;
	int fd;
	char cached_output[256];
};

static bool cmd_load_init(struct cmd_data_base *_data) {
	struct cmd_load_data *data = (struct cmd_load_data *)_data;
	data->fd = open("/proc/loadavg", O_RDONLY);
	if (data->fd < 0)
		return false;
	data->base.cached_fulltext = data->cached_output;
	return data->format;
}

static void cmd_load_destroy(struct cmd_data_base *_data) {
	struct cmd_load_data *data = (struct cmd_load_data *)_data;
	free(data->format);
	close(data->fd);
}

// generaterd using command ./scripts/gen-format.py 123
VPRINT_OPTS(cmd_load_var_options, {0x00000000, 0x000E0000, 0x00000000, 0x00000000});

static void cmd_load_recache(struct cmd_data_base *_data) {
	struct cmd_load_data *data = (struct cmd_load_data *)_data;

	char buf[65];
	lseek(data->fd, 0, SEEK_SET);
	ssize_t len = read(data->fd, buf, sizeof(buf) - 1);
	if (likely(len > 0)) {
		buf[len] = '\0';
		const char *loadavgs[3] = {NULL, NULL, NULL};
		char *tmp = buf;
		for (unsigned i = 0; i < 3; i++) {
			loadavgs[i] = tmp;
			tmp = strchr(tmp, ' ');
			*(tmp++) = '\0';
		}
		unsigned res;
		struct vprint ctx = {cmd_load_var_options, data->format, data->cached_output, data->cached_output + sizeof(data->cached_output)};
		while ((res = vprint_walk(&ctx)) != 0) {
			vprint_strcat(&ctx, loadavgs[res - '1']);
		}
	}
}

#define LOAD_OPTIONS(F) \
	F("format", OPT_TYPE_STR, offsetof(struct cmd_load_data, format)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_load_data, base.interval)), \

CMD_OPTS_GEN_STRUCTS(cmd_load, LOAD_OPTIONS)

DECLARE_CMD(cmd_load) = {
	.name = "load",
	.data_size = sizeof (struct cmd_load_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_load),

	.func_init = cmd_load_init,
	.func_destroy = cmd_load_destroy,
	.func_recache = cmd_load_recache
};
