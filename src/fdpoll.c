#include "fdpoll.h"
#include "networking.h"
#include "main.h"
#include "ini_parser.h"

#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <yajl_tree.h>

struct fdpoll_data {
	void (*func_handle)(void *);
	void *data;
};
static struct {
	struct pollfd *fds;
	struct fdpoll_data *data;
	unsigned size;
} g_fdpoll;

static void fdpoll_put(struct pollfd *pos, int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	pos->fd = fd;
	pos->events = POLLIN;
}

void fdpoll_init(void) {
	g_fdpoll.fds = (struct pollfd *)malloc(sizeof(struct pollfd) * 2);
	g_fdpoll.size = 2;
	g_fdpoll.data = NULL;

	fdpoll_put(g_fdpoll.fds + 0, STDIN_FILENO);
	fdpoll_put(g_fdpoll.fds + 1, g_net_global.netlink_fd);
}

void fdpoll_add(int fd, void (*func_handle)(void *), void *data) {
	if (!func_handle) {
		fprintf(stderr, "empty function passed %s\n", strerror(errno));
		exit(1);
	}
	g_fdpoll.size++;
	const unsigned s = g_fdpoll.size;
	g_fdpoll.fds = (struct pollfd *)realloc(g_fdpoll.fds, sizeof(struct pollfd) * s);
	g_fdpoll.data = (struct fdpoll_data *)realloc(g_fdpoll.data, sizeof(struct fdpoll_data) * (s - 2));
	fdpoll_put(g_fdpoll.fds + (s - 1), fd);
	g_fdpoll.data[s-1].data = data;
	g_fdpoll.data[s-1].func_handle = func_handle;
}

static void handle_click_event(struct runs_list *runs) {
	char input[1024];
	char errbuf[1024];

	while (fgets(input, sizeof(input), stdin)) {
		char* walker = input;
		if (*walker == '[')
			walker++;
		if (*walker == '\0' || *walker == '\n')
			continue;
		if (*walker == ',')
			walker++;

		yajl_val node = yajl_tree_parse(walker, errbuf, sizeof(errbuf));
		if(YAJL_IS_OBJECT(node)) {
			const char *name = NULL, *instance = NULL;
			int button = -1;

			for (size_t i = 0; i < node->u.object.len; ++i ) {
				const char * key = node->u.object.keys[i];
				if (0 == memcmp(key, "name", 5))
					name = YAJL_GET_STRING(node->u.object.values[i]);
				else if (0 == memcmp(key, "instance", 9))
					instance = YAJL_GET_STRING(node->u.object.values[i]);
				else if (0 == memcmp(key, "button", 7))
					button = (int)YAJL_GET_INTEGER(node->u.object.values[i]);
			}

			if (name == NULL || button == -1) {
				fprintf(stderr, "is3-status: bad click event object: %s\n", input);
				continue;
			}
			FOREACH_RUN(run, runs) {
				if ((0 == strcmp(run->vtable->name, name)) && (instance == run->instance || 0 == strcmp(run->instance, instance))) {
					if (run->vtable->func_cevent)
						run->vtable->func_cevent(run->data, button);
					break;
				}
			}
		} else
			fprintf(stderr, "is3-status: unable to parse click event:\n>>> %s\n", input);
		yajl_tree_free(node);
	}
}

bool fdpoll_run(struct runs_list *runs) {
	struct pollfd *const fds = g_fdpoll.fds;
#ifdef PROFILE
	static int counter = 10000;
	if ((--counter) == 0)
		return false;
	int ret = poll(fds, g_fdpoll.size, 0);
#else
	int ret = poll(fds, g_fdpoll.size, 1000);
#endif
	if (ret < 0) {
		fprintf(stderr, "poll failed with %s\n", strerror(errno));
		return false;
	}
	if (ret > 0) {
		if (fds[0].revents & POLLIN)
			handle_click_event(runs);
		else if(fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fprintf(stderr, "is3-status: STDIN closed\n");
			fds[0].fd = -1;
		}

		if (fds[1].revents & POLLIN)
			handle_netlink_read();
		else if(fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fprintf(stderr, "is3-status: NETLINK closed\n");
			fds[1].fd = -1;
		}
		for (unsigned i = 2; i < g_fdpoll.size; i++) {
			if (fds[i].revents & POLLIN)
				g_fdpoll.data[i - 2].func_handle(g_fdpoll.data[i - 2].data);
			else if(fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
				fprintf(stderr, "is3-status: fd %d closed\n", fds[i].fd);
				fds[i].fd = -1;
			}
		}
	}

	return true;
}
