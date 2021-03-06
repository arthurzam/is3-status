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
#include <stdio.h>

#include <unistd.h>
#include <errno.h>

#include "main.h"
#include "ini_parser.h"
#include "fdpoll.h"

void init_cevent_handle(struct runs_list *runs);

int main(int argc, char *argv[]) {
#ifdef TESTS
	if (!test_cmd_array_correct())
		return 1;
#endif
	struct runs_list runs = ini_parse(argc > 1 ? argv[1] : NULL);
	if (runs.runs_begin == NULL) {
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
		if (run->data->interval >= 0 && run->data->interval < g_general_settings.interval)
			run->data->interval = g_general_settings.interval;
	}

#define WRITE_LEN(str) write(STDOUT_FILENO, str, strlen(str))
	if (unlikely(0 > WRITE_LEN("{\"version\":1, \"click_events\": true}\n[\n[]\n"))) {
		fprintf(stderr, "unable to send start status bar\n");
		return 1;
	}
#undef WRITE_LEN

	init_cevent_handle(&runs);

	char output_buffer[4096] = ",[";
	int fdpoll_res;
	for (unsigned eventNum = 0; (fdpoll_res = fdpoll_run()) >= 0; ++eventNum) {
		char *ptr = output_buffer + 2;
		FOREACH_RUN(run, &runs) {
			if ((fdpoll_res > 0) || (run->data->interval > 0 && eventNum % run->data->interval == 0))
				run->vtable->func_recache(run->data);

			if (run != runs.runs_begin) // separator before all except first
				*(ptr++) = ',';
			size_t len;
#define OUTPUT_CONST_STR(str) memcpy(ptr, str, strlen(str)); ptr += strlen(str)
			OUTPUT_CONST_STR("{\"name\":\"");
			len = strlen(run->vtable->name);
			memcpy(ptr, run->vtable->name, len);
			ptr += len;

			OUTPUT_CONST_STR("\",\"markup\":\"none");

			if (run->instance) {
				OUTPUT_CONST_STR("\",\"instance\":\"");
				len = strlen(run->instance);
				memcpy(ptr, run->instance, len);
				ptr += len;
			}

			if (likely(run->data->cached_fulltext)) {
				len = strlen(run->data->cached_fulltext);
				if (likely(output_buffer + (sizeof(output_buffer) - 256) >  ptr + len)) {
					OUTPUT_CONST_STR("\",\"full_text\":\"");
					memcpy(ptr, run->data->cached_fulltext, len);
					ptr += len;
				} else {
					*(ptr++) = '}';
					break;
				}
			}

			if (run->data->cached_color[0]) {
				OUTPUT_CONST_STR("\",\"color\":\"");
				memcpy(ptr, run->data->cached_color, 8);
				ptr += 7;
			}
			*(ptr++) = '\"';
			*(ptr++) = '}';
#undef OUTPUT_CONST_STR
		}
		*(ptr++) = ']';
		*(ptr++) = '\n';

		if (unlikely(0 > write(STDOUT_FILENO, output_buffer, (size_t)(ptr - output_buffer)))) {
			fprintf(stderr, "main: unable to send output: %s\n", strerror(errno));
		}
	}

	free_all_run_instances(&runs);
	return 0;
}
