#include "main.h"

#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>

struct cmd_x11_language_data {
	struct cmd_data_base base;
	char *display;
	Display *dpy;
	const char *cached_text;
	const char *cached_color;
};

static bool cmd_x11_language_init(struct cmd_data_base *_data) {
	struct cmd_x11_language_data *data = (struct cmd_x11_language_data *)_data;
	if (data->display == NULL) {
		data->display = getenv("DISPLAY");
		if (data->display == NULL) {
			data->display = ":0";
		}
		data->display = strdup(data->display);
	}

	data->dpy = XOpenDisplay(data->display);

	free(data->display);
	data->display = NULL;

	return (data->dpy != NULL);
}

static void cmd_x11_language_destroy(struct cmd_data_base *_data) {
	struct cmd_x11_language_data *data = (struct cmd_x11_language_data *)_data;
	XCloseDisplay(data->dpy);
}

static bool cmd_x11_language_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_x11_language_data *data = (struct cmd_x11_language_data *)_data;

#define IS_BIT(val,bit) (((val) & (1U << (bit))) == 0)

#define IS_NUMLOCK(val) IS_BIT(val, 1)
#define CAPSLOCK_STR(val,cap,uncap) ((IS_BIT(val, 0)) ? (uncap) : (cap))

	if (update || !data->cached_text) {
		XKeyboardState values;
		XGetKeyboardControl(data->dpy, &values);
		const unsigned long lan = values.led_mask;

		if(!IS_BIT(lan,12)) { // Group 2
			data->cached_text = CAPSLOCK_STR(lan, "HEBREW", "Hebrew");
		} else {
			data->cached_text = CAPSLOCK_STR(lan, "ENGLISH", "English");
		}
		data->cached_color = IS_NUMLOCK(lan) ? g_general_settings.color_degraded : NULL;
	}

	if (data->cached_color)
		JSON_OUTPUT_COLOR(json_gen, data->cached_color);
	JSON_OUTPUT_KV(json_gen, "full_text", data->cached_text);

	return true;
}

#define X11_LANG_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_x11_language_data, base.align)), \
	F("display", OPT_TYPE_STR, offsetof(struct cmd_x11_language_data, display)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_x11_language_data, base.interval))

static const char *const cmd_x11_language_options_names[] = {
	X11_LANG_OPTIONS(CMD_OPTS_GEN_NAME)
};

static const struct cmd_option cmd_x11_language_options[] = {
	X11_LANG_OPTIONS(CMD_OPTS_GEN_DATA)
};

DECLARE_CMD(cmd_x11_language) = {
	.name = "x11_language",
	.data_size = sizeof (struct cmd_x11_language_data),

	.opts = {
		.names = cmd_x11_language_options_names,
		.opts = cmd_x11_language_options,
		.size = sizeof(cmd_x11_language_options) / sizeof(cmd_x11_language_options[0])
	},

	.func_init = cmd_x11_language_init,
	.func_destroy = cmd_x11_language_destroy,
	.func_output = cmd_x11_language_output
};
