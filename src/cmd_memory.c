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
#include <stdio.h>

struct cmd_memory_data {
	struct cmd_data_base base;

	char *format;

	long use_decimal;
	long use_method_classical;
	union {
		// if positive it means the exact value
		// if negetive it means the percentage
		long val;
		char *str;
	} threshold_degraded, threshold_critical;

	const char *cached_color;
	char cached_output[256];
};

static bool cmd_memory_init(struct cmd_data_base *_data) {
	struct cmd_memory_data *data = (struct cmd_memory_data *)_data;

	if (data->format == NULL)
		return false;

	data->use_decimal = !!data->use_decimal;
	data->use_method_classical = !!data->use_method_classical;

	if (data->threshold_degraded.str) {
		long temp = parse_human_bytes(data->threshold_degraded.str);
		free(data->threshold_degraded.str);
		data->threshold_degraded.val = temp;
	}
	if (data->threshold_critical.str) {
		long temp = parse_human_bytes(data->threshold_critical.str);
		free(data->threshold_critical.str);
		data->threshold_critical.val = temp;
	}

	return true;
}

static void cmd_memory_destroy(struct cmd_data_base *_data) {
	struct cmd_memory_data *data = (struct cmd_memory_data *)_data;
	free(data->format);
}

struct memory_info_t {
	int64_t ram_total;
	int64_t ram_available;
	int64_t ram_free;
	int64_t ram_buffers;
	int64_t ram_cached;
	int64_t ram_shared;
} __attribute__ ((aligned (sizeof(int64_t))));

static bool cmd_memory_file(struct memory_info_t *info) {
#define MEM_OPT(str, field) {str, X_STRLEN(str), offsetof(struct memory_info_t, field) / sizeof(uint64_t)}
	static const struct {
		// 14 is the longest("MemAvailable:") + 1
		char str[14] __attribute__ ((aligned (8)));
		uint8_t str_len;
		uint8_t offset;
	} g_mem_opts[] = {
		MEM_OPT("Buffers:", ram_buffers),
		MEM_OPT("Cached:", ram_cached),
		MEM_OPT("MemAvailable:", ram_available),
		MEM_OPT("MemFree:", ram_free),
		MEM_OPT("MemTotal:", ram_total),
		MEM_OPT("Shmem:", ram_shared),
	};
#undef MEM_OPT

	FILE *memFile = fopen("/proc/meminfo", "r");
	if (!memFile)
		return false;
	unsigned found = 0;
	char line[128];
	while (fgets(line, sizeof(line), memFile)) {
		int bottom = 0;
		int top = (sizeof(g_mem_opts) / sizeof(g_mem_opts[0])) - 1;
		while (bottom <= top) {
			const int mid = (bottom + top) / 2;
			const int cmp_res = memcmp(g_mem_opts[mid].str, line, g_mem_opts[mid].str_len);
			if (cmp_res == 0) {
				int64_t *const dst = ((int64_t *)info) + g_mem_opts[mid].offset;
				*dst = atoll(line + g_mem_opts[mid].str_len) * 1024;
				if (++found == sizeof(g_mem_opts) / sizeof(g_mem_opts[0]))
					goto _exit; // found all
				break;
			} else if (cmp_res > 0)
				top = mid - 1;
			else
				bottom = mid + 1;
		}
	}
_exit:
	fclose(memFile);
	return (found == sizeof(g_mem_opts) / sizeof(g_mem_opts[0]));
}

// generaterd using command ./scripts/gen-format.py AaFfSstUu
VPRINT_OPTS(cmd_memory_data_var_options, {0x00000000, 0x00000000, 0x00280042, 0x00380042});

static bool cmd_memory_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_memory_data *data = (struct cmd_memory_data *)_data;

	struct memory_info_t info = {0};
	if ((update || data->cached_output[0] == '\0') && cmd_memory_file(&info)) {
		int res;
		struct vprint ctx = {cmd_memory_data_var_options, data->format, data->cached_output, sizeof(data->cached_output)};
		while ((res = vprint_walk(&ctx)) >= 0) {
			int64_t value = 0;
			switch (res | 0x20) { // convert to lower case
				case 'a': value = info.ram_available; break;
				case 'f': value = info.ram_free; break;
				case 's': value = info.ram_shared; break;
				case 't': value = info.ram_total; break;
				case 'u':
					value = info.ram_total - (data->use_method_classical ? info.ram_free - info.ram_buffers - info.ram_cached :
																		   info.ram_available);
					break;
			}
			vprint_human_bytes(&ctx, (uint64_t)value, ((res & 0x20) == 0 ? (uint64_t)info.ram_total : 0), 1, data->use_decimal);
#define MEM_THRESHOLD_CMP(threshold, info) ((info)->ram_free < ((threshold) >= 0 ? (threshold) : -(threshold) * (info)->ram_total / 100))
			if (MEM_THRESHOLD_CMP(data->threshold_critical.val, &info))
				data->cached_color = g_general_settings.color_bad;
			else if (MEM_THRESHOLD_CMP(data->threshold_degraded.val, &info))
				data->cached_color = g_general_settings.color_degraded;
		}
	}

	if (data->cached_color)
		JSON_OUTPUT_COLOR(json_gen, data->cached_color);
	JSON_OUTPUT_KV(json_gen, "full_text", data->cached_output);
	return true;
}

#define MEMORY_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_memory_data, base.align)), \
	F("format", OPT_TYPE_STR, offsetof(struct cmd_memory_data, format)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_memory_data, base.interval)), \
	F("threshold_critical", OPT_TYPE_STR, offsetof(struct cmd_memory_data, threshold_critical.str)), \
	F("threshold_degraded", OPT_TYPE_STR, offsetof(struct cmd_memory_data, threshold_degraded.str)), \
	F("use_decimal", OPT_TYPE_LONG, offsetof(struct cmd_memory_data, use_decimal)), \
	F("use_method_classical", OPT_TYPE_LONG, offsetof(struct cmd_memory_data, use_method_classical)), \

static const char *const cmd_memory_options_names[] = {
	MEMORY_OPTIONS(CMD_OPTS_GEN_NAME)
};

static const struct cmd_option cmd_memory_options[] = {
	MEMORY_OPTIONS(CMD_OPTS_GEN_DATA)
};

DECLARE_CMD(cmd_memory) = {
	.name = "memory",
	.data_size = sizeof (struct cmd_memory_data),

	.opts = {
		.names = cmd_memory_options_names,
		.opts = cmd_memory_options,
		.size = sizeof(cmd_memory_options) / sizeof(cmd_memory_options[0])
	},

	.func_init = cmd_memory_init,
	.func_destroy = cmd_memory_destroy,
	.func_output = cmd_memory_output
};
