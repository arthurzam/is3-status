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

#include <alloca.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

struct cmd_battery_data {
	struct cmd_data_base base;

	char *format_missing;
	char *format_discharging;
	char *format_charging;
	char *format_full;

	union {
		char *device;
		int bat_fd;
	};
	long last_full_capacity;
	long threshold_time;
	long threshold_pct;

	char cached_output[256];
};

static bool cmd_battery_init(struct cmd_data_base *_data) {
	struct cmd_battery_data *data = (struct cmd_battery_data *)_data;

	if (!data->device)
		return false;
	if (!data->format_missing || !data->format_charging || !data->format_discharging)
		return false;
	if (!data->format_full)
		data->format_full = strdup(data->format_charging);

	data->last_full_capacity = !!data->last_full_capacity;
	data->base.cached_fulltext = data->cached_output;

#define BATTERY_PATH "/sys/devices/virtual/power_supply/"
#define BATTERY_PATH_SUFFIX "/uevent"
	const size_t device_len = strlen(data->device);
	char *path = alloca(strlen(BATTERY_PATH) + device_len + strlen(BATTERY_PATH_SUFFIX) + 1);
	memcpy(path, BATTERY_PATH, strlen(BATTERY_PATH));
	memcpy(path + strlen(BATTERY_PATH), data->device, device_len);
	memcpy(path + strlen(BATTERY_PATH) + device_len, BATTERY_PATH_SUFFIX, strlen(BATTERY_PATH_SUFFIX) + 1);
	free(data->device);
	data->device = NULL;
#undef BATTERY_PATH_SUFFIX
#undef BATTERY_PATH

	data->bat_fd = open(path, O_RDONLY);
	return data->bat_fd >= 0;
}

static void cmd_battery_destroy(struct cmd_data_base *_data) {
	struct cmd_battery_data *data = (struct cmd_battery_data *)_data;
	free(data->format_missing);
	free(data->format_discharging);
	free(data->format_charging);
	free(data->format_full);
	close(data->bat_fd);
}

struct battery_info_t {
	int status;

	int remainingW;
	int remainingAh; // temporary and converted to watt
	int present_rate;
	int voltage;

	int full_design_capacity;
	int full_design_design;
} __attribute__ ((aligned (sizeof(int))));

enum {
	BAT_STS_MISSING = 0,
	BAT_STS_DISCHARGIUNG = 1,
	BAT_STS_CHARGIUNG = 2,
	BAT_STS_FULL = 3,
};

__attribute__((always_inline)) inline bool cmd_battery_parse_file(int fd, struct battery_info_t *info) {
	enum {
		BAT_OPT_STATUS = 0,
		BAT_OPT_INT = 1,
		BAT_OPT_ABS_INT = 2,
	};
#define BAT_OPT(str, type, field) {str, X_STRLEN(str), type, offsetof(struct battery_info_t, field) / sizeof(int)}
	static const struct battery_opt_t {
		// 19 is the longest("ENERGY_FULL_DESIGN") + 1
		char str[19] __attribute__ ((aligned (8)));
		uint8_t str_len;
		uint8_t type:2;
		uint8_t offset:6;
	} g_bat_opts[] = {
		// formatted as a BTree in BFS
		/* 6 */ BAT_OPT("ENERGY_NOW", BAT_OPT_INT, remainingW),
		/* 3 */ BAT_OPT("CURRENT_NOW", BAT_OPT_ABS_INT, present_rate),
		/* 8 */ BAT_OPT("STATUS", BAT_OPT_STATUS, status),
		/* 1 */ BAT_OPT("CHARGE_FULL_DESIGN", BAT_OPT_INT, full_design_design),
		/* 5 */ BAT_OPT("ENERGY_FULL_DESIGN", BAT_OPT_INT, full_design_design),
		/* 7 */ BAT_OPT("POWER_NOW", BAT_OPT_ABS_INT, present_rate),
		/* 9 */ BAT_OPT("VOLTAGE_NOW", BAT_OPT_ABS_INT, voltage),
		/* 0 */ BAT_OPT("CHARGE_FULL", BAT_OPT_INT, full_design_capacity),
		/* 2 */ BAT_OPT("CHARGE_NOW", BAT_OPT_INT, remainingAh),
		/* 4 */ BAT_OPT("ENERGY_FULL", BAT_OPT_INT, full_design_capacity),
	};
#undef BAT_OPT

	char buffer[2048];
	ssize_t buf_len;
	unsigned offset = 0;
	lseek(fd, 0, SEEK_SET);
	while (0 < (buf_len = offset + read(fd, buffer + offset, sizeof(buffer) - 1 - offset))) {
		buffer[buf_len] = '\0';
		char *start = buffer;
		while (true) {
			if (0 == memcmp(start, "POWER_SUPPLY_", 13)) {
				start += 13;
				unsigned pos = 0;
				do {
					const int cmp_res = memcmp(g_bat_opts[pos].str, start, g_bat_opts[pos].str_len);
					if (cmp_res == 0) {
						const struct battery_opt_t *opt = g_bat_opts + pos;
						start += opt->str_len + 1;

						int *const dst = ((int *)info) + opt->offset;
						switch (opt->type) {
							case BAT_OPT_STATUS: {
								if (0 == memcmp(start, "Full", 4))
									*dst = BAT_STS_FULL;
								else if (0 == memcmp(start, "Charging", 8))
									*dst = BAT_STS_CHARGIUNG;
								break;
							} case BAT_OPT_ABS_INT:
								if (*start == '-')
									++start;
								/* fall through */
							case BAT_OPT_INT:
								*dst = atoi(start);
								break;
							default: __builtin_unreachable();
						}
						break;
					} else
						pos = (2 * pos) + (1 + !!(cmp_res < 0));
				} while (pos < ARRAY_SIZE(g_bat_opts));
			}
			char *endl = strchr(start, '\n');
			if (!endl)
				break;
			start = endl + 1;
		}
		offset = (unsigned)(buffer + buf_len - start);
		memmove(buffer, start, offset);
	}
	return true;
}

// generated using command ./scripts/gen-format.py bBt
VPRINT_OPTS(cmd_battery_var_options, {0x00000000, 0x00000000, 0x00000004, 0x00100004});

static void cmd_battery_recache(struct cmd_data_base *_data) {
	struct cmd_battery_data *data = (struct cmd_battery_data *)_data;

	struct battery_info_t info = {BAT_STS_DISCHARGIUNG, -1, -1, -1, -1, -1, -1};
	int full_design = -1;

	/* read file */
	{
		if (cmd_battery_parse_file(data->bat_fd, &info)) {
			full_design = data->last_full_capacity ? info.full_design_capacity : info.full_design_design;
			if (info.remainingW < 0)
				info.remainingW = info.remainingAh;
			if (full_design <= 0 || info.remainingW < 0)
				info.status = BAT_STS_MISSING;
		} else
			info.status = BAT_STS_MISSING;
	}

	double remaining_pct = info.remainingW * 100.0 / full_design;
	if (data->last_full_capacity && remaining_pct > 100.0)
		remaining_pct = 100.0;

	int remaining_time = 0;
	if (info.present_rate > 0) {
		const int val = (info.status == BAT_STS_CHARGIUNG ? full_design - info.remainingW :
						 info.status == BAT_STS_DISCHARGIUNG ? info.remainingW : 0);
		remaining_time = val * 60  / info.present_rate;
	}

	if (info.status == BAT_STS_FULL)
		CMD_COLOR_SET(data, g_general_settings.color_good);
	else if (info.status == BAT_STS_CHARGIUNG)
		CMD_COLOR_SET(data, g_general_settings.color_degraded);
	else if (info.status == BAT_STS_DISCHARGIUNG && (remaining_pct < (int)data->threshold_pct || remaining_time < data->threshold_time))
		CMD_COLOR_SET(data, g_general_settings.color_bad);
	else
		CMD_COLOR_CLEAN(data);

	/* Static check for format relative position */
	{
#define BAT_POS_CHECK(pos, field) \
_Static_assert(offsetof(struct cmd_battery_data, field) - offsetof(struct cmd_battery_data, format_missing) == (pos) * sizeof(char *), \
	"Wrong position for " # field)
		BAT_POS_CHECK(BAT_STS_MISSING, format_missing);
		BAT_POS_CHECK(BAT_STS_DISCHARGIUNG, format_discharging);
		BAT_POS_CHECK(BAT_STS_CHARGIUNG, format_charging);
		BAT_POS_CHECK(BAT_STS_FULL, format_full);
#undef BAT_POS_CHECK
	}
	const char *output_format = *(&data->format_missing + info.status);
	unsigned res;
	struct vprint ctx = {cmd_battery_var_options, output_format, data->cached_output, data->cached_output + sizeof(data->cached_output)};
	while ((res = vprint_walk(&ctx)) != 0) {
		switch (res) {
			case 'b':
				vprint_itoa(&ctx, (int)remaining_pct);
				break;
			case 'B':
				vprint_dtoa(&ctx, remaining_pct);
				break;
			case 't':
				vprint_time(&ctx, remaining_time);
				break;
		}
	}
}

#define BAT_OPTIONS(F) \
	F("device", OPT_TYPE_STR, offsetof(struct cmd_battery_data, device)), \
	F("format_charging", OPT_TYPE_STR, offsetof(struct cmd_battery_data, format_charging)), \
	F("format_discharging", OPT_TYPE_STR, offsetof(struct cmd_battery_data, format_discharging)), \
	F("format_full", OPT_TYPE_STR, offsetof(struct cmd_battery_data, format_full)), \
	F("format_missing", OPT_TYPE_STR, offsetof(struct cmd_battery_data, format_missing)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_battery_data, base.interval)), \
	F("last_full_capacity", OPT_TYPE_LONG, offsetof(struct cmd_battery_data, last_full_capacity)), \
	F("threshold_pct", OPT_TYPE_LONG, offsetof(struct cmd_battery_data, threshold_pct)), \
	F("threshold_time", OPT_TYPE_LONG, offsetof(struct cmd_battery_data, threshold_time)), \

CMD_OPTS_GEN_STRUCTS(cmd_battery, BAT_OPTIONS)

DECLARE_CMD(cmd_battery) = {
	.name = "battery",
	.data_size = sizeof (struct cmd_battery_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_battery),

	.func_init = cmd_battery_init,
	.func_destroy = cmd_battery_destroy,
	.func_recache = cmd_battery_recache
};
