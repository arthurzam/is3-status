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

#ifndef NETWORKING_H
#define NETWORKING_H

struct net_if_addrs {
	const char *if_name;
	char is_down;
	char if_ip4[16]; // [INET_ADDRSTRLEN]
	char if_ip6[46]; // [INET6_ADDRSTRLEN]
};

extern struct net_global_t {
	struct net_if_addrs* ifs_arr;
	unsigned ifs_size;

	int netlink_fd;
} g_net_global;

#define NET_ADD_IF_FAILED ((unsigned)-1)
unsigned net_add_if(const char *if_name);

#endif // NETWORKING_H
