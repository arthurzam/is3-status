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

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

struct cmd_memory_data {
	struct cmd_data_base base;

	int fd;
	char *format;

	long use_decimal;
	long use_method_classical;
	long threshold_degraded;
	long threshold_critical;

	char cached_output[256];
};

static bool cmd_memory_init(struct cmd_data_base *_data) {
	struct cmd_memory_data *data = (struct cmd_memory_data *)_data;

	if (!data->format)
		return false;

	data->use_decimal = !!data->use_decimal;
	data->use_method_classical = !!data->use_method_classical;
	data->base.cached_fulltext = data->cached_output;

	data->fd = open("/proc/meminfo", O_RDONLY);
	return data->fd >= 0;
}

static void cmd_memory_destroy(struct cmd_data_base *_data) {
	struct cmd_memory_data *data = (struct cmd_memory_data *)_data;
	free(data->format);
	close(data->fd);
}

struct memory_info_t {
	int64_t ram_total;
	int64_t ram_available;
	int64_t ram_free;
	int64_t ram_buffers;
	int64_t ram_cached;
	int64_t ram_shared;
} __attribute__ ((aligned (sizeof(int64_t))));

__attribute__((always_inline)) inline bool cmd_memory_file(struct memory_info_t *info, int fd) {
#define MEM_OPT(str, field) {str, X_STRLEN(str), offsetof(struct memory_info_t, field) / sizeof(uint64_t)}
	static const struct {
		// 14 is the longest("MemAvailable:") + 1
		char str[14] __attribute__ ((aligned (8)));
		uint8_t str_len;
		uint8_t offset;
	} g_mem_opts[] = {
		// formatted as a BTree in BFS
		/* 3 */ MEM_OPT("MemFree:", ram_free),
		/* 1 */ MEM_OPT("Cached:", ram_cached),
		/* 5 */ MEM_OPT("Shmem:", ram_shared),
		/* 0 */ MEM_OPT("Buffers:", ram_buffers),
		/* 2 */ MEM_OPT("MemAvailable:", ram_available),
		/* 4 */ MEM_OPT("MemTotal:", ram_total),
	};
#undef MEM_OPT

	char buffer[2048];
	ssize_t buf_len;
	unsigned offset = 0, found = 0;
	lseek(fd, 0, SEEK_SET);
	while (0 < (buf_len = offset + read(fd, buffer + offset, sizeof(buffer) - 1 - offset))) {
		buffer[buf_len] = '\0';
		char *start = buffer;
		while (true) {
			unsigned pos = 0;
			do {
				const int cmp_res = memcmp(g_mem_opts[pos].str, start, g_mem_opts[pos].str_len);
				if (cmp_res == 0) {
					int64_t *const dst = ((int64_t *)info) + g_mem_opts[pos].offset;
					start += g_mem_opts[pos].str_len;
					*dst = atoll(start) * 1024;
					if (++found == ARRAY_SIZE(g_mem_opts))
						return true;
					break;
				} else
					pos = (2 * pos) + (1 + !!(cmp_res < 0));
			} while (pos < ARRAY_SIZE(g_mem_opts));
			char *endl = strchr(start, '\n');
			if (!endl)
				break;
			start = endl + 1;
		}
		offset = (unsigned)(buffer + buf_len - start);
		memmove(buffer, start, offset);
	}
	return (found == ARRAY_SIZE(g_mem_opts));
}

// generaterd using command ./scripts/gen-format.py AaFfSstUu
VPRINT_OPTS(cmd_memory_data_var_options, {0x00000000, 0x00000000, 0x00280042, 0x00380042});

static void cmd_memory_recache(struct cmd_data_base *_data) {
	struct cmd_memory_data *data = (struct cmd_memory_data *)_data;

	struct memory_info_t info = {0};
	if (likely(cmd_memory_file(&info, data->fd))) {
		unsigned res;
		struct vprint ctx = {cmd_memory_data_var_options, data->format, data->cached_output, data->cached_output + sizeof(data->cached_output)};
		while ((res = vprint_walk(&ctx)) != 0) {
			int64_t value;
			switch (res | 0x20) { // convert to lower case
				case 'a': value = info.ram_available; break;
				case 'f': value = info.ram_free; break;
				case 's': value = info.ram_shared; break;
				case 't': value = info.ram_total; break;
				case 'u':
					value = info.ram_total - (data->use_method_classical ? info.ram_free - info.ram_buffers - info.ram_cached :
																		   info.ram_available);
					break;
				default: __builtin_unreachable();
			}
			vprint_human_bytes(&ctx, (uint64_t)value, ((res & 0x20) == 0 ? (uint64_t)info.ram_total : 0), 1, data->use_decimal);
		}
#define MEM_THRESHOLD_CMP(threshold) (info.ram_free < ((threshold) >= 0 ? (threshold) : -(threshold) * info.ram_total / 100))

		if (MEM_THRESHOLD_CMP(data->threshold_critical))
			CMD_COLOR_SET(data, g_general_settings.color_bad);
		else if (MEM_THRESHOLD_CMP(data->threshold_degraded))
			CMD_COLOR_SET(data, g_general_settings.color_degraded);
		else
			CMD_COLOR_CLEAN(data);
	}
}

#define MEMORY_OPTIONS(F) \
	F("format", OPT_TYPE_STR, offsetof(struct cmd_memory_data, format)), \
	F("interval", OPT_TYPE_LONG, offsetof(struct cmd_memory_data, base.interval)), \
	F("threshold_critical", OPT_TYPE_BYTE_THRESHOLD, offsetof(struct cmd_memory_data, threshold_critical)), \
	F("threshold_degraded", OPT_TYPE_BYTE_THRESHOLD, offsetof(struct cmd_memory_data, threshold_degraded)), \
	F("use_decimal", OPT_TYPE_LONG, offsetof(struct cmd_memory_data, use_decimal)), \
	F("use_method_classical", OPT_TYPE_LONG, offsetof(struct cmd_memory_data, use_method_classical)), \

CMD_OPTS_GEN_STRUCTS(cmd_memory, MEMORY_OPTIONS)

DECLARE_CMD(cmd_memory) = {
	.name = "memory",
	.data_size = sizeof (struct cmd_memory_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_memory),

	.func_init = cmd_memory_init,
	.func_destroy = cmd_memory_destroy,
	.func_recache = cmd_memory_recache
};
