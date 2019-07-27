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

struct cmd_battery_data {
	struct cmd_data_base base;

	char *format_missing;
	char *format_discharging;
	char *format_charging;
	char *format_full;

	char *path;
	long last_full_capacity;
	long threshold_time;
	long threshold_pct;

	const char *cached_color;
	char cached_output[256];
};

struct battery_info_t {
	int status;

	int remainingW;
	int remainingAh; // temprorary and converted to watt
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

static bool cmd_battery_init(struct cmd_data_base *_data) {
	struct cmd_battery_data *data = (struct cmd_battery_data *)_data;

	if (data->path == NULL)
		return false;
	if (data->format_missing == NULL || data->format_charging == NULL || data->format_discharging == NULL)
		return false;
	if (data->format_full == NULL)
		data->format_full = strdup(data->format_charging);

	data->last_full_capacity = !!data->last_full_capacity;

	return true;
}

static void cmd_battery_destroy(struct cmd_data_base *_data) {
	struct cmd_battery_data *data = (struct cmd_battery_data *)_data;
	free(data->format_missing);
	free(data->format_discharging);
	free(data->format_charging);
	free(data->format_full);
	free(data->path);
}

static void cmd_battery_parse_file(FILE *batFile, struct battery_info_t *info) {
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
		BAT_OPT("CHARGE_FULL", BAT_OPT_INT, full_design_capacity),
		BAT_OPT("CHARGE_FULL_DESIGN", BAT_OPT_INT, full_design_design),
		BAT_OPT("CHARGE_NOW", BAT_OPT_INT, remainingAh),
		BAT_OPT("CURRENT_NOW", BAT_OPT_ABS_INT, present_rate),
		BAT_OPT("ENERGY_FULL", BAT_OPT_INT, full_design_capacity),
		BAT_OPT("ENERGY_FULL_DESIGN", BAT_OPT_INT, full_design_design),
		BAT_OPT("ENERGY_NOW", BAT_OPT_INT, remainingW),
		BAT_OPT("POWER_NOW", BAT_OPT_ABS_INT, present_rate),
		BAT_OPT("STATUS", BAT_OPT_STATUS, status),
		BAT_OPT("VOLTAGE_NOW", BAT_OPT_ABS_INT, voltage)
	};
#undef BAT_OPT

	char line[256];
	while (fgets(line, sizeof(line), batFile)) {
		char *ptr = line;
		const struct battery_opt_t *opt = NULL;
		if (0 != memcmp(line, "POWER_SUPPLY_", 13))
			continue;
		ptr += 13;
		/* binary search opt */
		{
			int bottom = 0;
			int top = (sizeof(g_bat_opts) / sizeof(g_bat_opts[0])) - 1;
			while (bottom <= top) {
				const int mid = (bottom + top) / 2;
				const int cmp_res = memcmp(g_bat_opts[mid].str, ptr, g_bat_opts[mid].str_len);
				if (cmp_res == 0) {
					opt = g_bat_opts + mid;
					ptr += opt->str_len + 1;
					break;
				} else if (cmp_res > 0)
					top = mid - 1;
				else
					bottom = mid + 1;
			}
			if (bottom > top) // when not found -> opt == NULL
				continue;
		}

		int *const dst = ((int *)info) + opt->offset;
		switch (opt->type) {
			case BAT_OPT_STATUS: {
				const int x = strcmp(ptr, "Full");
				if (x == 0)
					*dst = BAT_STS_FULL;
				else if (x < 0 && 0 == strcmp(ptr, "Charging"))
					*dst = BAT_STS_CHARGIUNG;
				break;
			} case BAT_OPT_ABS_INT:
				if (*ptr == '-')
					++ptr;
				/* fall through */
			case BAT_OPT_INT:
				*dst = atoi(ptr);
				break;
		}
	}
}

// generaterd using command ./scripts/gen-format.py bBt
VPRINT_OPTS(cmd_battery_data_var_options, {0x00000000, 0x00000000, 0x00000004, 0x00100004});

static bool cmd_battery_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_battery_data *data = (struct cmd_battery_data *)_data;

	if (update) {
		struct battery_info_t info = {BAT_STS_DISCHARGIUNG, -1, -1, -1, -1, -1, -1};
		int full_design = -1;

		/* read file */
		{
			FILE *batFile = fopen(data->path, "r");
			if (batFile) {
				cmd_battery_parse_file(batFile, &info);
				fclose(batFile);

				full_design = data->last_full_capacity ? info.full_design_capacity : info.full_design_design;
				if (info.remainingAh != -1 && info.remainingW == -1) {
					info.present_rate = (int)(((float)info.voltage / 1000.0f) * ((float)info.present_rate / 1000.0f));
					if (info.voltage != -1) {
						info.remainingW = (int)(((float)info.voltage / 1000.0f) * ((float)info.remainingAh / 1000.0f));
						full_design = (int)(((float)info.voltage / 1000.0f) * ((float)full_design / 1000.0f));
					}
				}

				if (full_design == -1 || info.remainingW == -1)
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
			remaining_time = val * 3600 / info.present_rate;
		}

		data->cached_color = (info.status == BAT_STS_FULL ? g_general_settings.color_good :
							  info.status == BAT_STS_CHARGIUNG ? g_general_settings.color_degraded : NULL);
		if (info.status == BAT_STS_DISCHARGIUNG && (remaining_pct < (int)data->threshold_pct || remaining_time < data->threshold_time))
			data->cached_color = g_general_settings.color_bad;

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
		int res;
		struct vprint ctx = {cmd_battery_data_var_options, output_format, data->cached_output, sizeof(data->cached_output)};
		while ((res = vprint_walk(&ctx)) >= 0) {
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

	if (data->cached_color)
		JSON_OUTPUT_COLOR(json_gen, data->cached_color);
	JSON_OUTPUT_KV(json_gen, "full_text", data->cached_output);
	return true;
}

#define BAT_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_battery_data, base.align)), \
	F("format_charging", OPT_TYPE_STR, offsetof(struct cmd_battery_data, format_charging)), \
	F("format_discharging", OPT_TYPE_STR, offsetof(struct cmd_battery_data, format_discharging)), \
	F("format_full", OPT_TYPE_STR, offsetof(struct cmd_battery_data, format_full)), \
	F("format_missing", OPT_TYPE_STR, offsetof(struct cmd_battery_data, format_missing)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_battery_data, base.interval)), \
	F("last_full_capacity", OPT_TYPE_LONG, offsetof(struct cmd_battery_data, last_full_capacity)), \
	F("path", OPT_TYPE_STR, offsetof(struct cmd_battery_data, path)), \
	F("threshold_pct", OPT_TYPE_LONG, offsetof(struct cmd_battery_data, threshold_pct)), \
	F("threshold_time", OPT_TYPE_LONG, offsetof(struct cmd_battery_data, threshold_time)), \

static const char *const cmd_battery_data_options_names[] = {
	BAT_OPTIONS(CMD_OPTS_GEN_NAME)
};

static const struct cmd_option cmd_battery_data_options[] = {
	BAT_OPTIONS(CMD_OPTS_GEN_DATA)
};

DECLARE_CMD(cmd_battery) = {
	.name = "battery",
	.data_size = sizeof (struct cmd_battery_data),

	.opts = {
		.names = cmd_battery_data_options_names,
		.opts = cmd_battery_data_options,
		.size = sizeof(cmd_battery_data_options) / sizeof(cmd_battery_data_options[0])
	},

	.func_init = cmd_battery_init,
	.func_destroy = cmd_battery_destroy,
	.func_output = cmd_battery_output
};
