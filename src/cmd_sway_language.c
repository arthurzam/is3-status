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
#include "fdpoll.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

#include <yajl/yajl_parse.h>

struct cmd_sway_language_data {
	struct cmd_data_base base;

	char *keyboard_name;
	int socketfd;
	char cached_output[256];
};

struct msg_header_t {
	char magic[6];
	uint32_t size;
	uint32_t type;
} __attribute__((packed));
_Static_assert(sizeof(struct msg_header_t) == 14, "incorrect size for struct msg_header_t");

enum SWAY_IPC {
	IPC_SUBSCRIBE = 2,
	IPC_GET_INPUTS = 100,
};

enum CURRENT_KEY {
	CURRENT_KEY_UNSET = 0,
	CURRENT_KEY_IDENTIFIER = 1, // "identifier"
	CURRENT_KEY_XKB_LAYOUT = 2, // "xkb_active_layout_name"
};

struct cmd_sway_language_yajl_ctx {
	struct cmd_sway_language_data *data;
	const unsigned char *keyboard_layout;
	size_t keyboard_layout_len;
	uint8_t current_field;
	uint8_t depth;
	bool matching_identifier;
};

static int cmd_sway_language_yajl_string(void *_ctx, const unsigned char *str, size_t len) {
	struct cmd_sway_language_yajl_ctx *ctx = _ctx;
	if (len != 0) {
		if (ctx->current_field == CURRENT_KEY_IDENTIFIER) {
			if (0 == memcmp(str, ctx->data->keyboard_name, len))
				ctx->matching_identifier = true;
		} else if (ctx->current_field == CURRENT_KEY_XKB_LAYOUT) {
			ctx->keyboard_layout = str;
			ctx->keyboard_layout_len = len;
		}
	}
	return true;
}
static int cmd_sway_language_yajl_map_key(void *_ctx, const unsigned char *str, size_t len) {
	struct cmd_sway_language_yajl_ctx *ctx = _ctx;
	ctx->current_field = CURRENT_KEY_UNSET;
	switch (len) {
		case 10:
			if(likely(0 == memcmp(str, "identifier", 10)))
				ctx->current_field = CURRENT_KEY_IDENTIFIER;
			break;
		case 22:
			if(likely(0 == memcmp(str, "xkb_active_layout_name", 22)))
				ctx->current_field = CURRENT_KEY_XKB_LAYOUT;
			break;
	}
	return true;
}
static int cmd_sway_language_yajl_start_map(void *_ctx) {
	struct cmd_sway_language_yajl_ctx *ctx = _ctx;
	if ((++ctx->depth) == 1) {
		ctx->keyboard_layout = NULL;
		ctx->keyboard_layout_len = 0;
		ctx->current_field = CURRENT_KEY_UNSET;
		ctx->matching_identifier = false;
	}
	return true;
}
static int cmd_sway_language_yajl_end_map(void *_ctx) {
	struct cmd_sway_language_yajl_ctx *ctx = _ctx;

	if ((--ctx->depth) == 0 && ctx->keyboard_layout && ctx->matching_identifier) {
		if (ctx->keyboard_layout_len > sizeof(ctx->data->cached_output) - 1)
			ctx->keyboard_layout_len = sizeof(ctx->data->cached_output) - 1;
		memcpy(ctx->data->cached_output, ctx->keyboard_layout, ctx->keyboard_layout_len);
		ctx->data->cached_output[ctx->keyboard_layout_len] = '\0';
		return false;
	}
	return true;
}
static const yajl_callbacks cevent_callbacks = {
	.yajl_string = cmd_sway_language_yajl_string,
	.yajl_map_key = cmd_sway_language_yajl_map_key,
	.yajl_start_map = cmd_sway_language_yajl_start_map,
	.yajl_end_map = cmd_sway_language_yajl_end_map,
};

static bool handle_sway_language_events(void *arg) {
	struct cmd_sway_language_data *data = (struct cmd_sway_language_data *)arg;

	struct msg_header_t recv_buf;
	if (unlikely(sizeof(recv_buf) != read(data->socketfd, (&recv_buf), sizeof(recv_buf))))
		return false;
	ssize_t remaining = recv_buf.size;
	uint8_t buffer[2048];
	struct cmd_sway_language_yajl_ctx ctx = {
		.data = data
	};
	yajl_handle yajl_parse_handle = yajl_alloc(&cevent_callbacks, NULL, &ctx);
	do {
		ssize_t received = sizeof(buffer);
		if (received > remaining)
			received = remaining;
		received = read(data->socketfd, buffer, received);
		if (unlikely(received <= 0))
			goto _free_yajl;
		remaining -= received;
		yajl_parse(yajl_parse_handle, buffer, (size_t)received);
	} while (remaining > 0);
_free_yajl:
	yajl_free(yajl_parse_handle);
	return false;
}

static bool cmd_sway_language_init(struct cmd_data_base *_data) {
	struct cmd_sway_language_data *data = (struct cmd_sway_language_data *)_data;

	const char *sock = getenv("SWAYSOCK");
	if (!sock) {
		fputs("sway-language: empty SWAYSOCK\n", stderr);
		return false;
	}
	if ((data->socketfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		fputs("socket(sway-language) failed\n", stderr);
		return false;
	}
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sock, sizeof(addr.sun_path) - 1);
	addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
	if (connect(data->socketfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
		fprintf(stderr, "connect(sway-language) failed %s\n", addr.sun_path);
		return false;
	}

	static const struct {
		struct msg_header_t header;
		char payload[9];
	} sway_ipc_subscribe = {
		{
			.magic = "i3-ipc",
			.size = sizeof(sway_ipc_subscribe.payload),
			.type = IPC_SUBSCRIBE
		}, "[\"input\"]"
	};

	if (unlikely(0 > write(data->socketfd, &sway_ipc_subscribe, sizeof(sway_ipc_subscribe)))) {
		return false;
	}

	fdpoll_add(data->socketfd, handle_sway_language_events, data);

	data->base.cached_fulltext = data->cached_output;
	data->base.interval = -1;
	return true;
}

static void cmd_sway_language_destroy(struct cmd_data_base *_data) {
	struct cmd_sway_language_data *data = (struct cmd_sway_language_data *)_data;
	close(data->socketfd);
}

static void cmd_sway_language_recache(struct cmd_data_base *_data) {
	struct cmd_sway_language_data *data = (struct cmd_sway_language_data *)_data;

	static const struct msg_header_t sway_ipc_get_inputs = {
		.magic = "i3-ipc",
		.size = 0,
		.type = IPC_GET_INPUTS
	};
	if (unlikely(0 > write(data->socketfd, &sway_ipc_get_inputs, sizeof(sway_ipc_get_inputs)))) {
		return;
	}
}

#define SWAY_LANG_OPTIONS(F) \
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
