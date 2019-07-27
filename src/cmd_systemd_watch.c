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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <systemd/sd-bus.h>

struct cmd_systemd_watch_data {
	struct cmd_data_base base;
	char *cached_result;
	sd_bus *bus;
	char *unit_path;

	long use_user_bus;
	char *service_name;
};

static bool cmd_systemd_watch_init(struct cmd_data_base *_data) {
	struct cmd_systemd_watch_data *data = (struct cmd_systemd_watch_data *)_data;
	int r;

	if (data->service_name == NULL)
		return false;

	r = data->use_user_bus ? sd_bus_open_user(&data->bus) : sd_bus_open_system(&data->bus);
	if (r < 0) {
		fprintf(stderr, "is3-status: systemd_watch: Failed to connect to %s bus: %s\n",
				data->use_user_bus ? "user" : "system", strerror(-r));
		return false;
	}
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL;

	r = sd_bus_call_method(data->bus,
						   "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
						   "org.freedesktop.systemd1.Manager", "LoadUnit",
						   &error, &m, "s", data->service_name);
	if (r < 0)
		fprintf(stderr, "is3-status: systemd_watch: Failed to issue method call: %s\n", error.message);
	else if ((r = sd_bus_message_read(m, "o", &data->unit_path)) < 0)
		fprintf(stderr, "is3-status: systemd_watch: Failed to parse response message: %s\n", strerror(-r));
	else // successful
		data->unit_path = strdup(data->unit_path);

	sd_bus_error_free(&error);
	sd_bus_message_unref(m);
	return r >= 0;
}

static void cmd_systemd_watch_destroy(struct cmd_data_base *_data) {
	struct cmd_systemd_watch_data *data = (struct cmd_systemd_watch_data *)_data;
	sd_bus_unref(data->bus);
	free(data->service_name);
	free(data->unit_path);
	free(data->cached_result);
}

static bool cmd_systemd_watch_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_systemd_watch_data *data = (struct cmd_systemd_watch_data *)_data;

	if (update) {
		sd_bus_error error = SD_BUS_ERROR_NULL;
		int r;

		free(data->cached_result);
		data->cached_result = NULL;
		if (0 > (r = sd_bus_get_property_string(data->bus,
												"org.freedesktop.systemd1", data->unit_path,
												"org.freedesktop.systemd1.Unit", "ActiveState",
												&error, &data->cached_result))) {
			fprintf(stderr, "is3-status: systemd_watch: Failed to get property \'ActiveState\': %s\n", error.message);
		}
		sd_bus_error_free(&error);
	}

	JSON_OUTPUT_KV(json_gen, "full_text", data->cached_result);

	return true;
}

#define SYSTEMD_WATCH_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_systemd_watch_data, base.align)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_systemd_watch_data, base.interval)), \
	F("service", OPT_TYPE_STR, offsetof(struct cmd_systemd_watch_data, service_name)), \
	F("use_user_bus", OPT_TYPE_LONG, offsetof(struct cmd_systemd_watch_data, use_user_bus))

static const char *const cmd_systemd_watch_options_names[] = {
	SYSTEMD_WATCH_OPTIONS(CMD_OPTS_GEN_NAME)
};

static const struct cmd_option cmd_systemd_watch_options[] = {
	SYSTEMD_WATCH_OPTIONS(CMD_OPTS_GEN_DATA)
};

DECLARE_CMD(cmd_systemd_watch) = {
	.name = "systemd_watch",
	.data_size = sizeof (struct cmd_systemd_watch_data),

	.opts = {
		.names = cmd_systemd_watch_options_names,
		.opts = cmd_systemd_watch_options,
		.size = sizeof(cmd_systemd_watch_options) / sizeof(cmd_systemd_watch_options[0])
	},

	.func_init = cmd_systemd_watch_init,
	.func_destroy = cmd_systemd_watch_destroy,
	.func_output = cmd_systemd_watch_output
};
