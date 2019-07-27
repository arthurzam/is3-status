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

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>

#include <yajl/yajl_tree.h>

struct cmd_sway_language_data {
	struct cmd_data_base base;
	char *keyboard_name;
	char *buffer;
	char *cached_layout;
	unsigned buffer_size;
	int socketfd;
};

static char ipc_magic[] = {'i', '3', '-', 'i', 'p', 'c'};

static char *cmd_sway_language_get_socketpath(void) {
	const char *sock = getenv("SWAYSOCK");
	if (sock)
		return strdup(sock);

	FILE *fp = popen("sway --get-socketpath 2>/dev/null", "r");
	if (fp) {
		char *line = NULL;
		size_t line_size = 0;
		ssize_t nret = getline(&line, &line_size, fp);
		pclose(fp);
		if (nret > 0) {
			if (line[nret - 1] == '\n')
				line[nret - 1] = '\0';
			return line;
		}
		free(line);
	}
	return NULL;
}

static int cmd_sway_language_open_socket(void) {
	int socketfd;
	char *socket_path;

	if ((socketfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		fputs("is3-status: sway-language: Unable to open Unix socket\n", stderr);
		return -1;
	}
	if ((socket_path = cmd_sway_language_get_socketpath()) == NULL) {
		fputs("is3-status: sway-language: Unable to find Sway socket\n", stderr);
		return -1;
	}
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
	addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
	if (connect(socketfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
		fprintf(stderr, "is3-status: sway-language: Unable to connect to %s\n", socket_path);
		close(socketfd);
		free(socket_path);
		return -1;
	}
	free(socket_path);
	return socketfd;
}

static bool cmd_sway_language_query(struct cmd_sway_language_data *data) {
	/* send msg on IPC */
	{
		struct {
			uint32_t len;
			uint32_t type;
		} send_buf = {0, 100};
		struct iovec iov[2] = { {ipc_magic, sizeof(ipc_magic)}, {&send_buf, sizeof(send_buf)} };
		if (0 > writev(data->socketfd, iov, 2)) {
			fprintf(stderr, "is3-status: sway-language: unable to send request, error %s\n", strerror(errno));
			return false;
		}
	}

	/* recv msg on IPC */
	{
		struct __attribute__((packed)) {
			char magic[sizeof(ipc_magic)];
			uint32_t size;
			uint32_t type;
		} recv_buf;
		size_t total = 0;
		while (total < sizeof(recv_buf)) {
			ssize_t received = recv(data->socketfd, (char*)(&recv_buf) + total, sizeof(recv_buf) - total, 0);
			if (received <= 0) {
				fprintf(stderr, "is3-status: sway-language: Unable to receive IPC response, error %s\n", strerror(errno));
				return false;
			}
			total += (size_t)received;
		}

		if (data->buffer_size < recv_buf.size + 1) {
			data->buffer_size = recv_buf.size + 1;
			data->buffer = realloc(data->buffer, data->buffer_size);
		}
		total = 0;
		while (total < recv_buf.size) {
			ssize_t received = recv(data->socketfd, data->buffer + total, recv_buf.size - total, 0);
			if (received <= 0) {
				fprintf(stderr, "is3-status: sway-language: Unable to receive IPC response, error %s\n", strerror(errno));
				return false;
			}
			total += (size_t)received;
		}
		data->buffer[recv_buf.size] = '\0';
	}

	/* parse msg */
	{
		char errbuf[1024];
		yajl_val node = yajl_tree_parse(data->buffer, errbuf, sizeof(errbuf));

		if (YAJL_IS_ARRAY(node)) {
			for (size_t i = 0; i < node->u.array.len; ++i ) {
				char *xkb_active_layout_name = NULL;
				int found = 0;
				yajl_val obj = node->u.array.values[i];

				for (size_t ii = 0; ii < obj->u.object.len; ++ii ) {
					const char *key = obj->u.object.keys[ ii ];
					yajl_val value = obj->u.object.values[ ii ];
					if (!YAJL_IS_STRING(value))
						continue;
					if (0 == strcmp(key, "identifier")) {
						if (0 != strcmp(value->u.string, data->keyboard_name))
							continue;
						found = 1;
					} else if (0 == strcmp(key, "xkb_active_layout_name")) {
						xkb_active_layout_name = YAJL_GET_STRING(value);
					}
					if (found && xkb_active_layout_name) {
						if (!data->cached_layout || 0 != strcmp(xkb_active_layout_name, data->cached_layout))
							data->cached_layout = strdup(xkb_active_layout_name);
						yajl_tree_free(node);
						return true;
					}
				}
			}
		}
		yajl_tree_free(node);

		return false;
	}
}

static bool cmd_sway_language_init(struct cmd_data_base *_data) {
	struct cmd_sway_language_data *data = (struct cmd_sway_language_data *)_data;

	if (data->keyboard_name == NULL)
		return false;
	if (-1 == (data->socketfd = cmd_sway_language_open_socket()))
		return false;

	return true;
}

static void cmd_sway_language_destroy(struct cmd_data_base *_data) {
	struct cmd_sway_language_data *data = (struct cmd_sway_language_data *)_data;
	close(data->socketfd);
	free(data->buffer);
	free(data->keyboard_name);
	free(data->cached_layout);
}

static bool cmd_sway_language_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_sway_language_data *data = (struct cmd_sway_language_data *)_data;

	if (update && !cmd_sway_language_query(data))
		return false;
	JSON_OUTPUT_KV(json_gen, "full_text", data->cached_layout);
	return true;
}

#define SWAY_LANG_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_sway_language_data, base.align)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_sway_language_data, base.interval)), \
	F("keyboard-name", OPT_TYPE_STR, offsetof(struct cmd_sway_language_data, keyboard_name))

static const char *const cmd_sway_language_options_names[] = {
	SWAY_LANG_OPTIONS(CMD_OPTS_GEN_NAME)
};

static const struct cmd_option cmd_sway_language_options[] = {
	SWAY_LANG_OPTIONS(CMD_OPTS_GEN_DATA)
};

DECLARE_CMD(cmd_sway_language) = {
	.name = "sway_language",
	.data_size = sizeof (struct cmd_sway_language_data),

	.opts = {
		.names = cmd_sway_language_options_names,
		.opts = cmd_sway_language_options,
		.size = sizeof(cmd_sway_language_options) / sizeof(cmd_sway_language_options[0])
	},

	.func_init = cmd_sway_language_init,
	.func_destroy = cmd_sway_language_destroy,
	.func_output = cmd_sway_language_output
};
