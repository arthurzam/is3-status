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

#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>

#include <yajl_version.h>
#include <yajl_tree.h>
#include <yajl_gen.h>

#include "main.h"
#include "ini_parser.h"
#include "fdpoll.h"

__attribute__((always_inline)) inline void json_output(yajl_gen json_gen, const char *key, size_t key_size, const char *value, size_t value_size) {
	yajl_gen_string(json_gen, (const unsigned char *)key, key_size);
	yajl_gen_string(json_gen, (const unsigned char *)value, value_size);
}

static FILE *open_config(const char *defPath) {
	if (defPath && access(defPath, R_OK) == 0)
		return fopen(defPath, "r");
	const char *path = getenv("IS3_STATUS_CONFIG");
	if (path && access(path, R_OK) == 0)
		return fopen(path, "r");

	char buf[FILENAME_MAX + 1];
	buf[0] = buf[FILENAME_MAX] = '\0';

	if ((path = getenv("XDG_CONFIG_HOME")) && path[0] != '\0') {
		strncpy(buf, path, FILENAME_MAX);
		strncat(buf, "/is3-status.conf", FILENAME_MAX);
		if (access(buf, R_OK) == 0)
			return fopen(buf, "r");
	}
	if ((path = getenv("HOME"))) {
		strncpy(buf, path, FILENAME_MAX);
		const size_t len = strlen(buf);

		strncpy(buf + len, "/.config/is3-status.conf", FILENAME_MAX - len);
		if (access(buf, R_OK) == 0)
			return fopen(buf, "r");

		strncpy(buf + len, "/.is3-status.conf", FILENAME_MAX - len);
		if (access(buf, R_OK) == 0)
			return fopen(buf, "r");
	}
	if (access("/etc/is3-status.conf", R_OK) == 0)
		return fopen("/etc/is3-status.conf", "r");

	return NULL;
}

static bool handle_click_event(void *arg) {
	struct runs_list *runs = arg;
	char input[1024];
	bool res = false;

	while (fgets(input, sizeof(input), stdin)) {
		char* walker = input;
		if (*walker == '[')
			walker++;
		if (*walker == '\0' || *walker == '\n')
			continue;
		if (*walker == ',')
			walker++;

		yajl_val node = yajl_tree_parse(walker, NULL, 0);
		if (YAJL_IS_OBJECT(node)) {
			const char *name = NULL, *instance = NULL;
			int button = -1;

			for (size_t i = 0; i < node->u.object.len; ++i) {
				const char *key = node->u.object.keys[i];
				if (0 == memcmp(key, "name", 5))
					name = YAJL_GET_STRING(node->u.object.values[i]);
				else if (0 == memcmp(key, "instance", 9))
					instance = YAJL_GET_STRING(node->u.object.values[i]);
				else if (0 == memcmp(key, "button", 7))
					button = (int)YAJL_GET_INTEGER(node->u.object.values[i]);
			}

			if (name == NULL || button == -1) {
				fprintf(stderr, "handle_cevent: bad click event object: %s\n", input);
				continue;
			}
			FOREACH_RUN(run, runs) {
				if ((0 == strcmp(run->vtable->name, name)) &&
						(instance == run->instance/* == NULL*/ || 0 == strcmp(run->instance, instance))) {
					if (run->vtable->func_cevent && run->vtable->func_cevent(run->data, button))
						res = true;
					break;
				}
			}
		}
		yajl_tree_free(node);
	}
	return res;
}

int main(int argc, char *argv[])
{
#ifdef TESTS
	if (!test_cmd_array_correct())
		return 1;
#endif
	FILE *ini = open_config(argc > 1 ? argv[1] : NULL);
	if (!ini) {
		fprintf(stderr, "Couldn't find config file\n");
		return 1;
	}
	struct runs_list runs = ini_parse(ini);
	fclose(ini);
	if (runs.runs_begin == NULL || runs.runs_end == runs.runs_begin) {
		fprintf(stderr, "Couldn't load config file\n");
		return 1;
	}

	if (g_general_settings.interval <= 0)
		g_general_settings.interval = 1;
	FOREACH_RUN(run, &runs) {
		if (!run->vtable->func_init(run->data)) {
			fprintf(stderr, "init for %s:%s failed\n", run->vtable->name, run->instance);
			return 1;
		}
		run->vtable->func_recache(run->data);
		if (!run->data->align)
			run->data->align = g_general_settings.align;
		if (run->data->interval >= 0 && run->data->interval < g_general_settings.interval)
			run->data->interval = g_general_settings.interval;
	}

#define WRITE_LEN(str) write(STDOUT_FILENO, str, strlen(str))
	if (unlikely(0 > WRITE_LEN("{\"version\":1, \"click_events\": true}\n[\n"))) {
		fprintf(stderr, "unable to send start status bar\n");
		return 1;
	}
#undef WRITE_LEN

	yajl_gen json_gen = yajl_gen_alloc(NULL);
	yajl_gen_array_open(json_gen);
	yajl_gen_clear(json_gen);

	fdpoll_add(STDIN_FILENO, handle_click_event, &runs);
	struct iovec iov[2] = {	{NULL, 0}, {"\n", 1} };

	int fdpoll_res;
	for (unsigned eventNum = 0; (fdpoll_res = fdpoll_run()) >= 0; ++eventNum) {
		yajl_gen_array_open(json_gen);
		FOREACH_RUN(run, &runs) {
			if ((fdpoll_res > 0) || (run->data->interval > 0 && eventNum % run->data->interval == 0))
				run->vtable->func_recache(run->data);

#define JSON_OUTPUT(key,value) json_output(json_gen, (key), strlen(key), value, strlen(value))
			yajl_gen_map_open(json_gen);
			JSON_OUTPUT("name", run->vtable->name);
			JSON_OUTPUT("markup", "none");
			if (run->data->align)
				JSON_OUTPUT("align", run->data->align);
			if (run->instance)
				JSON_OUTPUT("instance", run->instance);
			if (run->data->cached_fulltext)
				JSON_OUTPUT("full_text", run->data->cached_fulltext);
			if (run->data->cached_color[0])
				json_output(json_gen, "color", 5, run->data->cached_color, 7);
			yajl_gen_map_close(json_gen);
#undef JSON_OUTPUT
		}
		yajl_gen_array_close(json_gen);

		yajl_gen_get_buf(json_gen, (const unsigned char **)(void *)&iov[0].iov_base, &iov[0].iov_len);
		if (unlikely(0 > writev(STDOUT_FILENO, iov, 2))) {
			fprintf(stderr, "main: unable to send output, error %s\n", strerror(errno));
		}
		yajl_gen_clear(json_gen);
	}

	yajl_gen_free(json_gen);
	free_all_run_instances(&runs);
	return 0;
}
