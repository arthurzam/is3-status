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

#ifndef GENERAL_H
#define GENERAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

extern struct general_settings_t {
	const char *align;
	long interval;
	char color_bad[8];
	char color_degraded[8];
	char color_good[8];
} g_general_settings;

enum cmd_option_type {
	OPT_TYPE_LONG = 0, ///< regular long variable
	OPT_TYPE_STR = 1, ///< malloced char *
	OPT_TYPE_COLOR = 2, ///< color variable of type char[8]
	OPT_TYPE_ALIGN = 3, ///< align variable of type const char*, which would point to the alignment
	/**
	  * should be a long variable, but with special parsing for suffix:
	  *  if using % suffix, the long would be a negetive number;
	  *  otherwise the suffix should be a byte suffix (ex. MB, GiB) and would be positive
	  */
	OPT_TYPE_BYTE_THRESHOLD = 4,
};
struct cmd_option {
	uint16_t type:3;
	uint16_t offset:13;
};
_Static_assert(sizeof(struct cmd_option) == 2, "incorrect bit width in struct cmd_option");
#define CMD_IMPL_OPTS_GEN_NAME(name, ...) name
#define CMD_IMPL_OPTS_GEN_DATA(name, ...) {__VA_ARGS__}

#define CMD_OPTS_GEN_STRUCTS(name, GEN) \
	static const char *const name ## _options_names[] = { GEN(CMD_IMPL_OPTS_GEN_NAME) }; \
	static const struct cmd_option name ## _options[] __attribute__ ((aligned (2))) = { GEN(CMD_IMPL_OPTS_GEN_DATA) };
#define CMD_OPTS_GEN_DATA(name) { \
		.names = name ## _options_names, .opts = name ## _options, \
		.size = ARRAY_SIZE(name ## _options) \
	}

struct cmd_opts {
	const char *const *const names;
	const struct cmd_option *const opts;
	const unsigned size;
};

struct cmd_data_base {
	const char *align;
	long interval;
	char *cached_fulltext;
	char cached_color[8];
};

enum click_event {
	CEVENT_MOUSE_LEFT = 1,
	CEVENT_MOUSE_MIDDLE = 2,
	CEVENT_MOUSE_RIGHT = 3,
	CEVENT_MOUSE_WHEEL_UP = 4,
	CEVENT_MOUSE_WHEEL_DOWN = 5
};

#define CMD_USE_ALIGNMENT 8
struct cmd {
	bool(*func_recache)(struct cmd_data_base *data);
	bool(*func_cevent)(struct cmd_data_base *data, int event);
	/**
	 * @brief Initialize the instance
	 *
	 * This function is called after config was loaded!
	 */
	bool(*func_init)(struct cmd_data_base *data);
	/**
	 * @brief Free all memory in data
	 *
	 * Shouldn't free the data structure itself
	 */
	void(*func_destroy)(struct cmd_data_base *data);

	const char *const name;
	const struct cmd_opts opts;
	const unsigned data_size;
} __attribute__ ((aligned (CMD_USE_ALIGNMENT)));
#define DECLARE_CMD(name) static const struct cmd name __attribute((used, section("cmd_array"), aligned(CMD_USE_ALIGNMENT)))

#define CMD_COLOR_SET(data, color) memcpy((data)->base.cached_color, (color), 8)
#define CMD_COLOR_CLEAN(data) (data)->base.cached_color[0] = '\0'

#define X_STRLEN(str) ((sizeof(str)/sizeof(str[0]))-sizeof(str[0]))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

_Static_assert(sizeof(char) == 1, "If it isn't of size 1 byte, a lot of code is incorrect!");

#define unlikely(expr) __builtin_expect(!!(expr), 0)
#define likely(expr) __builtin_expect(!!(expr), 1)

#endif // GENERAL_H
