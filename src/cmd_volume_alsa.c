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
#include "vprint.h"

#include <alsa/asoundlib.h>

struct cmd_volume_alsa_data {
	struct cmd_data_base base;
	char *format;
	char *format_muted;

	snd_mixer_t *mixer;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *sid;

	long volume_min, volume_range;
	bool supportes_mute;

	char *device;
	char *mixer_name;
	long mixer_idx;
	long wheel_step;
};

static bool cmd_volume_alsa_init(struct cmd_data_base *_data) {
	struct cmd_volume_alsa_data *data = (struct cmd_volume_alsa_data *)_data;

	if (data->wheel_step <= 0)
		data->wheel_step = 2;
	if (data->format == NULL)
		return false;

	int err;

	if ((err = snd_mixer_open(&data->mixer, 0)) < 0) {
		fprintf(stderr, "is3-status: ALSA: Cannot open mixer: %s\n", snd_strerror(err));
		return false;
	}

	err = snd_mixer_attach(data->mixer, (data->device ? data->device : "default"));
	free(data->device);
	if (err < 0) {
		fprintf(stderr, "is3-status: ALSA: Cannot attach mixer to device: %s\n", snd_strerror(err));
		goto _error_mixer;
	}

	/* Register this mixer */
	if ((err = snd_mixer_selem_register(data->mixer, NULL, NULL)) < 0) {
		fprintf(stderr, "is3-status: ALSA: snd_mixer_selem_register: %s\n", snd_strerror(err));
		goto _error_mixer;
	}

	if ((err = snd_mixer_load(data->mixer)) < 0) {
		fprintf(stderr, "is3-status: ALSA: snd_mixer_load: %s\n", snd_strerror(err));
		goto _error_mixer;
	}

	snd_mixer_selem_id_malloc(&data->sid);
	if (data->sid == NULL) {
		goto _error_mixer;
	}
	/* Find the given mixer */
	snd_mixer_selem_id_set_index(data->sid, (unsigned int)data->mixer_idx);
	snd_mixer_selem_id_set_name(data->sid, (data->mixer_name ? data->mixer_name : "Master"));
	free(data->mixer_name);
	if (!(data->elem = snd_mixer_find_selem(data->mixer, data->sid))) {
		fprintf(stderr, "is3-status: ALSA: Cannot find mixer %s (index %i)\n",
				snd_mixer_selem_id_get_name(data->sid), snd_mixer_selem_id_get_index(data->sid));
		snd_mixer_selem_id_free(data->sid);
		data->sid = NULL;
		goto _error_mixer;
	}

	data->supportes_mute = snd_mixer_selem_has_playback_switch(data->elem);
	long min, max;
	snd_mixer_selem_get_playback_volume_range(data->elem, &min, &max);
	data->volume_min = min;
	data->volume_range = max - min;

	return true;
_error_mixer:
	snd_mixer_close(data->mixer);
	data->mixer = NULL;
	return false;
}

static void cmd_volume_alsa_destroy(struct cmd_data_base *_data) {
	struct cmd_volume_alsa_data *data = (struct cmd_volume_alsa_data *)_data;
	snd_mixer_close(data->mixer);
	snd_mixer_selem_id_free(data->sid);
	free(data->format);
	free(data->format_muted);
}

// generaterd using command ./gen-format.py vV
VPRINT_OPTS(cmd_volume_alsa_var_options, {0x00000000, 0x00000000, 0x00400000, 0x00400000});

static bool cmd_volume_alsa_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_volume_alsa_data *data = (struct cmd_volume_alsa_data *)_data;
	(void)update;

	int res;

	long mixer_volume;
	snd_mixer_handle_events(data->mixer);
	snd_mixer_selem_get_playback_volume(data->elem, 0, &mixer_volume);

	const int volume = (int)(((mixer_volume - data->volume_min) * 100 + data->volume_range / 2) / data->volume_range);
	const char *output_format = data->format;
	if (data->supportes_mute) {
		int pbval;
		if ((res = snd_mixer_selem_get_playback_switch(data->elem, 0, &pbval)) < 0)
			fprintf(stderr, "is3-status: ALSA: get_playback_switch: %s\n", snd_strerror(res));
		if (!pbval) {
			JSON_OUTPUT_COLOR(json_gen, g_general_settings.color_degraded);
			if (data->format_muted)
				output_format = data->format_muted;
		}
	}

	char buffer[256];
	struct vprint ctx = {cmd_volume_alsa_var_options, output_format, buffer, sizeof(buffer)};
	while ((res = vprint_walk(&ctx)) >= 0) {
		switch (res) {
			case 'v':
			case 'V':
				vprint_itoa(&ctx, volume);
				break;
		}
	}

	JSON_OUTPUT_K(json_gen, "full_text", buffer, sizeof(buffer) - ctx.remainingSize);

	return true;
}

static bool cmd_volume_alsa_cevent(struct cmd_data_base *_data, int event) {
	struct cmd_volume_alsa_data *data = (struct cmd_volume_alsa_data *)_data;

	int res;

	switch(event) {
		case CEVENT_MOUSE_MIDDLE:
			if (data->supportes_mute) {
				int pbval;
				if ((res = snd_mixer_selem_get_playback_switch(data->elem, 0, &pbval)) < 0)
					fprintf(stderr, "is3-status: ALSA: get_playback_switch: %s\n", snd_strerror(res));
				else if ((res = snd_mixer_selem_set_playback_switch(data->elem, 0, !pbval)) < 0)
					fprintf(stderr, "is3-status: ALSA: set_playback_switch: %s\n", snd_strerror(res));
				else
					return true;
			}
			break;
		case CEVENT_MOUSE_WHEEL_UP:
		case CEVENT_MOUSE_WHEEL_DOWN: {
			long val;
			snd_mixer_handle_events(data->mixer);
			snd_mixer_selem_get_playback_volume(data->elem, 0, &val);

			const long change = (data->wheel_step * data->volume_range + (100 / 2)) / 100;
			if (event == CEVENT_MOUSE_WHEEL_UP) {
				val += change;
				const long max = data->volume_min + data->volume_range;
				if (val > max)
					val = max;
			} else {
				val -= change;
				if (val < data->volume_min)
					val = data->volume_min;
			}
			snd_mixer_selem_set_playback_volume(data->elem, 0, val);
			return true;
		}
	}
	return false;
}

#define VOLUME_ALSA_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_volume_alsa_data, base.align)), \
	F("device", OPT_TYPE_STR, offsetof(struct cmd_volume_alsa_data, device)), \
	F("format", OPT_TYPE_STR, offsetof(struct cmd_volume_alsa_data, format)), \
	F("format_muted", OPT_TYPE_STR, offsetof(struct cmd_volume_alsa_data, format_muted)), \
	F("mixer", OPT_TYPE_STR, offsetof(struct cmd_volume_alsa_data, mixer_name)), \
	F("mixer_idx", OPT_TYPE_LONG, offsetof(struct cmd_volume_alsa_data, mixer_idx)), \
	F("wheel_step", OPT_TYPE_LONG, offsetof(struct cmd_volume_alsa_data, wheel_step))

static const char *const cmd_volume_alsa_options_names[] = {
	VOLUME_ALSA_OPTIONS(CMD_OPTS_GEN_NAME)
};

static const struct cmd_option cmd_volume_alsa_options[] = {
	VOLUME_ALSA_OPTIONS(CMD_OPTS_GEN_DATA)
};

DECLARE_CMD(cmd_volume_alsa) = {
	.name = "volume_alsa",
	.data_size = sizeof (struct cmd_volume_alsa_data),

	.opts = {
		.names = cmd_volume_alsa_options_names,
		.opts = cmd_volume_alsa_options,
		.size = sizeof(cmd_volume_alsa_options) / sizeof(cmd_volume_alsa_options[0])
	},

	.func_init = cmd_volume_alsa_init,
	.func_destroy = cmd_volume_alsa_destroy,
	.func_output = cmd_volume_alsa_output,
	.func_cevent = cmd_volume_alsa_cevent
};
