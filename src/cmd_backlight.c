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
#include <stdio.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

struct cmd_backlight_data {
	struct cmd_data_base base;
	char *format;
	char *device;
	long wheel_step;
	long max_brightness;
	int backlight_fd;
	bool supports_changing;
	char cached_output[128];
};

static long cmd_backlight_read_value(int fd) {
	char buf[64];
	lseek(fd, 0, SEEK_SET);
	ssize_t len = read(fd, buf, sizeof(buf) - 1);
	if (unlikely(len <= 0))
		return -1;
	buf[len] = '\0';
	return atol(buf);
}

static bool cmd_backlight_init(struct cmd_data_base *_data) {
	struct cmd_backlight_data *data = (struct cmd_backlight_data *)_data;

	if (data->wheel_step <= 0)
		data->wheel_step = 2;
	if (!data->format)
		return false;
	if (!data->device)
		return false;
	data->base.cached_fulltext = data->cached_output;

#define BACKLIGHT_PATH "/sys/class/backlight/"
	const size_t device_len = strlen(data->device);
	char *path = alloca(strlen(BACKLIGHT_PATH) + device_len + 1);
	memcpy(path, BACKLIGHT_PATH, strlen(BACKLIGHT_PATH));
	memcpy(path + strlen(BACKLIGHT_PATH), data->device, device_len + 1);
	free(data->device);
	data->device = NULL;
#undef BACKLIGHT_PATH

	int dir_fd = open(path, O_PATH | O_DIRECTORY);
	if (dir_fd < 0)
		return false;
	data->supports_changing = (0 == faccessat(dir_fd, "brightness", W_OK, AT_EACCESS));

	/* read max_brightness */
	{
		int max_brightness_fd = openat(dir_fd, "max_brightness", O_RDONLY);
		if (max_brightness_fd < 0)
			return false;
		data->max_brightness = cmd_backlight_read_value(max_brightness_fd);
		close(max_brightness_fd);
		if (data->max_brightness <= 0)
			return false;
	}

	data->backlight_fd = openat(dir_fd, "brightness", data->supports_changing ? O_RDWR : O_RDONLY);
	close(dir_fd);
	return data->backlight_fd >= 0;
}

static void cmd_backlight_destroy(struct cmd_data_base *_data) {
	struct cmd_backlight_data *data = (struct cmd_backlight_data *)_data;
	free(data->format);
	close(data->backlight_fd);
}

// generaterd using command ./scripts/gen-format.py vV
VPRINT_OPTS(cmd_backlight_data_var_options, {0x00000000, 0x00000000, 0x00400000, 0x00400000});
static void cmd_backlight_update_text(struct cmd_backlight_data *data, long value) {
	if (likely(value >= 0)) {
		const int brightness = (int)((value * 100 + data->max_brightness / 2) / data->max_brightness);
		struct vprint ctx = {cmd_backlight_data_var_options, data->format, data->cached_output, data->cached_output + sizeof(data->cached_output)};
		while (vprint_walk(&ctx) != 0) {
			vprint_itoa(&ctx, brightness);
		}
	}
}

static void cmd_backlight_recache(struct cmd_data_base *_data) {
	struct cmd_backlight_data *data = (struct cmd_backlight_data *)_data;
	cmd_backlight_update_text(data, cmd_backlight_read_value(data->backlight_fd));
}

static bool cmd_backlight_cevent(struct cmd_data_base *_data, unsigned event, unsigned modifiers) {
	(void) modifiers;
	struct cmd_backlight_data *data = (struct cmd_backlight_data *)_data;
	if (data->supports_changing) {
		long new_value;
		switch (event) {
			case CEVENT_MOUSE_LEFT:
				new_value = 0;
				break;
			case CEVENT_MOUSE_RIGHT:
				new_value = data->max_brightness;
				break;
			case CEVENT_MOUSE_WHEEL_UP:
			case CEVENT_MOUSE_WHEEL_DOWN: {
				new_value = cmd_backlight_read_value(data->backlight_fd);
				if (unlikely(new_value < 0))
					return false;

				const long change = (data->wheel_step * data->max_brightness + (100 / 2)) / 100;
				if (event == CEVENT_MOUSE_WHEEL_UP) {
					new_value += change;
					if (new_value > data->max_brightness)
						new_value = data->max_brightness;
				} else {
					new_value -= change;
					if (new_value < 0)
						new_value = 0;
				}
				break;
			}
			default:
				return false;
		}
		char res[64];
		int res_len = snprintf(res, sizeof(res), "%ld", new_value);
		lseek(data->backlight_fd, 0, SEEK_SET);
		if (likely(res_len == write(data->backlight_fd, res, (size_t)res_len)))
			cmd_backlight_update_text(data, new_value);
	}
	return false;
}

#define CPU_TEMP_OPTIONS(F) \
	F("device", OPT_TYPE_STR, offsetof(struct cmd_backlight_data, device)), \
	F("format", OPT_TYPE_STR, offsetof(struct cmd_backlight_data, format)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_backlight_data, base.interval)), \
	F("wheel_step", OPT_TYPE_LONG, offsetof(struct cmd_backlight_data, wheel_step)), \

CMD_OPTS_GEN_STRUCTS(cmd_backlight, CPU_TEMP_OPTIONS)

DECLARE_CMD(cmd_backlight) = {
	.name = "backlight",
	.data_size = sizeof (struct cmd_backlight_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_backlight),

	.func_init = cmd_backlight_init,
	.func_destroy = cmd_backlight_destroy,
	.func_recache = cmd_backlight_recache,
	.func_cevent = cmd_backlight_cevent
};
