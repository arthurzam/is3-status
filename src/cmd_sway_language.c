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
	unsigned buffer_size;
	int socketfd;
};

struct msg_header_t {
	char magic[6];
	uint32_t size;
	uint32_t type;
} __attribute__((packed));
_Static_assert(sizeof(struct msg_header_t) == 14, "incorrect size for struct msg_header_t");

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

	if ((socket_path = cmd_sway_language_get_socketpath()) == NULL) {
		fputs("sway-language: Unable to find Sway socket\n", stderr);
		return -1;
	}
	if ((socketfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		fputs("sway-language: Unable to open Unix socket\n", stderr);
		return -1;
	}
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
	addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
	if (connect(socketfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
		fprintf(stderr, "sway-language: Unable to connect to %s\n", socket_path);
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
		static const struct msg_header_t msg = {
			.magic = {'i', '3', '-', 'i', 'p', 'c'},
			.size = 0,
			.type = 100
		};
		if (0 > write(data->socketfd, &msg, sizeof(msg))) {
			fprintf(stderr, "sway-language: unable to send request, error %s\n", strerror(errno));
			return false;
		}
	}

	/* recv msg on IPC */
	{
		struct msg_header_t recv_buf;
		size_t total = 0;
		do {
			ssize_t received = recv(data->socketfd, (uint8_t*)(&recv_buf) + total, sizeof(recv_buf) - total, 0);
			if (received <= 0) {
				fprintf(stderr, "sway-language: Unable to receive IPC response, error %s\n", strerror(errno));
				return false;
			}
			total += (size_t)received;
		} while (total < sizeof(recv_buf));

		if (data->buffer_size < recv_buf.size + 1) {
			data->buffer_size = recv_buf.size + 1;
			data->buffer = realloc(data->buffer, data->buffer_size);
		}
		total = 0;
		do {
			ssize_t received = recv(data->socketfd, data->buffer + total, recv_buf.size - total, 0);
			if (received <= 0) {
				fprintf(stderr, "sway-language: Unable to receive IPC response, error %s\n", strerror(errno));
				return false;
			}
			total += (size_t)received;
		} while (total < recv_buf.size);
		data->buffer[recv_buf.size] = '\0';
	}

	/* parse msg */
	{
		yajl_val node = yajl_tree_parse(data->buffer, NULL, 0);

		if (likely(YAJL_IS_ARRAY(node))) {
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
						if (!data->base.cached_fulltext || 0 != strcmp(xkb_active_layout_name, data->base.cached_fulltext)) {
							free(data->base.cached_fulltext);
							data->base.cached_fulltext = strdup(xkb_active_layout_name);
						}
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

	if (!data->keyboard_name)
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
	free(data->base.cached_fulltext);
}

static bool cmd_sway_language_recache(struct cmd_data_base *_data) {
	struct cmd_sway_language_data *data = (struct cmd_sway_language_data *)_data;

	return cmd_sway_language_query(data);
}

#define SWAY_LANG_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_sway_language_data, base.align)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_sway_language_data, base.interval)), \
	F("keyboard-name", OPT_TYPE_STR, offsetof(struct cmd_sway_language_data, keyboard_name))

CMD_OPTS_GEN_STRUCTS(cmd_sway_language, SWAY_LANG_OPTIONS)

DECLARE_CMD(cmd_sway_language) = {
	.name = "sway_language",
	.data_size = sizeof (struct cmd_sway_language_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_sway_language),

	.func_init = cmd_sway_language_init,
	.func_destroy = cmd_sway_language_destroy,
	.func_recache = cmd_sway_language_recache
};
