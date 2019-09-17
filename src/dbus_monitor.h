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

#ifndef DBUS_MONITOR_H
#define DBUS_MONITOR_H

#include "main.h"

#include <systemd/sd-bus.h>

enum dbus_field_type {
	FIELD_STR = 0,
	FIELD_LONG = 1,
	FIELD_DOUBLE = 2,

	FIELD_ARR_STR_FIRST = 3,
	FIELD_ARR_DICT_EXPAND = 4
};
struct dbus_field {
	uint16_t type:3;
	uint16_t offset:13;
};
_Static_assert(sizeof(struct dbus_field) == 2, "incorrect bit width in struct dbus_field");
struct dbus_fields_t {
	const char *const *const names;
	const struct dbus_field *const opts;
	void(*const func_recache)(struct cmd_data_base *data);
	const unsigned size;
	const unsigned data_base_offset;
};

#define DBUS_MONITOR_GEN_FIELDS(name, GEN, _func_recache, data_struct, field) \
	static const char *const name ## _fields_names[] = { GEN(CMD_IMPL_OPTS_GEN_NAME) }; \
	static const struct dbus_field name ## _fields[] __attribute__ ((aligned (2))) = { GEN(CMD_IMPL_OPTS_GEN_DATA) }; \
	static const struct dbus_fields_t name = { .names = name ## _fields_names, .opts = name ## _fields, \
		.size = ARRAY_SIZE(name ## _fields), .func_recache = (_func_recache), \
		.data_base_offset = offsetof(data_struct, field) };

struct dbus_monitor_base {
	const struct dbus_fields_t *fields;
};

void dbus_parse_arr_fields(sd_bus_message *m, void *data);
bool dbus_add_watcher(const char *sender, const char *path, void *dst_data);

#endif // DBUS_MONITOR_H
