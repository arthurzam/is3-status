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

#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

struct cmd_x11_language_data {
	struct cmd_data_base base;

	Display *dpy;
	int xkbEventType;

	char *display;

	char *lan1_def;
	char *lan1_upper;
	char *lan2_def;
	char *lan2_upper;
};

static void cmd_x11_language_recache(struct cmd_data_base *_data);

bool handle_x11_lan_events(void *arg) {
	struct cmd_x11_language_data *data = (struct cmd_x11_language_data *)arg;

	XEvent e;
	XNextEvent(data->dpy, &e);
	if (likely(e.type == data->xkbEventType)) {
		XkbEvent *xkbEvent = (XkbEvent *)&e;
		if (xkbEvent->any.xkb_type == XkbStateNotify || xkbEvent->any.xkb_type == XkbIndicatorStateNotify)
			cmd_x11_language_recache(arg);
	}
	return false;
}

static bool cmd_x11_language_init(struct cmd_data_base *_data) {
	struct cmd_x11_language_data *data = (struct cmd_x11_language_data *)_data;

	data->dpy = XOpenDisplay(data->display);
	free(data->display);
	data->display = NULL;
	if (!data->dpy)
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

	XKeysymToKeycode(data->dpy, XK_F1);
	XkbQueryExtension(data->dpy, 0, &data->xkbEventType, 0, 0, 0);
	XkbSelectEvents(data->dpy, XkbUseCoreKbd, XkbAllEventsMask, XkbStateNotifyMask | XkbIndicatorStateNotifyMask);
	XkbSelectEventDetails(data->dpy, XkbUseCoreKbd, XkbStateNotify, XkbAllStateComponentsMask, XkbGroupStateMask);
	XSync(data->dpy, false);

	fdpoll_add(ConnectionNumber(data->dpy), handle_x11_lan_events, data);

	data->base.interval = -1;
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

static void cmd_x11_language_recache(struct cmd_data_base *_data) {
	struct cmd_x11_language_data *data = (struct cmd_x11_language_data *)_data;

	XKeyboardState values;
	XGetKeyboardControl(data->dpy, &values);
	const unsigned long lan = values.led_mask;

#define BIT_MOVE(val,src,dst) (((val) & (1U << (src))) >> ((src) - (dst)))

	unsigned cached_index = BIT_MOVE(lan, 12, 1) | BIT_MOVE(lan, 0, 0);
	data->base.cached_fulltext = *(&data->lan1_def + cached_index);

	if ((lan & 0x2U) == 0)
		CMD_COLOR_SET(data, g_general_settings.color_degraded);
	else
		CMD_COLOR_CLEAN(data);

#define BAT_POS_CHECK(pos, field) \
	_Static_assert(offsetof(struct cmd_x11_language_data, field) - offsetof(struct cmd_x11_language_data, lan1_def) == (pos) * sizeof(char *), \
		"Wrong position for " # field)
			BAT_POS_CHECK(0, lan1_def);
			BAT_POS_CHECK(1, lan1_upper);
			BAT_POS_CHECK(2, lan2_def);
			BAT_POS_CHECK(3, lan2_upper);
#undef BAT_POS_CHECK
}

static bool cmd_x11_language_cevent(struct cmd_data_base *_data, unsigned event, unsigned modifiers) {
	(void) modifiers;
	struct cmd_x11_language_data *data = (struct cmd_x11_language_data *)_data;

	unsigned toogle_mask, check_mask;
	switch (event) {
		case CEVENT_MOUSE_LEFT: // Num Lock
			toogle_mask = 0x10;
			check_mask = (1U << 1);
			break;
		case CEVENT_MOUSE_RIGHT: // Caps Lock
			toogle_mask = 0x02;
			check_mask = (1U << 0);
			break;
		default: return false;
	}
	XKeyboardState values;
	XGetKeyboardControl(data->dpy, &values);
	unsigned value_mask = ((values.led_mask & check_mask) == 0) ? toogle_mask : 0;
	XkbLockModifiers(data->dpy, XkbUseCoreKbd, toogle_mask, value_mask);
	cmd_x11_language_recache(_data);
	return false;
}

#define X11_LANG_OPTIONS(F) \
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
	.func_recache = cmd_x11_language_recache,
	.func_cevent = cmd_x11_language_cevent
};
