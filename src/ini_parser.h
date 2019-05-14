#ifndef INI_PARSER_H
#define INI_PARSER_H

#include "general.h"

#include <stdio.h>

struct run_instance {
	const struct cmd *vtable;
	struct cmd_data_base *data;
	char *instance;
};

extern struct general_settings_t {
	const char *align;
	long interval;
	char color_bad[8];
	char color_degraded[8];
	char color_good[8];
} g_general_settings;

struct run_instance *ini_parse(FILE *ini, unsigned *res_size);
void free_all_run_instances(struct run_instance *runs, unsigned size);

#endif // INI_PARSER_H
