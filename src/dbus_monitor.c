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

#include "dbus_monitor.h"
#include "fdpoll.h"

#include <stdio.h>
#include <stdlib.h>

static sd_bus *g_dbus_monitor_bus = NULL;

static const struct dbus_field *find_field(const struct dbus_fields_t *fields, const char *name) {
	int bottom = 0;
	int top = (int)fields->size - 1;

	while (bottom <= top) {
		const int mid = (bottom + top) / 2;
		const int cmp_res = strcmp(fields->names[mid], name);
		if (cmp_res == 0)
			return fields->opts + mid;
		else if (cmp_res > 0)
			top = mid - 1;
		else
			bottom = mid + 1;
	}
	return NULL;
}

void dbus_parse_arr_fields(sd_bus_message *m, void *data) {
	// msg format: a{sv}
	const struct dbus_fields_t *const fields = ((struct dbus_monitor_base *)data)->fields;

	sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sv}");
	while (0 < sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv")) {

		const char *key, *value;
		sd_bus_message_read_basic(m, SD_BUS_TYPE_STRING, &key);
		sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, NULL);

		const struct dbus_field *f = find_field(fields, key);
		if (f) {
			void *dst = (uint8_t *)data + f->offset;
			switch (f->type) {
				case FIELD_STR:
					sd_bus_message_read_basic(m, SD_BUS_TYPE_STRING, &value);
					free(*(char **)dst);
					*(char **)dst = (value[0] == '\0' ? NULL : strdup(value));
					break;
				case FIELD_LONG:
					sd_bus_message_read_basic(m, SD_BUS_TYPE_INT64, dst);
					break;
				case FIELD_DOUBLE:
					sd_bus_message_read_basic(m, SD_BUS_TYPE_DOUBLE, dst);
					break;
				case FIELD_ARR_STR_FIRST:
					sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");

					sd_bus_message_read_basic(m, SD_BUS_TYPE_STRING, &value);
					free(*(char **)dst);
					*(char **)dst = (value[0] == '\0' ? NULL : strdup(value));

					sd_bus_message_skip(m, NULL);
					sd_bus_message_exit_container(m); // exit array "s"
					break;
				case FIELD_ARR_DICT_EXPAND:
					dbus_parse_arr_fields(m, data);
					break;
				default: __builtin_unreachable();
			}
		} else
			sd_bus_message_skip(m, NULL);
		sd_bus_message_exit_container(m); // exit variant "v"
		sd_bus_message_exit_container(m); // exit dict "sv"
	}
	sd_bus_message_exit_container(m); // exit array "{sv}"
}

static int dbus_monitor_systemd_handler(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
	(void)ret_error;
	// msg format: sa{sv}as

	sd_bus_message_skip(m, NULL); // first string is the interface name - unneeded for now
	dbus_parse_arr_fields(m, userdata);
	return 0;
}


static bool dbus_monitor_handler(void *data) {
	(void)data;
	while (0 < sd_bus_process(g_dbus_monitor_bus, NULL));
	return false;
}

static bool dbus_monitor_setup() {
	int r = sd_bus_open_user(&g_dbus_monitor_bus);
	if (r < 0) {
		fprintf(stderr, "is3-status: dbus: Failed to connect to user bus: %s\n", strerror(-r));
		return false;
	}
	fdpoll_add(sd_bus_get_fd(g_dbus_monitor_bus), dbus_monitor_handler, NULL);
	return true;
}

bool dbus_add_watcher(const char *sender, const char *path, void *dst_data) {
	if (!g_dbus_monitor_bus && !dbus_monitor_setup())
		return false;

	sd_bus_match_signal(g_dbus_monitor_bus, NULL, sender, path,
						"org.freedesktop.DBus.Properties", "PropertiesChanged",
						dbus_monitor_systemd_handler, dst_data);

	return true;
}
