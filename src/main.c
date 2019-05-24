#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>

#include <yajl_version.h>

#include "main.h"
#include "ini_parser.h"
#include "fdpoll.h"

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
	if (g_general_settings.interval == 0)
		g_general_settings.interval = 1;
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
		if (!run->vtable->func_init(run->data)) {
			fprintf(stderr, "is3-status: init for %s:%s failed\n", run->vtable->name, run->instance);
			return 1;
		}
		if (!run->data->align)
			run->data->align = g_general_settings.align;
		if (run->data->interval < g_general_settings.interval)
			run->data->interval = g_general_settings.interval;
	}

#define WRITE_LEN(str) write(STDOUT_FILENO, str, strlen(str))
	if (0 > WRITE_LEN("{\"version\":1, \"click_events\": true}\n[\n")) {
		fprintf(stderr, "is3-status: unable to send start status bar\n");
		return 1;
	}
#undef WRITE_LEN

	yajl_gen json_gen = yajl_gen_alloc(NULL);
	yajl_gen_array_open(json_gen);
	yajl_gen_clear(json_gen);

	fdpoll_init();
	struct iovec iov[2] = {	{NULL, 0}, {"\n", 1} };

	for (unsigned eventNum = 0; fdpoll_run(runs, runs_end); ++eventNum) {
		yajl_gen_array_open(json_gen);
		for(struct run_instance *run = runs; run != runs_end; run++) {
			yajl_gen_map_open(json_gen);

			JSON_OUTPUT_KV(json_gen, "name", run->vtable->name);
			JSON_OUTPUT_KV(json_gen, "markup", "none");
			if (run->data->align)
				JSON_OUTPUT_KV(json_gen, "align", run->data->align);
			if (run->instance)
				JSON_OUTPUT_KV(json_gen, "instance", run->instance);
			run->vtable->func_output(run->data, json_gen, (eventNum % run->data->interval == 0));

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
