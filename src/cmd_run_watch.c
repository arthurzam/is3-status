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

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

struct cmd_run_watch_data {
	struct cmd_data_base base;
	char *path;
	char *text_down;
	char *text_up;
};

static bool cmd_run_watch_init(struct cmd_data_base *_data) {
	struct cmd_run_watch_data *data = (struct cmd_run_watch_data *)_data;
	if (!data->path)
		return false;
	if (!data->text_down)
		data->text_down = strdup("Not Running");
	if (!data->text_up)
		data->text_up = strdup("Running");
	return true;
}

static void cmd_run_watch_destroy(struct cmd_data_base *_data) {
	struct cmd_run_watch_data *data = (struct cmd_run_watch_data *)_data;
	free(data->path);
	free(data->text_down);
	free(data->text_up);
}

static void cmd_run_watch_recache(struct cmd_data_base *_data) {
	struct cmd_run_watch_data *data = (struct cmd_run_watch_data *)_data;

	data->base.cached_fulltext = data->text_down;
	int fd = open(data->path, O_RDONLY);
	if (likely(fd >= 0)) {
		char buf[64];
		ssize_t len = read(fd, buf, sizeof(buf) - 1);
		if (likely(len > 0)) {
			buf[len] = '\0';
			pid_t pid = (pid_t)atol(buf);
			if (kill(pid, 0) == 0 || errno == EPERM)
				data->base.cached_fulltext = data->text_up;
		}
		close(fd);
	}
}

#define RUN_WATCH_OPTIONS(F) \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_run_watch_data, base.interval)), \
	F("path", OPT_TYPE_STR, offsetof(struct cmd_run_watch_data, path)), \
	F("text_down", OPT_TYPE_STR, offsetof(struct cmd_run_watch_data, text_down)), \
	F("text_up", OPT_TYPE_STR, offsetof(struct cmd_run_watch_data, text_up)), \

CMD_OPTS_GEN_STRUCTS(cmd_run_watch, RUN_WATCH_OPTIONS)

DECLARE_CMD(cmd_run_watch) = {
	.name = "run_watch",
	.data_size = sizeof (struct cmd_run_watch_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_run_watch),

	.func_init = cmd_run_watch_init,
	.func_destroy = cmd_run_watch_destroy,
	.func_recache = cmd_run_watch_recache
};
