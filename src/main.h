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

#include <yajl_gen.h>

extern struct general_settings_t {
	const char *align;
	long interval;
	char color_bad[8];
	char color_degraded[8];
	char color_good[8];
} g_general_settings;

enum cmd_option_type {
	OPT_TYPE_LONG = 0,
	OPT_TYPE_STR = 1,
	OPT_TYPE_COLOR = 2,
	OPT_TYPE_ALIGN = 3
};
struct cmd_option {
	uint16_t type:2;
	uint16_t offset:14;
};
#define CMD_OPTS_GEN_NAME(name, ...) name
#define CMD_OPTS_GEN_DATA(name, ...) {__VA_ARGS__}

struct cmd_opts {
	const char *const *const names;
	const struct cmd_option *const opts;
	const unsigned size;
};

struct cmd_data_base {
	const char *align;
	long interval;
};

enum click_event {
	CEVENT_MOUSE_LEFT = 1,
	CEVENT_MOUSE_MIDDLE = 2,
	CEVENT_MOUSE_RIGHT = 3,
	CEVENT_MOUSE_WHEEL_UP = 4,
	CEVENT_MOUSE_WHEEL_DOWN = 5
};

struct cmd {
	bool(*func_output)(struct cmd_data_base *data, yajl_gen json_gen, bool update);
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
} __attribute__ ((aligned (__BIGGEST_ALIGNMENT__)));
#define DECLARE_CMD(name) static const struct cmd name __attribute((used, section("cmd_array")))

__attribute__((always_inline)) inline void json_output(yajl_gen json_gen, const char *key, size_t key_size, const char *value, size_t value_size) {
	yajl_gen_string(json_gen, (const unsigned char *)key, key_size);
	yajl_gen_string(json_gen, (const unsigned char *)value, value_size);
}
#define JSON_OUTPUT_K(json_gen,key,value,value_size) json_output((json_gen), (key), strlen(key), value, value_size)
#define JSON_OUTPUT_KV(json_gen,key,value) JSON_OUTPUT_K(json_gen, key, value, strlen(value))
#define JSON_OUTPUT_COLOR(json_gen,value) json_output((json_gen), "color", 5, (value), 7)

#define X_STRLEN(str) ((sizeof(str)/sizeof(str[0]))-sizeof(str[0]))

#endif // GENERAL_H
