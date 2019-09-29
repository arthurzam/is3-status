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
#include <alloca.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

struct cmd_cpu_temperature_data {
	struct cmd_data_base base;
	char *format;
	union {
		char *device;
		int thermal_dir_fd;
	};
	long high_threshold;
	char cached_output[128];
};

static bool cmd_cpu_temperature_init(struct cmd_data_base *_data) {
	struct cmd_cpu_temperature_data *data = (struct cmd_cpu_temperature_data *)_data;

	if (!data->format)
		return false;
	if (!data->device)
		return false;
	data->base.cached_fulltext = data->cached_output;

#define THERMAL_PATH "/sys/devices/virtual/thermal/"
	const size_t device_len = strlen(data->device);
	char *path = alloca(strlen(THERMAL_PATH) + device_len + 1);
	memcpy(path, THERMAL_PATH, strlen(THERMAL_PATH));
	memcpy(path + strlen(THERMAL_PATH), data->device, device_len + 1);
	free(data->device);
	data->device = NULL;
#undef THERMAL_PATH

	data->thermal_dir_fd = open(path, O_PATH | O_DIRECTORY);
	return data->thermal_dir_fd >= 0;
}

static void cmd_cpu_temperature_destroy(struct cmd_data_base *_data) {
	struct cmd_cpu_temperature_data *data = (struct cmd_cpu_temperature_data *)_data;
	free(data->format);
	close(data->thermal_dir_fd);
}

// generaterd using command ./scripts/gen-format.py cf
VPRINT_OPTS(cmd_cpu_temperature_data_var_options, {0x00000000, 0x00000000, 0x00000000, 0x00000048});

static void cmd_cpu_temperature_recache(struct cmd_data_base *_data) {
	struct cmd_cpu_temperature_data *data = (struct cmd_cpu_temperature_data *)_data;

	int curr_value = -1;
	{
		int fd = openat(data->thermal_dir_fd, "temp", O_RDONLY);
		if (likely(fd >= 0)) {
			char buf[64];
			ssize_t len = read(fd, buf, sizeof(buf) - 1);
			if (likely(len > 0)) {
				buf[len] = '\0';
				curr_value = atoi(buf) / 1000;
			}
		}
		close(fd);
	}

	unsigned res;
	struct vprint ctx = {cmd_cpu_temperature_data_var_options, data->format, data->cached_output, data->cached_output + sizeof(data->cached_output)};
	while ((res = vprint_walk(&ctx)) != 0) {
		if (curr_value == -1)
			vprint_strcat(&ctx, "???");
		else {
			int output = curr_value;
			if (res == 'f')
				output = output * 9 / 5 + 32;
			vprint_itoa(&ctx, output);
		}
	}
	if (data->high_threshold > 0 && data->high_threshold < curr_value)
		CMD_COLOR_SET(data, g_general_settings.color_bad);
	else
		CMD_COLOR_CLEAN(data);
}

#define CPU_TEMP_OPTIONS(F) \
	F("device", OPT_TYPE_STR, offsetof(struct cmd_cpu_temperature_data, device)), \
	F("format", OPT_TYPE_STR, offsetof(struct cmd_cpu_temperature_data, format)), \
	F("high_threshold", OPT_TYPE_LONG, offsetof(struct cmd_cpu_temperature_data, high_threshold)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_cpu_temperature_data, base.interval)), \

CMD_OPTS_GEN_STRUCTS(cmd_cpu_temperature, CPU_TEMP_OPTIONS)

DECLARE_CMD(cmd_cpu_temperature) = {
	.name = "cpu_temperature",
	.data_size = sizeof (struct cmd_cpu_temperature_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_cpu_temperature),

	.func_init = cmd_cpu_temperature_init,
	.func_destroy = cmd_cpu_temperature_destroy,
	.func_recache = cmd_cpu_temperature_recache
};
