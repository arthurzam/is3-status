#include "networking.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <netdb.h>
#include <errno.h>
#include <net/if.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#define FOREACH_RTA(begin,len) \
	for(struct rtattr *rta = (struct rtattr *)(begin); \
		RTA_OK(rta, (len)); \
		rta = RTA_NEXT(rta, (len)))

struct net_global_t g_net_global = {
	.ifs_arr = NULL,
	.ifs_size = 0,
	.netlink_fd = -1
};

static void setup_netlink() {
	g_net_global.netlink_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (g_net_global.netlink_fd < 0) {
		fprintf(stderr, "socket(netlink) failed: %s\n", strerror(errno));
		exit(1);
	}

	struct sockaddr_nl addr = {
		.nl_family = AF_NETLINK,
		.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE,
		.nl_pid = (uint32_t)getpid()
	};

	if (bind (g_net_global.netlink_fd, (struct sockaddr *) &addr, sizeof (addr)) < 0) {
		fprintf(stderr, "bind(netlink) failed: %s\n", strerror(errno));
		exit(1);
	}
}

static void net_query_info(struct net_if_addrs *curr_if) {
	struct ifaddrs *list = NULL;
	getifaddrs(&list);
	for (struct ifaddrs *iter = list; iter; iter = iter->ifa_next) {
		if (0 == strcmp(curr_if->if_name, iter->ifa_name)) {
			curr_if->is_down = (char)((iter->ifa_flags & (IFF_UP | IFF_RUNNING)) != (IFF_UP | IFF_RUNNING));
			getnameinfo(iter->ifa_addr, sizeof(struct sockaddr_in), curr_if->if_ip4, sizeof(curr_if->if_ip4), NULL, 0, NI_NUMERICHOST);
		}
	}
	freeifaddrs(list);
}

unsigned net_add_if(const char *if_name) {
	++g_net_global.ifs_size;
	g_net_global.ifs_arr = realloc(g_net_global.ifs_arr, sizeof(struct net_if_addrs) * g_net_global.ifs_size);
	struct net_if_addrs *const curr = g_net_global.ifs_arr + (g_net_global.ifs_size - 1);

	curr->if_name = if_name;
	curr->if_ip4[0] = '\0';
	curr->if_ip6[0] = '\0';
	curr->is_down = true;

	net_query_info(curr);

	if (g_net_global.netlink_fd == -1)
		setup_netlink();

	return g_net_global.ifs_size - 1;
}

static struct net_if_addrs *net_find_if(const char *if_name) {
	for(unsigned i = 0; i < g_net_global.ifs_size; i++)
		if(0 == strcmp(g_net_global.ifs_arr[i].if_name, if_name))
			return g_net_global.ifs_arr + i;
	return NULL;
}

void handle_netlink_read() {
	char buf[4096];
	struct iovec iov = { buf, sizeof buf };
	struct sockaddr_nl snl;
	struct msghdr msg = { (void *) &snl, sizeof snl, &iov, 1, NULL, 0, 0 };
	struct net_if_addrs *curr_if;
	long status;
	while((status = recvmsg(g_net_global.netlink_fd, &msg, 0)) > 0 || errno == EINTR) {
		if (status <= 0)
			continue;

		// DOCS: man 7 rtnetlink
		for (struct nlmsghdr *h = (struct nlmsghdr *) buf; NLMSG_OK (h, (unsigned int) status); h = NLMSG_NEXT (h, status)) {
			bool isDel = false;
			switch (h->nlmsg_type) {
				case NLMSG_ERROR:
					fprintf(stderr, "read_netlink : some kind of error\n");
					/* fall through */
				case NLMSG_DONE: // Finished reading
					return;
				case RTM_DELLINK:
					isDel = true;
					/* fall through */
				case RTM_GETLINK:
				case RTM_NEWLINK: {
					struct ifinfomsg *ifi = (struct ifinfomsg*)NLMSG_DATA(h);
					char *if_name = NULL;
					FOREACH_RTA(IFLA_RTA(ifi), h->nlmsg_len) {
						if (rta->rta_type == IFLA_IFNAME) {
							if_name = (char*)RTA_DATA(rta);
							break;
						}
					}
					if(if_name && (curr_if = net_find_if(if_name))) {
						if (isDel)
							curr_if->is_down = true;
						else
							curr_if->is_down = (char)((ifi->ifi_flags & (IFF_UP | IFF_RUNNING)) != (IFF_UP | IFF_RUNNING));
					}
					break;
				}
				case RTM_DELADDR:
					isDel = true;
					/* fall through */
				case RTM_GETADDR:
				case RTM_NEWADDR: {
					struct ifaddrmsg *ifa = (struct ifaddrmsg*)NLMSG_DATA(h);
					char *if_name = NULL;
					void *address = NULL;
					FOREACH_RTA(IFA_RTA(ifa), h->nlmsg_len) {
						switch (rta->rta_type) {
							case IFA_LABEL:
								if_name = (char*)RTA_DATA(rta);
								break;
							case IFA_ADDRESS:
							case IFA_LOCAL:
								address = RTA_DATA(rta);
								break;
						}
					}
					if(if_name && (curr_if = net_find_if(if_name))) {
						if (isDel) {
							if (ifa->ifa_family == AF_INET6)
								curr_if->if_ip6[0] = '\0';
							else if (ifa->ifa_family == AF_INET)
								curr_if->if_ip4[0] = '\0';
							break;
						} else {
							if (ifa->ifa_family == AF_INET6)
								inet_ntop(AF_INET6, address, curr_if->if_ip6, sizeof(curr_if->if_ip6));
							else if (ifa->ifa_family == AF_INET)
								inet_ntop(AF_INET , address, curr_if->if_ip4, sizeof(curr_if->if_ip4));
						}
					}
					break;
				}
			}
		}
	}
	if (status == 0) {
		fprintf(stderr, "netlink_read : EOF\n");
	} else if (status < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
		fprintf(stderr, "recvmsg(netlink) failed: %s\n", strerror(errno));
	}
}
