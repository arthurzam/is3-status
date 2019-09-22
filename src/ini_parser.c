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
#include "vprint.h"

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
	int bottom = 0;
	int top = (int)cmd_opts->size - 1;

	while (bottom <= top) {
		const int mid = (bottom + top) / 2;
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

static bool parse_assignment(void *cmd_data, const struct cmd_opts *cmd_opts, char *ptr) {
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
			free(*((char **)dst));
			*((char **)dst) = strdup(value);
			break;
		} case OPT_TYPE_LONG: {
			*((long *)dst) = atol(value);
			break;
		} case OPT_TYPE_COLOR: {
			if (value[0] != '#' || value[7] != '\0') {
				fprintf(stderr, "Color is incorrect [%s]\n", value);
				return false;
			}
			for (unsigned i = 1; i < 7; i++) {
				if (!isalpha(value[i])) {
					fprintf(stderr, "Color is incorrect [%s]\n", value);
					return false;
				}
			}
			memcpy(dst, value, 8);
			break;
		} case OPT_TYPE_BYTE_THRESHOLD: {
			*((long *)dst) = parse_human_bytes(value);
			break;
		} default: __builtin_unreachable();
	}

	return true;
}


#define GENERAL_OPTIONS(F) \
	F("color_bad", OPT_TYPE_COLOR, offsetof(struct general_settings_t, color_bad)), \
	F("color_degraded", OPT_TYPE_COLOR, offsetof(struct general_settings_t, color_degraded)), \
	F("color_good", OPT_TYPE_COLOR, offsetof(struct general_settings_t, color_good)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct general_settings_t, interval))
CMD_OPTS_GEN_STRUCTS(general, GENERAL_OPTIONS)
static const struct cmd_opts general_opts = CMD_OPTS_GEN_DATA(general);
struct general_settings_t g_general_settings = {
	.interval = 1,
	.color_bad = "#FF0000",
	.color_degraded = "#FFFF00",
	.color_good = "#00FF00"
};

struct runs_list ini_parse(FILE *ini) {
	struct run_instance *runs = NULL, *curr = NULL;
	unsigned res_size = 0;
	char buffer[1024], *ptr;
	buffer[sizeof(buffer) - 1] = '\0';

	while (fgets(buffer, sizeof(buffer) - 1, ini)) {
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

			++res_size;
			runs = realloc(runs, res_size * sizeof(struct run_instance));
			curr = runs + (res_size - 1);
			curr->vtable = cmd;
			curr->instance = (space == ender ? NULL : strdup(space));
			curr->data = calloc(cmd->data_size, 1);
		} else {
			void *data = curr ? (void *)curr->data : (void *)&g_general_settings;
			const struct cmd_opts *opts = curr ? &curr->vtable->opts : &general_opts;
			if (!parse_assignment(data, opts, ptr))
				goto _error;
		}
	}
	return (struct runs_list){runs, runs + res_size};

_error:
	free(runs);
	return (struct runs_list){NULL, NULL};
}

void free_all_run_instances(struct runs_list *runs) {
	for (struct run_instance *run = runs->runs_begin; run != runs->runs_end; run++) {
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
	const size_t sizeof_section = (size_t)(((const uint8_t *)&__stop_cmd_array) - ((const uint8_t *)&__start_cmd_array));
	if (sizeof_section % sizeof(struct cmd) != 0) {
		fprintf(stderr, "sizeof(section cmd_array) = %lu\n", sizeof_section);
		fprintf(stderr, "sizeof(struct cmd) = %lu\n", sizeof(struct cmd));
		return TEST_ERR(ERR_STR("somehow cmd_array section's pointers are incorrect"));
	}
	for (const struct cmd *iter = &__start_cmd_array; iter < &__stop_cmd_array; ++iter) {
		if (!iter->name)
			return TEST_ERR(ERR_STR("empty cmd name"));
		if (!iter->func_init || !iter->func_destroy || !iter->func_recache)
			return TEST_ERR(ERR_STR("cmd %s: must have function is empty"), iter->name);

		for (unsigned i = 1; i < iter->opts.size; ++i)
			if (0 <= strcmp(iter->opts.names[i - 1], iter->opts.names[i]))
				return TEST_ERR(ERR_STR("cmd %s: options not sorted"), iter->name);

		static const struct {
			const char *name;
			unsigned type;
			unsigned offset;
		} base_opts[] = {
			{"interval", OPT_TYPE_LONG, offsetof(struct cmd_data_base, interval)},
		};
		for (size_t i = 0; i < ARRAY_SIZE(base_opts); ++i) {
			const struct cmd_option *cmd_option = find_cmd_option(&iter->opts, base_opts[i].name);
			if (!cmd_option);
			else if (cmd_option->type != base_opts[i].type)
				return TEST_ERR(ERR_STR("cmd %s: incorrect type for %s"), iter->name, base_opts[i].name);
			else if (cmd_option->offset != base_opts[i].offset)
				return TEST_ERR(ERR_STR("cmd %s: incorrect offset for %s"), iter->name, base_opts[i].name);
		}
	}
	return true;
}

#endif
