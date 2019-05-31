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

#include "ini_parser.h"
#include "main.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static char *strip_str(char *begin, char *end) {
	for (; isspace(*begin) && begin < end; ++begin);
	for (; isspace(*end) && begin <= end; --end)
		*end = '\0';
	return begin;
}

extern const struct cmd __start_cmd_array;
extern const struct cmd __stop_cmd_array;

static const struct cmd *find_cmd(const char *name) {
	for (const struct cmd *iter = &__start_cmd_array; iter < &__stop_cmd_array; ++iter)
		if (0 == strcmp(name, iter->name))
			return iter;
	return NULL;
}

static const struct cmd_option *find_cmd_option(const struct cmd_opts *cmd_opts, const char *name) {
	unsigned bottom = 0;
	unsigned top = cmd_opts->size - 1;

	while (bottom <= top) {
		const unsigned mid = (bottom + top) / 2;
		const int cmp_res = strcmp(cmd_opts->names[mid], name);
		if (cmp_res == 0)
			return cmd_opts->opts + mid;
		else if (cmp_res > 0)
			top = mid - 1;
		else
			bottom = mid + 1;
	}
	return NULL;
}

static bool parse_config(void *cmd_data, const struct cmd_opts *cmd_opts, char *ptr) {
	char *value = strchr(ptr, '=');
	if (value == NULL) {
		fprintf(stderr, "Bad line [%s]\n", ptr);
		return false;
	}
	*value = '\0';

	ptr = strip_str(ptr, value - 1);
	const struct cmd_option *cmd_option = find_cmd_option(cmd_opts, ptr);
	if (cmd_option == NULL) {
		fprintf(stderr, "Unknown Option [%s]\n", ptr);
		return false;
	}
	if (isspace(value[1])) // remove first space if found
		++value;
	{
		size_t pos = strlen(value + 1);
		if (value[pos] == '\n')
			value[pos] = '\0';
		++value;
	}
	void *dst = (uint8_t *)cmd_data + cmd_option->offset;
	switch (cmd_option->type) {
		case OPT_TYPE_STR: {
			char *str = strdup(value);
			memcpy(dst, &str, sizeof(str));
			break;
		}
		case OPT_TYPE_LONG: {
			long l = atol(value);
			memcpy(dst, &l, sizeof(l));
			break;
		}
		case OPT_TYPE_COLOR: {
			char color[8];
			if (value[0] != '#' || value[7] != '\0') {
				fprintf(stderr, "Color is incorrect [%s]\n", value);
				return false;
			}
			for (unsigned i = 1; i < 7; i++) {
				if (((value[i] < '0') | (value[i] > '9')) & ((value[i] < 'A') | (value[i] > 'Z'))) {
					fprintf(stderr, "Color is incorrect [%s]\n", value);
					return false;
				}
			}
			memcpy(dst, &color, sizeof(color));
			break;
		}
		case OPT_TYPE_ALIGN: {
			const char *align = NULL;
			if (0 == memcmp(value, "left", 5))
				align = "left";
			else if (0 == memcmp(value, "right", 6))
				align = "right";
			else if (0 == memcmp(value, "center", 7))
				align = "center";
			memcpy(dst, &align, sizeof(align));
			break;
		}
	}

	return true;
}


#define GENERAL_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct general_settings_t, align)), \
	F("color_bad", OPT_TYPE_COLOR, offsetof(struct general_settings_t, color_bad)), \
	F("color_degraded", OPT_TYPE_COLOR, offsetof(struct general_settings_t, color_degraded)), \
	F("color_good", OPT_TYPE_COLOR, offsetof(struct general_settings_t, color_good)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct general_settings_t, interval))

static const char *const general_options_names[] = {
	GENERAL_OPTIONS(CMD_OPTS_GEN_NAME)
};
static const struct cmd_option general_date_options[] = {
	GENERAL_OPTIONS(CMD_OPTS_GEN_DATA)
};
static const struct cmd_opts general_opts = {
	.names = general_options_names,
	.opts = general_date_options,
	.size = sizeof(general_date_options) / sizeof(general_date_options[0])
};
struct general_settings_t g_general_settings = {0};

struct runs_list ini_parse(FILE *ini) {
	struct run_instance *runs = NULL, *curr = NULL;
	unsigned res_size = 0;
	char buffer[1024], *ptr;
	buffer[sizeof(buffer) - 1] = '\0';

	for(unsigned lineno = 0; !feof(ini); ++lineno) {
		if (NULL == fgets(buffer, sizeof(buffer) - 1, ini))
			break;
		ptr = buffer;

		for (; isspace(*ptr); ++ptr);

		if (ptr[0] == '\0' || ptr[0] == '#')
			continue;
		else if (ptr[0] == '[') {
			++ptr;
			for (; isspace(*ptr); ++ptr);
			char *space = strchr(ptr, ' ');
			char *ender = strchr(space ? space + 1 : ptr, ']');
			if (ender == NULL) {
				fprintf(stderr, "Incorrect section name [%s]\n", buffer);
				goto _error;
			}
			*ender = '\0';
			if (space == NULL) {
				space = ender;
			} else {
				*space = '\0';
				space = strip_str(space + 1, ender - 1);
			}
			ptr = strip_str(ptr, space);

			const struct cmd *cmd = find_cmd(ptr);
			if (cmd == NULL) {
				fprintf(stderr, "Could not find module [%s]\n", ptr);
				goto _error;
			}

			++(res_size);
			runs = realloc(runs, res_size * sizeof(struct run_instance));
			curr = runs + (res_size - 1);
			curr->vtable = cmd;
			curr->instance = (space == ender ? NULL : strdup(space));
			curr->data = calloc(cmd->data_size, 1);
		} else {
			void *data = curr ? (void *)curr->data : (void *)&g_general_settings;
			const struct cmd_opts *opts = curr ? &curr->vtable->opts : &general_opts;
			if (!parse_config(data, opts, ptr))
				goto _error;
		}
	}
	struct runs_list res = {runs, runs + res_size};
	return res;

_error:
	free(runs);
	struct runs_list error_res = {NULL, NULL};
	return error_res;
}

void free_all_run_instances(struct runs_list *runs) {
	for(struct run_instance *run = runs->runs_begin; run != runs->runs_end; run++) {
		run->vtable->func_destroy(run->data);
		free(run->data);
		free(run->instance);
	}
	free(runs->runs_begin);
}

#ifdef TESTS

int test_cmd_array_correct(void) {
#define TEST_ERR(...) ((void)fprintf(stderr, __VA_ARGS__), false)
#define ERR_STR(str) "test_cmd_array_correct: "str"\n"
	if ((size_t)(((const char *)&__stop_cmd_array) - ((const char *)&__start_cmd_array)) % sizeof(struct cmd) != 0) {
		return TEST_ERR(ERR_STR("somehow cmd_array section's pointers are incorrect"));
	}
	for (const struct cmd *iter = &__start_cmd_array; iter < &__stop_cmd_array; ++iter) {
		if (!iter->name)
			return TEST_ERR(ERR_STR("empty cmd name"));
		if (!iter->func_init || !iter->func_destroy || !iter->func_output)
			return TEST_ERR(ERR_STR("cmd %s: must have function is empty"), iter->name);

		for(unsigned i = 1; i < iter->opts.size; ++i)
			if (0 <= strcmp(iter->opts.names[i - 1], iter->opts.names[i]))
				return TEST_ERR(ERR_STR("cmd %s: options not sorted"), iter->name);

		const struct {
			const char *name;
			unsigned type;
		} base_opts[] = {
			{"align", OPT_TYPE_ALIGN},
			{"interval", OPT_TYPE_LONG},
		};
		for (size_t i = 0; i < sizeof(base_opts) / sizeof(base_opts[0]); ++i) {
			const struct cmd_option *cmd_option = find_cmd_option(&iter->opts, base_opts[i].name);
			if(cmd_option && cmd_option->type != base_opts[i].type)
				return TEST_ERR(ERR_STR("cmd %s: incorrect type for %s"), iter->name, base_opts[i].name);
		}
	}
	return true;
}

#endif
