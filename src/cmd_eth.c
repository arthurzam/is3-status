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
#include "networking.h"

#include <string.h>
#include <stdio.h>

struct cmd_eth_data {
	struct cmd_data_base base;
	char *format_up;
	char *format_down;

	char *interface;

	unsigned if_pos;
};

static bool cmd_eth_init(struct cmd_data_base *_data) {
	struct cmd_eth_data *data = (struct cmd_eth_data *)_data;
	if (!data->interface)
		return false;
	if (!data->format_up)
		data->format_up = strdup("%a");

	data->if_pos = net_add_if(data->interface);
	// data->interface is used in inner networking array

	return true;
}

static void cmd_eth_destroy(struct cmd_data_base *_data) {
	struct cmd_eth_data *data = (struct cmd_eth_data *)_data;
	free(data->format_up);
	free(data->format_down);
	free(data->interface);
}

// generaterd using command ./gen-format.py Aa46
VPRINT_OPTS(cmd_eth_var_options, {0x00000000, 0x00500000, 0x00000002, 0x00000002});

static bool cmd_eth_output(struct cmd_data_base *_data, yajl_gen json_gen, bool update) {
	struct cmd_eth_data *data = (struct cmd_eth_data *)_data;
	(void)update;

	struct net_if_addrs *curr_if = g_net_global.ifs_arr + data->if_pos;
	const char *output_format = (curr_if->is_down && data->format_down) ? data->format_down : data->format_up;
	const char *color = curr_if->is_down ? color = g_general_settings.color_bad : g_general_settings.color_good;

	int res;
	char buffer[256];
	struct vprint ctx = {cmd_eth_var_options, output_format, buffer, buffer + sizeof(buffer)};
	while ((res = vprint_walk(&ctx)) >= 0) {
		const char *addr = NULL;
		switch (res) {
			case 'a':
			case 'A':
				if (curr_if->if_ip4[0] != '\0') {
					addr = curr_if->if_ip4;
					break;
				}
				/* fall through */
			case '6':
				if (curr_if->if_ip6[0] != '\0')
					addr = curr_if->if_ip6;
				break;
			case '4':
				if (curr_if->if_ip4[0] != '\0')
					addr = curr_if->if_ip4;
				break;
		}
		if (!addr) {
			addr = "no IP";
			color = g_general_settings.color_degraded;
		}
		vprint_strcat(&ctx, addr);
	}

	JSON_OUTPUT_COLOR(json_gen, color);
	JSON_OUTPUT_K(json_gen, "full_text", buffer, (size_t)(ctx.buffer_start - buffer));
	return true;
}

#define ETH_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_eth_data, base.align)), \
	F("format_down", OPT_TYPE_STR, offsetof(struct cmd_eth_data, format_down)), \
	F("format_up", OPT_TYPE_STR, offsetof(struct cmd_eth_data, format_up)), \
	F("interface", OPT_TYPE_STR, offsetof(struct cmd_eth_data, interface))

CMD_OPTS_GEN_STRUCTS(cmd_eth, ETH_OPTIONS)

DECLARE_CMD(cmd_eth) = {
	.name = "eth",
	.data_size = sizeof (struct cmd_eth_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_eth),

	.func_init = cmd_eth_init,
	.func_destroy = cmd_eth_destroy,
	.func_output = cmd_eth_output
};
