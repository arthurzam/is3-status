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

#include "fdpoll.h"
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>

struct fdpoll_data {
	bool (*func_handle)(void *);
	void *data;
};
static struct {
	struct pollfd *fds;
	struct fdpoll_data *data;
	unsigned size;
} g_fdpoll = {NULL, NULL, 0};

void fdpoll_add(int fd, bool(*func_handle)(void *), void *data) {
	const unsigned s = g_fdpoll.size;
	g_fdpoll.size++;
	g_fdpoll.fds = (struct pollfd *)realloc(g_fdpoll.fds, sizeof(struct pollfd) * g_fdpoll.size);
	g_fdpoll.data = (struct fdpoll_data *)realloc(g_fdpoll.data, sizeof(struct fdpoll_data) * g_fdpoll.size);

	int flags;
	if (likely(0 <= (flags = fcntl(fd, F_GETFL, 0))))
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	g_fdpoll.fds[s].fd = fd;
	g_fdpoll.fds[s].events = POLLIN;
	g_fdpoll.fds[s].revents = 0;
	g_fdpoll.data[s].data = data;
	g_fdpoll.data[s].func_handle = func_handle;
}

int fdpoll_run(void) {
	struct pollfd *const fds = g_fdpoll.fds;
#ifdef PROFILE
	static int counter = 10000;
	if ((--counter) == 0)
		return -1;
	int ret = poll(fds, g_fdpoll.size, 0);
#else
	int ret = poll(fds, g_fdpoll.size, 1000);
#endif
	int res = 0;
	if (unlikely(ret < 0)) {
		fprintf(stderr, "fdpoll: failed with %s\n", strerror(errno));
		return -1;
	} else if (ret > 0) {
		for (unsigned i = 0; i < g_fdpoll.size; i++) {
			if (fds[i].revents & POLLIN) {
				if (g_fdpoll.data[i].func_handle(g_fdpoll.data[i].data))
					res = 1;
			}
			if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
				fprintf(stderr, "fdpoll: fd %d closed\n", fds[i].fd);
				fds[i].fd = -1;
			}
		}
	}
	return res;
}
