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

#include <stdlib.h>
#include <string.h>

#include <yajl/yajl_parse.h>

#include <unistd.h>

#include "main.h"
#include "ini_parser.h"
#include "fdpoll.h"

#if __GNUC__
	#pragma GCC optimize ("-Os")
#endif

enum CURRENT_KEY {
	CURRENT_KEY_UNSET = 0,
	CURRENT_KEY_NAME = 1, // "name"
	CURRENT_KEY_INSTANCE = 2, // "instance"
	CURRENT_KEY_BUTTON = 3, // "button"
	CURRENT_KEY_MODIFIERS = 4, // "modifiers"
};

static struct {
	struct runs_list *runs;
	yajl_handle yajl_parse_handle;

	char *name;
	char *instance;
	uint8_t button;
	uint8_t current_key;
	bool force_update;
} g_cevent_data;

static int cevent_integer(void *ctx, long long value) {
	(void) ctx;
	if (g_cevent_data.current_key == CURRENT_KEY_BUTTON)
		g_cevent_data.button = (uint8_t)value;
	return true;
}

static int cevent_string(void *ctx, const unsigned char *str, size_t len) {
	(void) ctx;
	char **dst;
	switch (g_cevent_data.current_key) {
		case CURRENT_KEY_NAME: dst = &g_cevent_data.name; break;
		case CURRENT_KEY_INSTANCE: dst = &g_cevent_data.instance; break;
		default: return true;
	}
	free(*dst);
	*dst = malloc(len + 1);
	memcpy(*dst, str, len);
	(*dst)[len] = '\0';
	return true;
}

static int cevent_map_key(void *ctx, const unsigned char *str, size_t len) {
	(void) ctx;
	g_cevent_data.current_key = CURRENT_KEY_UNSET;
	switch (len) {
		case 4:
			if(likely(0 == memcmp(str, "name", 4)))
				g_cevent_data.current_key = CURRENT_KEY_NAME;
			break;
		case 6:
			if(likely(0 == memcmp(str, "button", 6)))
				g_cevent_data.current_key = CURRENT_KEY_BUTTON;
			break;
		case 8:
			if(likely(0 == memcmp(str, "instance", 8)))
				g_cevent_data.current_key = CURRENT_KEY_INSTANCE;
			break;
		case 9:
			if(likely(0 == memcmp(str, "modifiers", 9)))
				g_cevent_data.current_key = CURRENT_KEY_MODIFIERS;
			break;
	}
	return true;
}

static int cevent_start_map(void *ctx) {
	(void) ctx;
	free(g_cevent_data.name);
	free(g_cevent_data.instance);
	g_cevent_data.name = NULL;
	g_cevent_data.instance = NULL;
	g_cevent_data.button = __CEVENT_MOUSE_UNSET;
	g_cevent_data.current_key = CURRENT_KEY_UNSET;
	return true;
}

static int cevent_end_map(void *ctx) {
	(void) ctx;

	if (g_cevent_data.name == NULL || g_cevent_data.button == __CEVENT_MOUSE_UNSET)
		return true;

	FOREACH_RUN(run, g_cevent_data.runs) {
		if ((0 == strcmp(run->vtable->name, g_cevent_data.name)) &&
				(g_cevent_data.instance == run->instance/* == NULL*/ || 0 == strcmp(run->instance, g_cevent_data.instance))) {
			if (run->vtable->func_cevent && run->vtable->func_cevent(run->data, g_cevent_data.button))
				g_cevent_data.force_update = true;
			break;
		}
	}
	return true;
}

static const yajl_callbacks cevent_callbacks = {
	.yajl_integer = cevent_integer,
	.yajl_string = cevent_string,
	.yajl_map_key = cevent_map_key,
	.yajl_start_map = cevent_start_map,
	.yajl_end_map = cevent_end_map,
};


bool handle_click_event(void *arg) {
	(void)arg;

	uint8_t input[2048];
	g_cevent_data.force_update = false;
	ssize_t ret = read(STDIN_FILENO, input, sizeof(input));
	if (ret > 0)
		yajl_parse(g_cevent_data.yajl_parse_handle, input, (size_t)ret);
	else
		close(STDIN_FILENO);
	return g_cevent_data.force_update;
}

void init_cevent_handle(struct runs_list *runs) {
	g_cevent_data.runs = runs;
	g_cevent_data.yajl_parse_handle = yajl_alloc(&cevent_callbacks, NULL, NULL);
	fdpoll_add(STDIN_FILENO, handle_click_event, NULL);
}
