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

#include <string.h>

#include <sys/statvfs.h>

struct cmd_disk_usage_data {
	struct cmd_data_base base;
	char *vfs_path;
	char *format;

	long use_decimal;
	long threshold_degraded;
	long threshold_critical;

	char cached_output[256];
};

static bool cmd_disk_usage_init(struct cmd_data_base *_data) {
	struct cmd_disk_usage_data *data = (struct cmd_disk_usage_data *)_data;
	if (!data->format)
		return false;
	if (!data->vfs_path)
		data->vfs_path = strdup("/");
	data->use_decimal = !!data->use_decimal;

	data->base.cached_fulltext = data->cached_output;
	return true;
}

static void cmd_disk_usage_destroy(struct cmd_data_base *_data) {
	struct cmd_disk_usage_data *data = (struct cmd_disk_usage_data *)_data;
	free(data->format);
	free(data->vfs_path);
}

// generaterd using command ./scripts/gen-format.py aAfFtuU
VPRINT_OPTS(cmd_disk_usage_var_options, {0x00000000, 0x00000000, 0x00200042, 0x00300042});

static bool cmd_disk_usage_recache(struct cmd_data_base *_data) {
	struct cmd_disk_usage_data *data = (struct cmd_disk_usage_data *)_data;

	struct statvfs buf;
	int res;

	if (statvfs(data->vfs_path, &buf) == 0) {
		struct vprint ctx = {cmd_disk_usage_var_options, data->format, data->cached_output, data->cached_output + sizeof(data->cached_output)};
		while ((res = vprint_walk(&ctx)) >= 0) {
			uint64_t value = 0;
			switch (res | 0x20) { // convert to lower case
				case 'a': value = (uint64_t)buf.f_bavail; break;
				case 'f': value = (uint64_t)buf.f_bfree; break;
				case 't': value = (uint64_t)buf.f_blocks; break;
				case 'u': value = (uint64_t)(buf.f_blocks - buf.f_bfree); break;
				default: __builtin_unreachable();
			}
			vprint_human_bytes(&ctx, value, ((res & 0x20) == 0 ? (uint64_t)buf.f_blocks : 0), (uint64_t)buf.f_bsize, data->use_decimal);
		}
#define DISK_THRESHOLD_CMP(threshold) ((threshold) >= 0 ? (buf.f_bfree * buf.f_blocks < (uint64_t)(threshold)) : buf.f_bfree * 100 < (uint64_t)(-(threshold)) * buf.f_blocks )
		if (DISK_THRESHOLD_CMP(data->threshold_critical))
			CMD_COLOR_SET(data, g_general_settings.color_bad);
		else if (DISK_THRESHOLD_CMP(data->threshold_degraded))
			CMD_COLOR_SET(data, g_general_settings.color_degraded);
		else
			CMD_COLOR_CLEAN(data);
	}

	return true;
}

#define DISK_USAGE_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_disk_usage_data, base.align)), \
	F("format", OPT_TYPE_STR, offsetof(struct cmd_disk_usage_data, format)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_disk_usage_data, base.interval)), \
	F("path", OPT_TYPE_STR, offsetof(struct cmd_disk_usage_data, vfs_path)), \
	F("threshold_critical", OPT_TYPE_BYTE_THRESHOLD, offsetof(struct cmd_disk_usage_data, threshold_critical)), \
	F("threshold_degraded", OPT_TYPE_BYTE_THRESHOLD, offsetof(struct cmd_disk_usage_data, threshold_degraded)), \
	F("use_decimal", OPT_TYPE_LONG, offsetof(struct cmd_disk_usage_data, use_decimal))

CMD_OPTS_GEN_STRUCTS(cmd_disk_usage, DISK_USAGE_OPTIONS)

DECLARE_CMD(cmd_disk_usage) = {
	.name = "disk_usage",
	.data_size = sizeof (struct cmd_disk_usage_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_disk_usage),

	.func_init = cmd_disk_usage_init,
	.func_destroy = cmd_disk_usage_destroy,
	.func_recache = cmd_disk_usage_recache
};
