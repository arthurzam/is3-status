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

	char cached_output[256];
};

static void cmd_mpris_recache(struct cmd_data_base *_data);

#define DBUS_MPRIS_FIELDS(F) \
	F("Metadata", FIELD_ARR_DICT_EXPAND, 0), \
	F("PlaybackStatus", FIELD_STR, offsetof(struct dbus_mpris_data, playback_status)), \
	F("Position", FIELD_LONG, offsetof(struct dbus_mpris_data, position)), \
	F("mpris:length", FIELD_LONG, offsetof(struct dbus_mpris_data, length)), \
	F("xesam:album", FIELD_STR, offsetof(struct dbus_mpris_data, album)), \
	F("xesam:artist", FIELD_ARR_STR_FIRST, offsetof(struct dbus_mpris_data, artist)), \
	F("xesam:title", FIELD_STR, offsetof(struct dbus_mpris_data, title)), \

DBUS_MONITOR_GEN_FIELDS(cmd_mpris_dbus, DBUS_MPRIS_FIELDS, cmd_mpris_recache, struct cmd_mpris_data, data)

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

	int r = sd_bus_open_user(&data->bus);
	if (r < 0) {
		fprintf(stderr, "mpris: Failed to connect to user bus: %s\n", strerror(-r));
		return false;
	}

	data->data.fields = &cmd_mpris_dbus;
	dbus_add_watcher(data->mpris_service, "/org/mpris/MediaPlayer2", &data->data);

	sd_bus_get_property_string(data->bus, data->mpris_service, "/org/mpris/MediaPlayer2",
							   "org.mpris.MediaPlayer2.Player", "PlaybackStatus", NULL, &data->data.playback_status);
	sd_bus_get_property_trivial(data->bus, data->mpris_service, "/org/mpris/MediaPlayer2",
								"org.mpris.MediaPlayer2.Player", "Position",
								NULL, SD_BUS_TYPE_INT64, &data->data.position);
	sd_bus_message *reply = NULL;
	if (0 <= sd_bus_get_property(data->bus, data->mpris_service, "/org/mpris/MediaPlayer2",
								 "org.mpris.MediaPlayer2.Player", "Metadata", NULL, &reply, "a{sv}"))
		dbus_parse_arr_fields(reply, &data->data);
	sd_bus_message_unref(reply);

	data->base.cached_fulltext = data->cached_output;
	data->base.interval = -1;
	return true;
}

static void cmd_mpris_destroy(struct cmd_data_base *_data) {
	struct cmd_mpris_data *data = (struct cmd_mpris_data *)_data;
	free(data->mpris_service);
	free(data->format_playing);
	free(data->format_paused);
	free(data->format_stopped);

	free(data->data.title);
	free(data->data.artist);
	free(data->data.album);
	free(data->data.playback_status);

	sd_bus_unref(data->bus);
}

// generaterd using command ./gen-format.py AalpTt
VPRINT_OPTS(cmd_mpris_var_options, {0x00000000, 0x00000000, 0x00100002, 0x00111002});

static void cmd_mpris_recache(struct cmd_data_base *_data) {
	struct cmd_mpris_data *data = (struct cmd_mpris_data *)_data;

	const char *output_format = data->format_stopped;
	if (!data->data.playback_status);
	else if (0 == memcmp(data->data.playback_status, "Playing", 8)) {
		output_format = data->format_playing;
		CMD_COLOR_SET(data, g_general_settings.color_good);
	} else if (0 == memcmp(data->data.playback_status, "Paused", 7)) {
		output_format = data->format_paused;
		CMD_COLOR_SET(data, g_general_settings.color_degraded);
	} else
		CMD_COLOR_CLEAN(data);

	unsigned res;
	struct vprint ctx = {cmd_mpris_var_options, output_format, data->cached_output, data->cached_output + sizeof(data->cached_output)};
	while ((res = vprint_walk(&ctx)) != 0) {
		switch (res) {
			case 'A':
				if (data->data.album)
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
}

static void cmd_mpris_cevent(struct cmd_data_base *_data, unsigned event, unsigned modifiers) {
	struct cmd_mpris_data *data = (struct cmd_mpris_data *)_data;
	const char *op = NULL;
	switch (event) {
		case CEVENT_MOUSE_MIDDLE:
			op = (modifiers & (CEVENT_MOD_SHIFT | CEVENT_MOD_CONTROL)) ? "Stop" : "PlayPause";
			break;
		case CEVENT_MOUSE_LEFT:
			if (modifiers & (CEVENT_MOD_SHIFT | CEVENT_MOD_CONTROL)) {
				sd_bus_call_method(data->bus, data->mpris_service, "/org/mpris/MediaPlayer2",
								   "org.mpris.MediaPlayer2.Player", "Seek", NULL, NULL,
								   "x", -(int64_t)data->data.position * 1000000);
				return;
			}
			op = "Previous";
			break;
		case CEVENT_MOUSE_RIGHT:
			op = "Next";
			break;
		default: return;
	}
	sd_bus_call_method(data->bus, data->mpris_service, "/org/mpris/MediaPlayer2",
					   "org.mpris.MediaPlayer2.Player", op, NULL, NULL, NULL);
}

#define MPRIS_OPTIONS(F) \
	F("format_paused", OPT_TYPE_STR, offsetof(struct cmd_mpris_data, format_paused)), \
	F("format_playing", OPT_TYPE_STR, offsetof(struct cmd_mpris_data, format_playing)), \
	F("format_stopped", OPT_TYPE_STR, offsetof(struct cmd_mpris_data, format_stopped)), \
	F("mpris_service", OPT_TYPE_STR, offsetof(struct cmd_mpris_data, mpris_service))

CMD_OPTS_GEN_STRUCTS(cmd_mpris, MPRIS_OPTIONS)

DECLARE_CMD(cmd_mpris) = {
	.name = "mpris",
	.data_size = sizeof (struct cmd_mpris_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_mpris),

	.func_init = cmd_mpris_init,
	.func_destroy = cmd_mpris_destroy,
	.func_recache = cmd_mpris_recache,
	.func_cevent = cmd_mpris_cevent
};
