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

unsigned net_add_if(const char *if_name);

void handle_netlink_read(void);

#endif // NETWORKING_H
