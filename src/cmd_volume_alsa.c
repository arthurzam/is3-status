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

#include <alloca.h>

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

	char cached_output[256];
};

static void cmd_volume_alsa_recache(struct cmd_data_base *_data);

static int cmd_volume_alsa_mixer_event(snd_mixer_elem_t *elem, unsigned int mask) {
	if (mask & SND_CTL_EVENT_MASK_VALUE)
		cmd_volume_alsa_recache((struct cmd_data_base *)snd_mixer_elem_get_callback_private(elem));
	return 0;
}

static bool handle_volume_alsa_read(void *arg) {
	snd_mixer_t *handle = (snd_mixer_t *)arg;
	snd_mixer_handle_events(handle);
	return false;
}

static bool cmd_volume_alsa_init(struct cmd_data_base *_data) {
	struct cmd_volume_alsa_data *data = (struct cmd_volume_alsa_data *)_data;

	if (data->wheel_step <= 0)
		data->wheel_step = 2;
	if (!data->format)
		return false;

	int err;

	if ((err = snd_mixer_open(&data->mixer, 0)) < 0) {
		fprintf(stderr, "ALSA: Cannot open mixer: %s\n", snd_strerror(err));
		return false;
	}

	err = snd_mixer_attach(data->mixer, (data->device ? data->device : "default"));
	free(data->device);
	if (err < 0) {
		fprintf(stderr, "ALSA: Cannot attach mixer to device: %s\n", snd_strerror(err));
		goto _error_mixer;
	}

	/* Register this mixer */
	if ((err = snd_mixer_selem_register(data->mixer, NULL, NULL)) < 0) {
		fprintf(stderr, "ALSA: snd_mixer_selem_register: %s\n", snd_strerror(err));
		goto _error_mixer;
	}

	if ((err = snd_mixer_load(data->mixer)) < 0) {
		fprintf(stderr, "ALSA: snd_mixer_load: %s\n", snd_strerror(err));
		goto _error_mixer;
	}


	snd_mixer_selem_id_malloc(&data->sid);
	if (!data->sid) {
		goto _error_mixer;
	}
	/* Find the given mixer */
	snd_mixer_selem_id_set_index(data->sid, (unsigned int)data->mixer_idx);
	snd_mixer_selem_id_set_name(data->sid, (data->mixer_name ? data->mixer_name : "Master"));
	free(data->mixer_name);
	if (!(data->elem = snd_mixer_find_selem(data->mixer, data->sid))) {
		fprintf(stderr, "ALSA: Cannot find mixer\n");
		snd_mixer_selem_id_free(data->sid);
		data->sid = NULL;
		goto _error_mixer;
	}

	data->supportes_mute = snd_mixer_selem_has_playback_switch(data->elem);
	long min, max;
	snd_mixer_selem_get_playback_volume_range(data->elem, &min, &max);
	data->volume_min = min;
	data->volume_range = max - min;

	/* add callback for mixer */
	{
		snd_mixer_elem_set_callback(data->elem, cmd_volume_alsa_mixer_event);
		snd_mixer_elem_set_callback_private(data->elem, data);

		unsigned count = (unsigned)snd_mixer_poll_descriptors_count(data->mixer);
		struct pollfd *polls = alloca(sizeof(struct pollfd) * count);
		count = (unsigned)snd_mixer_poll_descriptors(data->mixer, polls, count);
		for (unsigned i = 0; i < count; ++i)
			fdpoll_add(polls[i].fd, handle_volume_alsa_read, data->mixer);
	}

	data->base.cached_fulltext = data->cached_output;
	data->base.interval = -1;
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

// generaterd using command ./scripts/gen-format.py vV
VPRINT_OPTS(cmd_volume_alsa_var_options, {0x00000000, 0x00000000, 0x00400000, 0x00400000});

static void cmd_volume_alsa_recache(struct cmd_data_base *_data) {
	struct cmd_volume_alsa_data *data = (struct cmd_volume_alsa_data *)_data;

	long mixer_volume;
	snd_mixer_selem_get_playback_volume(data->elem, 0, &mixer_volume);

	const int volume = (int)(((mixer_volume - data->volume_min) * 100 + data->volume_range / 2) / data->volume_range);
	const char *output_format = data->format;
	CMD_COLOR_CLEAN(data);
	if (data->supportes_mute) {
		int pbval, res;
		if ((res = snd_mixer_selem_get_playback_switch(data->elem, 0, &pbval)) < 0)
			fprintf(stderr, "ALSA: get_playback_switch: %s\n", snd_strerror(res));
		if (!pbval) {
			CMD_COLOR_SET(data, g_general_settings.color_degraded);
			if (data->format_muted)
				output_format = data->format_muted;
		}
	}

	struct vprint ctx = {cmd_volume_alsa_var_options, output_format, data->cached_output, data->cached_output + sizeof(data->cached_output)};
	while (vprint_walk(&ctx) != 0) {
		vprint_itoa(&ctx, volume);
	}
}

static void cmd_volume_alsa_cevent(struct cmd_data_base *_data, unsigned event, unsigned modifiers) {
	(void) modifiers;
	struct cmd_volume_alsa_data *data = (struct cmd_volume_alsa_data *)_data;
	int res;
	switch (event) {
		case CEVENT_MOUSE_MIDDLE:
			if (data->supportes_mute) {
				int pbval;
				if ((res = snd_mixer_selem_get_playback_switch(data->elem, 0, &pbval)) < 0)
					fprintf(stderr, "ALSA: get_playback_switch: %s\n", snd_strerror(res));
				else if ((res = snd_mixer_selem_set_playback_switch(data->elem, 0, !pbval)) < 0)
					fprintf(stderr, "ALSA: set_playback_switch: %s\n", snd_strerror(res));
				else
					cmd_volume_alsa_recache(_data);
			}
			break;
		case CEVENT_MOUSE_WHEEL_UP:
		case CEVENT_MOUSE_WHEEL_DOWN: {
			long val;
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
			cmd_volume_alsa_recache(_data);
			break;
		}
	}
}

#define VOLUME_ALSA_OPTIONS(F) \
	F("device", OPT_TYPE_STR, offsetof(struct cmd_volume_alsa_data, device)), \
	F("format", OPT_TYPE_STR, offsetof(struct cmd_volume_alsa_data, format)), \
	F("format_muted", OPT_TYPE_STR, offsetof(struct cmd_volume_alsa_data, format_muted)), \
	F("mixer", OPT_TYPE_STR, offsetof(struct cmd_volume_alsa_data, mixer_name)), \
	F("mixer_idx", OPT_TYPE_LONG, offsetof(struct cmd_volume_alsa_data, mixer_idx)), \
	F("wheel_step", OPT_TYPE_LONG, offsetof(struct cmd_volume_alsa_data, wheel_step))

CMD_OPTS_GEN_STRUCTS(cmd_volume_alsa, VOLUME_ALSA_OPTIONS)

DECLARE_CMD(cmd_volume_alsa) = {
	.name = "volume_alsa",
	.data_size = sizeof (struct cmd_volume_alsa_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_volume_alsa),

	.func_init = cmd_volume_alsa_init,
	.func_destroy = cmd_volume_alsa_destroy,
	.func_recache = cmd_volume_alsa_recache,
	.func_cevent = cmd_volume_alsa_cevent
};
