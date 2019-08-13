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
#include <stdio.h>

#include <unistd.h>

struct cmd_cpu_temperature_data {
	struct cmd_data_base base;
	char *format;
	char *path;
	long high_threshold;
	int curr_value;
};

static bool cmd_cpu_temperature_init(struct cmd_data_base *_data) {
	struct cmd_cpu_temperature_data *data = (struct cmd_cpu_temperature_data *)_data;

	if (data->format == NULL)
		return false;

	if (data->path) {
		const size_t len = strlen(data->path);
		data->path = realloc(data->path, len + 7);
		memcpy(data->path + len, "/temp", 6);
	} else
		data->path = strdup("/sys/class/thermal/thermal_zone0/temp");
	data->curr_value = -1;

	if (access(data->path, R_OK) != 0)
		return false;

	return true;
}

static void cmd_cpu_temperature_destroy(struct cmd_data_base *_data) {
	struct cmd_cpu_temperature_data *data = (struct cmd_cpu_temperature_data *)_data;
	free(data->format);
	free(data->path);
}

// generaterd using command ./scripts/gen-format.py cf
VPRINT_OPTS(cmd_cpu_temperature_data_var_options, {0x00000000, 0x00000000, 0x00000000, 0x00000048});

static bool cmd_cpu_temperature_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_cpu_temperature_data *data = (struct cmd_cpu_temperature_data *)_data;

	if (update || data->curr_value == -1) {
		char buf[64];
		FILE *f;
		if ((f = fopen(data->path, "r")) && fgets(buf, sizeof(buf), f))
			data->curr_value = atoi(buf) / 1000;
		else
			data->curr_value = -1;
		fclose(f);
	}

	int res;
	char buffer[256];
	struct vprint ctx = {cmd_cpu_temperature_data_var_options, data->format, buffer, buffer + sizeof(buffer)};
	while ((res = vprint_walk(&ctx)) >= 0) {
		if (data->curr_value == -1)
			vprint_strcat(&ctx, "???");
		else {
			int output = data->curr_value;
			if (res == 'f')
				output = output * 9 / 5 + 32;
			vprint_itoa(&ctx, output);
		}
	}
	if (data->high_threshold > 0 && data->high_threshold < data->curr_value)
		JSON_OUTPUT_COLOR(json_gen, g_general_settings.color_bad);
	JSON_OUTPUT_K(json_gen, "full_text", buffer, (size_t)(ctx.buffer_start - buffer));

	return true;
}

#define CPU_TEMP_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_cpu_temperature_data, base.align)), \
	F("format", OPT_TYPE_STR, offsetof(struct cmd_cpu_temperature_data, format)), \
	F("high_threshold", OPT_TYPE_LONG, offsetof(struct cmd_cpu_temperature_data, high_threshold)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_cpu_temperature_data, base.interval)), \
	F("path", OPT_TYPE_STR, offsetof(struct cmd_cpu_temperature_data, path))

CMD_OPTS_GEN_STRUCTS(cmd_cpu_temperature, CPU_TEMP_OPTIONS)

DECLARE_CMD(cmd_cpu_temperature) = {
	.name = "cpu_temperature",
	.data_size = sizeof (struct cmd_cpu_temperature_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_cpu_temperature),

	.func_init = cmd_cpu_temperature_init,
	.func_destroy = cmd_cpu_temperature_destroy,
	.func_output = cmd_cpu_temperature_output
};
