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

#include <errno.h>
#include <signal.h>
#include <unistd.h>

struct cmd_run_watch_data {
	struct cmd_data_base base;
	char *path;
};

static bool cmd_run_watch_init(struct cmd_data_base *_data) {
	struct cmd_run_watch_data *data = (struct cmd_run_watch_data *)_data;
	return data->path;
}

static void cmd_run_watch_destroy(struct cmd_data_base *_data) {
	struct cmd_run_watch_data *data = (struct cmd_run_watch_data *)_data;
	free(data->path);
}

static bool cmd_run_watch_recache(struct cmd_data_base *_data) {
	struct cmd_run_watch_data *data = (struct cmd_run_watch_data *)_data;

	FILE *pid_file = fopen(data->path, "r");
	if (!pid_file)
		return false;
	char buffer[128];
	if (fgets(buffer, sizeof(buffer), pid_file)) {
		pid_t pid = (pid_t)strtol(buffer, NULL, 10);
		data->base.cached_fulltext = (kill(pid, 0) == 0 || errno == EPERM) ? "Running" : "Not Running";
	}
	fclose(pid_file);

	return true;
}

#define RUN_WATCH_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_run_watch_data, base.align)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_run_watch_data, base.interval)), \
	F("path", OPT_TYPE_STR, offsetof(struct cmd_run_watch_data, path))

CMD_OPTS_GEN_STRUCTS(cmd_run_watch, RUN_WATCH_OPTIONS)

DECLARE_CMD(cmd_run_watch) = {
	.name = "run_watch",
	.data_size = sizeof (struct cmd_run_watch_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_run_watch),

	.func_init = cmd_run_watch_init,
	.func_destroy = cmd_run_watch_destroy,
	.func_recache = cmd_run_watch_recache
};
