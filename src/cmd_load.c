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

#include <stdio.h>
#include <string.h>

struct cmd_load_data {
	struct cmd_data_base base;
	char *format;
	char cached_output[256];
};

static bool cmd_load_init(struct cmd_data_base *_data) {
	struct cmd_load_data *data = (struct cmd_load_data *)_data;
	return data->format;
}

static void cmd_load_destroy(struct cmd_data_base *_data) {
	struct cmd_load_data *data = (struct cmd_load_data *)_data;
	free(data->format);
}

// generaterd using command ./scripts/gen-format.py 123
VPRINT_OPTS(cmd_load_var_options, {0x00000000, 0x000E0000, 0x00000000, 0x00000000});

static bool cmd_load_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_load_data *data = (struct cmd_load_data *)_data;

	double loadavg[3];
	if (update && getloadavg(loadavg, 3) != -1) {
		int res;
		struct vprint ctx = {cmd_load_var_options, data->format, data->cached_output, data->cached_output + sizeof(data->cached_output)};
		while ((res = vprint_walk(&ctx)) >= 0) {
			vprint_dtoa(&ctx, loadavg[res - '1']);
		}
	}

	JSON_OUTPUT_KV(json_gen, "full_text", data->cached_output);

	return true;
}

#define LOAD_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_load_data, base.align)), \
	F("format", OPT_TYPE_STR, offsetof(struct cmd_load_data, format)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_load_data, base.interval)), \

CMD_OPTS_GEN_STRUCTS(cmd_load, LOAD_OPTIONS)

DECLARE_CMD(cmd_load) = {
	.name = "load",
	.data_size = sizeof (struct cmd_load_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_load),

	.func_init = cmd_load_init,
	.func_destroy = cmd_load_destroy,
	.func_output = cmd_load_output
};
