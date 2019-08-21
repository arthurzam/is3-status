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
#include <errno.h>
#include <unistd.h>
#include <linux/genetlink.h>
#include <sys/socket.h>
#include <linux/rtnetlink.h>

#include <libnl3/netlink/attr.h>

// https://alamot.github.io/nl80211/
// https://www.systutorials.com/docs/linux/man/8-genl-ctrl-list/
// https://mdlayher.com/blog/linux-netlink-and-go-part-2-generic-netlink/
// https://wiki.linuxfoundation.org/networking/generic_netlink_howto

int get_family_id() {
	int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);

	struct sockaddr_nl addr = {
		.nl_family = NETLINK_GENERIC,
		.nl_groups = 0,
		.nl_pid = (uint32_t)getpid() + 1
	};

	if (bind (fd, (struct sockaddr *) &addr, sizeof (addr)) < 0) {
		fprintf(stderr, "bind(netlink) failed: %s\n", strerror(errno));
		exit(1);
	}

	struct {
		struct nlmsghdr nlhdr;
		struct genlmsghdr genhdr;
		struct nlattr attr;
		char family_name[8];
	} data = {
		.nlhdr = {
			.nlmsg_len = sizeof(data),
			.nlmsg_type = GENL_ID_CTRL,
			.nlmsg_flags = NLM_F_REQUEST,
			.nlmsg_pid = (uint32_t)getpid() + 1,
			.nlmsg_seq = (unsigned)time(NULL)
		},
		.genhdr = {
			.cmd = CTRL_CMD_GETFAMILY,
			.version = 1
		},
		.attr = {
			.nla_len = NLA_ALIGN(NLA_HDRLEN + 8),
			.nla_type = CTRL_ATTR_FAMILY_NAME
		},
		.family_name = "nl80211"
	};

	send(fd, &data, sizeof(data), 0);

	char buf[4096];
	int id = -1;
	int status = recv(fd, buf, sizeof(buf), 0);
	for (struct nlmsghdr *h = (struct nlmsghdr *)buf; NLMSG_OK(h, (unsigned)status); h = NLMSG_NEXT(h, status)) {
		if (h->nlmsg_type != GENL_ID_CTRL)
			continue;

#define GENL_LEN(nlmsg) ((nlmsg)->nlmsg_len - GENL_HDRLEN - NLMSG_HDRLEN)
#define NLA_OK(nla,len) \
	((len) >= (int)sizeof(struct nlattr) && (nla)->nla_len >= sizeof(struct nlattr) && (nla)->nla_len <= (len))
#define NLA_NEXT(nla,attrlen) \
	((attrlen) -= NLA_ALIGN((nla)->nla_len), (struct nlattr*)(void*)(((char*)(nla)) + NLA_ALIGN((nla)->nla_len)))
#define FOREACH_NLA(begin,len) \
	for(struct nlattr *nla = (struct nlattr *)(begin); NLA_OK(nla, (len)); nla = NLA_NEXT(nla, (len)))

		unsigned len = GENL_LEN(h);
		void *head = ((unsigned char *)(h) + NLMSG_HDRLEN + GENL_HDRLEN);
		FOREACH_NLA(head, len) {
			if ((nla->nla_type & NLA_TYPE_MASK) == CTRL_ATTR_FAMILY_ID) {
				id = *(const uint16_t *) nla_data(nla);
				goto _exit;
			}
		}
	}
_exit:
	close(fd);
	return id;
}
