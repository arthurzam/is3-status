#include "ini_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/uio.h>

#include <yajl_version.h>
#include <yajl_tree.h>

#include "networking.h"

static FILE *open_config() {
	const char *path = getenv("IS3_STATUS_CONFIG");
	if (path && access(path, R_OK) == 0)
		return fopen(path, "r");




	return NULL;
}

static void setup_global_settings() {
	if (g_general_settings.color_good[0] == '\0')
		memcpy(g_general_settings.color_good, "#00FF00", 8);
	if (g_general_settings.color_degraded[0] == '\0')
		memcpy(g_general_settings.color_degraded, "#FFFF00", 8);
	if (g_general_settings.color_bad[0] == '\0')
		memcpy(g_general_settings.color_bad, "#FF0000", 8);
}

static void handle_click_event(struct run_instance *runs_begin, struct run_instance * runs_end) {
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
			for(struct run_instance *run = runs_begin; run != runs_end; run++) {
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

static void set_nonblocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main()
{
	FILE *ini = open_config();
	if (ini == NULL) {
		fprintf(stderr, "is3-status: Couldn't find config file\n");
		return 1;
	}
	unsigned count = 0;
	struct run_instance *runs = ini_parse(ini, &count);
	fclose(ini);

	if (runs == NULL || count == 0) {
		fprintf(stderr, "is3-status: Couldn't load config file\n");
		return 1;
	}

	setup_global_settings();

	struct run_instance *const runs_end = runs + count;
	for(struct run_instance *run = runs; run != runs_end; run++) {
		run->vtable->func_init(run->data);
		if (!run->data->align)
			run->data->align = g_general_settings.align;
		if (run->data->interval < g_general_settings.interval)
			run->data->interval = g_general_settings.interval;
	}

	struct iovec iov[2] = {	{NULL, 0}, 	{"\n", 1} };

#define WRITE_LEN(str) write(STDOUT_FILENO, str, strlen(str))
	if (0 > WRITE_LEN("{\"version\":1, \"click_events\": true}\n[\n")) {
		fprintf(stderr, "is3-status: unable to send start status bar\n");
		return 1;
	}
#undef WRITE_LEN

	yajl_gen json_gen = yajl_gen_alloc(NULL);
	yajl_gen_array_open(json_gen);
	yajl_gen_clear(json_gen);

	struct pollfd fds[2];

	set_nonblocking(STDIN_FILENO);
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;

	set_nonblocking(g_net_global.netlink_fd);
	fds[1].fd = g_net_global.netlink_fd;
	fds[1].events = POLLIN;

	int ret;

#ifdef PROFILE
	for (int k = 0; k < 1000000; k++) {
		ret = poll(fds, sizeof(fds) / sizeof(fds[0]), 0);
#else
	for (;;) {
		ret = poll(fds, sizeof(fds) / sizeof(fds[0]), 1000);
#endif
		if (ret < 0) {
			fprintf(stderr, "poll failed with %s\n", strerror(errno));
			break;
		}
		if (fds[0].revents & POLLIN)
			handle_click_event(runs, runs_end);
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

		yajl_gen_array_open(json_gen);
		for(struct run_instance *run = runs; run != runs_end; run++) {
			yajl_gen_map_open(json_gen);

			JSON_OUTPUT_KV(json_gen, "name", run->vtable->name);
			JSON_OUTPUT_KV(json_gen, "markup", "none");
			if (run->data->align)
				JSON_OUTPUT_KV(json_gen, "align", run->data->align);
			if (run->instance)
				JSON_OUTPUT_KV(json_gen, "instance", run->instance);
			run->vtable->func_output(run->data, json_gen, true);

			yajl_gen_map_close(json_gen);
		}
		yajl_gen_array_close(json_gen);

		yajl_gen_get_buf(json_gen, (const unsigned char **)&iov[0].iov_base, &iov[0].iov_len);
		if ( 0 > writev(STDOUT_FILENO, iov, 2)) {
			fprintf(stderr, "is3-status: unable to send output, error %s\n", strerror(errno));
		}
		yajl_gen_clear(json_gen);
	}

	yajl_gen_free(json_gen);
	free_all_run_instances(runs, count);


	return 0;
}
