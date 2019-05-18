#include "main.h"
#include "vprint.h"

#include <string.h>
#include <stdio.h>

#include <time.h>

struct cmd_date_data {
	struct cmd_data_base base;
	char *format;
	char *timezone;
};

static const char *g_curr_tz = NULL;
static const char *g_local_tz = NULL;

static bool cmd_date_init(struct cmd_data_base *_data) {
	struct cmd_date_data *data = (struct cmd_date_data *)_data;
	if (data->format == NULL) {
		data->format = strdup("%a %Y-%m-%d %H:%M:%S");
	}

	if (g_local_tz == NULL)
		g_local_tz = getenv("TZ");

	if (data->timezone == NULL && g_local_tz)
		data->timezone = strdup(g_local_tz);

	return true;
}

static void cmd_date_destroy(struct cmd_data_base *_data) {
	struct cmd_date_data *data = (struct cmd_date_data *)_data;
	free(data->format);
	free(data->timezone);
}

static bool cmd_date_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_date_data *data = (struct cmd_date_data *)_data;
	(void)update;

	if (data->timezone != g_curr_tz) {
		if (data->timezone) {
			setenv("TZ", data->timezone, 1);
		} else {
			unsetenv("TZ");
		}
		tzset();
		g_curr_tz = data->timezone;
	}

	struct tm tm;
	char timebuf[1024];
	time_t t = time(NULL);
	localtime_r(&t, &tm);

	const size_t len = strftime(timebuf, sizeof(timebuf), data->format, &tm);
	JSON_OUTPUT_K(json_gen, "full_text", timebuf, len);

	return true;
}

#define DATE_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_date_data, base.align)), \
	F("format", OPT_TYPE_STR, offsetof(struct cmd_date_data, format)), \
	F("timezone", OPT_TYPE_STR, offsetof(struct cmd_date_data, timezone))

static const char *const cmd_date_options_names[] = {
	DATE_OPTIONS(CMD_OPTS_GEN_NAME)
};

static const struct cmd_option cmd_date_options[] = {
	DATE_OPTIONS(CMD_OPTS_GEN_DATA)
};

DECLARE_CMD(cmd_date) = {
	.name = "date",
	.data_size = sizeof (struct cmd_date_data),

	.opts = {
		.names = cmd_date_options_names,
		.opts = cmd_date_options,
		.size = sizeof(cmd_date_options) / sizeof(cmd_date_options[0])
	},

	.func_init = cmd_date_init,
	.func_destroy = cmd_date_destroy,
	.func_output = cmd_date_output
};
