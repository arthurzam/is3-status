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
#include <time.h>


#include <net/if.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <linux/genetlink.h>
#include <sys/socket.h>
#include <linux/rtnetlink.h>
#include <linux/nl80211.h>

#include <libnl3/netlink/attr.h>
#include <netlink/genl/genl.h>  //genl_connect, genlmsg_put

// https://alamot.github.io/nl80211/
// https://www.systutorials.com/docs/linux/man/8-genl-ctrl-list/
// https://mdlayher.com/blog/linux-netlink-and-go-part-2-generic-netlink/
// https://wiki.linuxfoundation.org/networking/generic_netlink_howto


struct cmd_wireless_data {
	struct cmd_data_base base;
	char *format_up;
	char *format_down;

	char *interface;
	int nl80211_id;
	int netlink_fd;
//	uint32_t netlink_seq;
	uint32_t netlink_pid;
	unsigned interface_idx;

	unsigned if_pos;
};

#define GENL_LEN(nlmsg) ((nlmsg)->nlmsg_len - GENL_HDRLEN - NLMSG_HDRLEN)
#define GENL_DATA(nlmsg) (void*)((unsigned char *)(nlmsg) + NLMSG_HDRLEN + GENL_HDRLEN)

#define NLA_DATA(nla) (void *)((char *)(nla) + NLA_HDRLEN)
#define NLA_DATA_CAST(nla,type) *(type *)NLA_DATA(nla)
#define NLA_OK(nla,len) \
	((len) >= (int)sizeof(struct nlattr) && (nla)->nla_len >= sizeof(struct nlattr) && (nla)->nla_len <= (len))
#define NLA_NEXT(nla,attrlen) \
	((attrlen) -= NLA_ALIGN((nla)->nla_len), (struct nlattr*)(void*)(((char*)(nla)) + NLA_ALIGN((nla)->nla_len)))
#define FOREACH_NLA(iter,begin,len) \
	for (struct nlattr *(iter) = (struct nlattr *)(begin); NLA_OK((iter), (len)); (iter) = NLA_NEXT((iter), (len)))

static int cmd_wireless_get_family_id(struct cmd_wireless_data *data) {
	int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);

	struct sockaddr_nl addr = {
		.nl_family = NETLINK_GENERIC,
		.nl_groups = 0,
		.nl_pid = data->netlink_pid
	};

	if (bind (fd, (struct sockaddr *) &addr, sizeof (addr)) < 0) {
		fprintf(stderr, "bind(netlink) failed: %s\n", strerror(errno));
		exit(1);
	}

	static struct {
		struct nlmsghdr nlhdr;
		struct genlmsghdr genhdr;
		struct nlattr attr;
		char family_name[8] __attribute__ ((aligned (NLA_ALIGNTO)));
	} buffer __attribute__ ((aligned (8))) = {
		.nlhdr = {
			.nlmsg_len = sizeof(buffer),
			.nlmsg_type = GENL_ID_CTRL,
			.nlmsg_flags = NLM_F_REQUEST,
			.nlmsg_pid = UINT32_MAX, // temp value
			.nlmsg_seq = 0
		},
		.genhdr = { .cmd = CTRL_CMD_GETFAMILY, .version = 1 },
		.attr = {
			.nla_len = NLA_ALIGN(NLA_HDRLEN + sizeof(buffer.family_name)),
			.nla_type = CTRL_ATTR_FAMILY_NAME
		},
		.family_name = "nl80211"
	};
	_Static_assert(sizeof (buffer) == 32, "it should be 32 bytes -> something happaned");

	buffer.nlhdr.nlmsg_pid = data->netlink_pid;
	send(fd, &buffer, sizeof(buffer), 0);

	char buf[4096];
	int id = -1;
	int status = (int)recv(fd, buf, sizeof(buf), 0);
	for (struct nlmsghdr *h = (struct nlmsghdr *)buf; NLMSG_OK(h, (unsigned)status); h = NLMSG_NEXT(h, status)) {
		if (h->nlmsg_type != GENL_ID_CTRL)
			continue;

		unsigned len = GENL_LEN(h);
		FOREACH_NLA(nla, GENL_DATA(h), len) {
			if ((nla->nla_type & NLA_TYPE_MASK) == CTRL_ATTR_FAMILY_ID) {
				id = NLA_DATA_CAST(nla, uint16_t);
				goto _exit;
			}
		}
	}
_exit:
	data->netlink_fd = fd;
	return id;
}

static unsigned if_index(const char *interface) {
	struct ifreq ifr;
	ifr.ifr_ifindex = -1;
	strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	ioctl(fd, SIOCGIFINDEX, &ifr);
	close(fd);
	return (unsigned)ifr.ifr_ifindex;
}

static bool cmd_wireless_init(struct cmd_data_base *_data) {
	struct cmd_wireless_data *data = (struct cmd_wireless_data *)_data;
	if (!data->interface)
		return false;
	if (!data->format_up)
		data->format_up = strdup("%a");

	data->interface_idx = if_index(data->interface);
	data->netlink_pid = (uint32_t)getpid() + 1;
//	data->netlink_seq = (unsigned)time(NULL);
	data->nl80211_id = cmd_wireless_get_family_id(data);

//	data->if_pos = net_add_if(data->interface);
	// data->interface is used in inner networking array

	return true;
}

static void cmd_wireless_destroy(struct cmd_data_base *_data) {
	struct cmd_wireless_data *data = (struct cmd_wireless_data *)_data;
	free(data->format_up);
	free(data->format_down);
	free(data->interface);
}

static bool cmd_wireless_recache(struct cmd_data_base *_data) {
	struct cmd_wireless_data *data = (struct cmd_wireless_data *)_data;

	const struct {
		struct nlmsghdr nlhdr;
		struct genlmsghdr genhdr;
		struct nlattr attr;
		uint32_t ifidx __attribute__ ((aligned (NLA_ALIGNTO)));
	} buffer = {
		.nlhdr = {
			.nlmsg_len = sizeof(buffer),
			.nlmsg_type = (uint16_t)data->nl80211_id,
			.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST,
			.nlmsg_pid = data->netlink_pid,
			.nlmsg_seq = 0
		},
		.genhdr = { .cmd = NL80211_CMD_GET_SCAN, .version = 0 },
		.attr = {
			.nla_len = NLA_ALIGN(NLA_HDRLEN + sizeof(buffer.ifidx)),
			.nla_type = NL80211_ATTR_IFINDEX
		},
		.ifidx = data->interface_idx
	};
	_Static_assert(sizeof (buffer) == 28, "it should be 28 bytes -> something happaned");

	send(data->netlink_fd, &buffer, sizeof(buffer), 0);

	char buf[4096];
	int status = (int)recv(data->netlink_fd, buf, sizeof(buf), 0);
	for (struct nlmsghdr *h = (struct nlmsghdr *)buf; NLMSG_OK(h, (unsigned)status); h = NLMSG_NEXT(h, status)) {
		// https://elixir.bootlin.com/linux/latest/source/net/wireless/nl80211.c#L8437

		// For query NL80211_CMD_GET_SCAN, the answer is:
		//		h->nlmsg_type == NL80211_CMD_NEW_SCAN_RESULTS

		unsigned len = GENL_LEN(h);
		FOREACH_NLA(nla, GENL_DATA(h), len) {
			switch (nla->nla_type & NLA_TYPE_MASK) {
				case NL80211_ATTR_BSS: {
					unsigned len2 = nla->nla_len - NLA_HDRLEN;
					FOREACH_NLA(nla_bss, NLA_DATA(nla), len2) {
						switch (nla_bss->nla_type & NLA_TYPE_MASK) {
							case NL80211_BSS_SIGNAL_MBM:
								fprintf(stderr, "signal = %u\n", NLA_DATA_CAST(nla_bss, uint32_t));
								break;
							case NL80211_BSS_FREQUENCY:
								fprintf(stderr, "freq:  %u\n", NLA_DATA_CAST(nla_bss, uint32_t));
								break;
						}
					}
					break;
				}
			}
		}
	}

	return true;
}

#define WIRELESS_OPTIONS(F) \
	F("align", OPT_TYPE_ALIGN, offsetof(struct cmd_wireless_data, base.align)), \
	F("format_down", OPT_TYPE_STR, offsetof(struct cmd_wireless_data, format_down)), \
	F("format_up", OPT_TYPE_STR, offsetof(struct cmd_wireless_data, format_up)), \
	F("interface", OPT_TYPE_STR, offsetof(struct cmd_wireless_data, interface))

CMD_OPTS_GEN_STRUCTS(cmd_wireless, WIRELESS_OPTIONS)

DECLARE_CMD(cmd_wireless) = {
	.name = "wireless",
	.data_size = sizeof (struct cmd_wireless_data),

	.opts = CMD_OPTS_GEN_DATA(cmd_wireless),

	.func_init = cmd_wireless_init,
	.func_destroy = cmd_wireless_destroy,
	.func_recache = cmd_wireless_recache
};

/* TODO: investigation
 *		we maybe can use to register ourself to SCAN multicast group
 *		the group itself - NL80211_MULTICAST_GROUP_SCAN (sends NL80211_CMD_NEW_SCAN_RESULTS packets)
 *		nl80211 groups - https://elixir.bootlin.com/linux/latest/source/net/wireless/nl80211.c#L52
 *		connect to multicast group - https://stackoverflow.com/questions/26265453/netlink-multicast-kernel-group
 *			it works using the function 'nl_socket_add_memberships'
 */
