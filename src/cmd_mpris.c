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
#include "vprint.h"
#include "dbus_monitor.h"

#include <stdio.h>

struct cmd_mpris_data {
	struct cmd_data_base base;
	char *mpris_service;
	char *format_playing;
	char *format_paused;
	char *format_stopped;

	sd_bus *bus;

	struct dbus_mpris_data {
		const struct dbus_fields_t *fields;
		char *title;
		char *artist;
		char *album;
		char *playback_status;
		long length;
		long position;
	} data;
};

#define DBUS_MPRIS_FIELDS(F) \
	F("Metadata", FIELD_ARR_DICT_EXPAND, 0), \
	F("PlaybackStatus", FIELD_STR, offsetof(struct dbus_mpris_data, playback_status)), \
	F("Position", FIELD_LONG, offsetof(struct dbus_mpris_data, position)), \
	F("mpris:length", FIELD_LONG, offsetof(struct dbus_mpris_data, length)), \
	F("xesam:album", FIELD_STR, offsetof(struct dbus_mpris_data, album)), \
	F("xesam:artist", FIELD_ARR_STR_FIRST, offsetof(struct dbus_mpris_data, artist)), \
	F("xesam:title", FIELD_STR, offsetof(struct dbus_mpris_data, title)), \

static const char *const cmd_mpris_dbus_fields_names[] = {
	DBUS_MPRIS_FIELDS(CMD_OPTS_GEN_NAME)
};

static const struct dbus_field cmd_mpris_dbus_fields[] = {
	DBUS_MPRIS_FIELDS(CMD_OPTS_GEN_DATA)
};
static const struct dbus_fields_t cmd_mpris_dbus = {
	.names = cmd_mpris_dbus_fields_names,
	.opts = cmd_mpris_dbus_fields,
	.size = sizeof(cmd_mpris_dbus_fields) / sizeof(cmd_mpris_dbus_fields[0])
};

static bool cmd_mpris_init(struct cmd_data_base *_data) {
	struct cmd_mpris_data *data = (struct cmd_mpris_data *)_data;

	if (!data->mpris_service)
		return false;
	if (!data->format_stopped)
		data->format_stopped = strdup("Stopped");
	if (!data->format_paused)
		data->format_paused = strdup(data->format_stopped);
	if (!data->format_playing)
		data->format_playing = strdup("%T");

	sd_bus_error error = SD_BUS_ERROR_NULL;
	int r = sd_bus_open_user(&data->bus);
	if (r < 0) {
		fprintf(stderr, "is3-status: mpris: Failed to connect to user bus: %s\n", strerror(-r));
		return false;
	}

	data->data.fields = &cmd_mpris_dbus;
	dbus_add_watcher(data->mpris_service, "/org/mpris/MediaPlayer2", &data->data);

	sd_bus_get_property_string(data->bus, data->mpris_service, "/org/mpris/MediaPlayer2",
							   "org.mpris.MediaPlayer2.Player", "PlaybackStatus", &error, &data->data.playback_status);
	sd_bus_error_free(&error);

	sd_bus_get_property_trivial(data->bus, data->mpris_service, "/org/mpris/MediaPlayer2",
								"org.mpris.MediaPlayer2.Player", "Position",
								&error, SD_BUS_TYPE_INT64, &data->data.position);
	sd_bus_error_free(&error);

	sd_bus_message *reply = NULL;
	if (0 <= sd_bus_get_property(data->bus, data->mpris_service, "/org/mpris/MediaPlayer2",
								 "org.mpris.MediaPlayer2.Player", "Metadata", &error, &reply, "a{sv}"))
		dbus_parse_arr_fields(reply, &data->data);
	sd_bus_error_free(&error);
	sd_bus_message_unref(reply);

	return true;
}

static void cmd_mpris_destroy(struct cmd_data_base *_data) {
	struct cmd_mpris_data *data = (struct cmd_mpris_data *)_data;
	free(data->mpris_service);
	free(data->format_paused);
	free(data->format_playing);
	free(data->format_stopped);

	free(data->data.album);
	free(data->data.artist);
	free(data->data.title);
	free(data->data.playback_status);

	sd_bus_unref(data->bus);
}

// generaterd using command ./gen-format.py AalpTt
VPRINT_OPTS(cmd_mpris_var_options, {0x00000000, 0x00000000, 0x00100002, 0x00111002});

static bool cmd_mpris_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_mpris_data *data = (struct cmd_mpris_data *)_data;
	(void)update;

	const char *output_format = data->format_stopped, *color = NULL;
	if (data->data.playback_status == NULL);
	else if (0 == strcmp(data->data.playback_status, "Playing")) {
		output_format = data->format_playing;
		color = g_general_settings.color_good;
	} else if (0 == strcmp(data->data.playback_status, "Paused")) {
		output_format = data->format_paused;
		color = g_general_settings.color_degraded;
	}

	int res;
	char buffer[256];
	struct vprint ctx = {cmd_mpris_var_options, output_format, buffer, sizeof(buffer)};
	while ((res = vprint_walk(&ctx)) >= 0) {
		switch (res) {
			case 'A':
				if (data->data.artist)
					vprint_strcat(&ctx, data->data.album);
				break;
			case 'a':
				if (data->data.artist)
					vprint_strcat(&ctx, data->data.artist);
				break;
			case 't':
				if (data->data.title)
					vprint_strcat(&ctx, data->data.title);
				break;
			case 'p':
				vprint_time(&ctx, (int)data->data.position / 1000000);
				break;
			case 'l':
				vprint_time(&ctx, (int)data->data.length / 1000000);
				break;
			case 'T':
				if (data->data.artist) {
					vprint_strcat(&ctx, data->data.artist);
					if (data->data.title)
						vprint_strcat(&ctx, " - ");
				}
				if (data->data.title)
					vprint_strcat(&ctx, data->data.title);
				break;
		}
	}
	if (color)
		JSON_OUTPUT_COLOR(json_gen, color);
	JSON_OUTPUT_K(json_gen, "full_text", buffer, sizeof(buffer) - ctx.remainingSize);
	return true;
}

static bool cmd_mpris_cevent(struct cmd_data_base *_data, int event) {
	struct cmd_mpris_data *data = (struct cmd_mpris_data *)_data;
	const char *op = NULL;
	switch(event) {
		case CEVENT_MOUSE_MIDDLE:
			op = "PlayPause";
			break;
		case CEVENT_MOUSE_LEFT:
			op = "Previous";
			break;
		case CEVENT_MOUSE_RIGHT:
			op = "Next";
			break;
		default:
			return true;
	}
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int r = sd_bus_call_method(data->bus, data->mpris_service, "/org/mpris/MediaPlayer2",
							   "org.mpris.MediaPlayer2.Player", op, &error, NULL, NULL);
	if (r < 0)
		fprintf(stderr, "is3-status: mpris: failed click event %s with: %s\n", op, error.message);
	sd_bus_error_free(&error);
	return r >= 0;
}

#define MPRIS_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_mpris_data, base.align)), \
	F("format_paused", OPT_TYPE_STR, offsetof(struct cmd_mpris_data, format_paused)), \
	F("format_playing", OPT_TYPE_STR, offsetof(struct cmd_mpris_data, format_playing)), \
	F("format_stopped", OPT_TYPE_STR, offsetof(struct cmd_mpris_data, format_stopped)), \
	F("mpris_service", OPT_TYPE_STR, offsetof(struct cmd_mpris_data, mpris_service))

static const char *const cmd_mpris_options_names[] = {
	MPRIS_OPTIONS(CMD_OPTS_GEN_NAME)
};

static const struct cmd_option cmd_mpris_options[] = {
	MPRIS_OPTIONS(CMD_OPTS_GEN_DATA)
};

DECLARE_CMD(cmd_mpris) = {
	.name = "mpris",
	.data_size = sizeof (struct cmd_mpris_data),

	.opts = {
		.names = cmd_mpris_options_names,
		.opts = cmd_mpris_options,
		.size = sizeof(cmd_mpris_options) / sizeof(cmd_mpris_options[0])
	},

	.func_init = cmd_mpris_init,
	.func_destroy = cmd_mpris_destroy,
	.func_output = cmd_mpris_output,
	.func_cevent = cmd_mpris_cevent
};
