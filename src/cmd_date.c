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

#include <time.h>

struct cmd_date_data {
	struct cmd_data_base base;
	char *format;
	char *timezone;
};

static const char *g_curr_tz = NULL;
static const char *g_local_tz = NULL;

static bool cmd_date_init(struct cmd_data_base *_data) {
	struct cmd_date_data *data = (struct cmd_date_data *)_data;
	if (!data->format)
		return false;

	if (!g_local_tz)
		g_local_tz = getenv("TZ");

	if (!data->timezone && g_local_tz)
		data->timezone = strdup(g_local_tz);

	return true;
}

static void cmd_date_destroy(struct cmd_data_base *_data) {
	struct cmd_date_data *data = (struct cmd_date_data *)_data;
	free(data->format);
	free(data->timezone);
}

static bool cmd_date_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_date_data *data = (struct cmd_date_data *)_data;
	(void)update;

	if (data->timezone != g_curr_tz) {
		if (data->timezone)
			setenv("TZ", data->timezone, 1);
		else
			unsetenv("TZ");
		tzset();
		g_curr_tz = data->timezone;
	}

	struct tm tm;
	char timebuf[1024];
	time_t t = time(NULL);
	localtime_r(&t, &tm);

	const size_t len = strftime(timebuf, sizeof(timebuf), data->format, &tm);
	JSON_OUTPUT_K(json_gen, "full_text", timebuf, len);

	return true;
}

#define DATE_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_date_data, base.align)), \
	F("format", OPT_TYPE_STR, offsetof(struct cmd_date_data, format)), \
	F("timezone", OPT_TYPE_STR, offsetof(struct cmd_date_data, timezone))

CMD_OPTS_GEN_STRUCTS(cmd_date, DATE_OPTIONS)

DECLARE_CMD(cmd_date) = {
	.name = "date",
	.data_size = sizeof (struct cmd_date_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_date),

	.func_init = cmd_date_init,
	.func_destroy = cmd_date_destroy,
	.func_output = cmd_date_output
};
