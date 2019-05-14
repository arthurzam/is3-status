#include "general.h"
#include "vprint.h"

#include <string.h>
#include <stdio.h>

#include <sys/statvfs.h>

#define CACHE_TEXT_SIZE 256

struct cmd_disk_usage_data {
	struct cmd_data_base base;
	char *format;
	char *cached_text;
};

static bool cmd_disk_usage_init(struct cmd_data_base *_data) {
	struct cmd_disk_usage_data *data = (struct cmd_disk_usage_data *)_data;
	if (data->format == NULL) {
		data->format = strdup("Free: %F%%");
	}
	data->cached_text = malloc(CACHE_TEXT_SIZE);
	return true;
}

static void cmd_disk_usage_destroy(struct cmd_data_base *_data) {
	struct cmd_disk_usage_data *data = (struct cmd_disk_usage_data *)_data;
	free(data->format);
}

// generaterd using command ./gen-format.py fFuU
static const uint32_t cmd_disk_usage_var_options[8] = {0x00000000, 0x00000000, 0x00200040, 0x00200040, 0x00000000, 0x00000000, 0x00000000, 0x00000000};

static bool cmd_disk_usage_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_disk_usage_data *data = (struct cmd_disk_usage_data *)_data;

	if (update) {
		int res;
		struct vprint ctx = {cmd_disk_usage_var_options, data->format, data->cached_text, CACHE_TEXT_SIZE};
		while ((res = vprint_walk(&ctx)) >= 0) {
			const char *addr = NULL;
			switch (res) {

			}
			vprint_strcat(&ctx, addr);
		}
	}

//	JSON_OUTPUT_K(json_gen, "full_text", timebuf, len);

	return true;
}

#define DISK_USAGE_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_disk_usage_data, base.align)), \
	F("format", OPT_TYPE_STR, offsetof(struct cmd_disk_usage_data, format))

static const char *const cmd_disk_usage_options_names[] = {
	DISK_USAGE_OPTIONS(CMD_OPTS_GEN_NAME)
};

static const struct cmd_option cmd_disk_usage_options[] = {
	DISK_USAGE_OPTIONS(CMD_OPTS_GEN_DATA)
};

DECLARE_CMD(cmd_disk_usage) = {
	.name = "date",
	.data_size = sizeof (struct cmd_disk_usage_data),

	.opts = {
		.names = cmd_disk_usage_options_names,
		.opts = cmd_disk_usage_options,
		.size = sizeof(cmd_disk_usage_options) / sizeof(cmd_disk_usage_options[0])
	},

	.func_init = cmd_disk_usage_init,
	.func_destroy = cmd_disk_usage_destroy,
	.func_output = cmd_disk_usage_output
};
