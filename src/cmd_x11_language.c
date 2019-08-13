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

#include <X11/Xlib.h>

struct cmd_x11_language_data {
	struct cmd_data_base base;

	char *lan1_def;
	char *lan1_upper;
	char *lan2_def;
	char *lan2_upper;

	Display *dpy;
	unsigned cached_index;
	unsigned cached_color;

	char *display;
};

static bool cmd_x11_language_init(struct cmd_data_base *_data) {
	struct cmd_x11_language_data *data = (struct cmd_x11_language_data *)_data;

	const char *display = data->display;
	if (!data->display && !(display = getenv("DISPLAY")))
		display = ":0";

	data->dpy = XOpenDisplay(display);
	free(data->display);
	data->display = NULL;
	if (data->dpy == NULL)
		return false;

	if (!data->lan1_def)
		return false;
	data->lan1_upper = strdup(data->lan1_def);
	for (char *ptr = data->lan1_upper; *ptr; ++ptr)
		*ptr &= ~0x20; // make upper case

	if (!data->lan2_def)
		data->lan2_def = strdup(data->lan1_def);
	data->lan2_upper = strdup(data->lan2_def);
	for (char *ptr = data->lan2_upper; *ptr; ++ptr)
		*ptr &= ~0x20; // make upper case

	return true;
}

static void cmd_x11_language_destroy(struct cmd_data_base *_data) {
	struct cmd_x11_language_data *data = (struct cmd_x11_language_data *)_data;

	free(data->lan1_def);
	free(data->lan1_upper);
	free(data->lan2_def);
	free(data->lan2_upper);

	XCloseDisplay(data->dpy);
}

static bool cmd_x11_language_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_x11_language_data *data = (struct cmd_x11_language_data *)_data;

	if (update) {
		XKeyboardState values;
		XGetKeyboardControl(data->dpy, &values);
		const unsigned long lan = values.led_mask;

#define BIT_MOVE(val,src,dst) (((val) & (1U << (src))) >> ((src) - (dst)))

		data->cached_index = BIT_MOVE(lan, 12, 1) | BIT_MOVE(lan, 0, 0);
		data->cached_color = ((lan & 0x2U) == 0);
	}

#define BAT_POS_CHECK(pos, field) \
	_Static_assert(offsetof(struct cmd_x11_language_data, field) - offsetof(struct cmd_x11_language_data, lan1_def) == (pos) * sizeof(char *), \
		"Wrong position for " # field)
			BAT_POS_CHECK(0, lan1_def);
			BAT_POS_CHECK(1, lan1_upper);
			BAT_POS_CHECK(2, lan2_def);
			BAT_POS_CHECK(3, lan2_upper);
#undef BAT_POS_CHECK

	if (data->cached_color)
		JSON_OUTPUT_COLOR(json_gen, g_general_settings.color_degraded);
	JSON_OUTPUT_KV(json_gen, "full_text", *(&data->lan1_def + data->cached_index));

	return true;
}

#define X11_LANG_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_x11_language_data, base.align)), \
	F("display", OPT_TYPE_STR, offsetof(struct cmd_x11_language_data, display)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_x11_language_data, base.interval)), \
	F("language1", OPT_TYPE_STR, offsetof(struct cmd_x11_language_data, lan1_def)), \
	F("language2", OPT_TYPE_STR, offsetof(struct cmd_x11_language_data, lan2_def)), \

CMD_OPTS_GEN_STRUCTS(cmd_x11_language, X11_LANG_OPTIONS)

DECLARE_CMD(cmd_x11_language) = {
	.name = "x11_language",
	.data_size = sizeof (struct cmd_x11_language_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_x11_language),

	.func_init = cmd_x11_language_init,
	.func_destroy = cmd_x11_language_destroy,
	.func_output = cmd_x11_language_output
};
