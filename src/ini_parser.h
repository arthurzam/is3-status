#ifndef INI_PARSER_H
#define INI_PARSER_H

#include <stdio.h>

struct cmd;
struct cmd_data_base;

struct run_instance {
	const struct cmd *vtable;
	struct cmd_data_base *data;
	char *instance;
};

struct run_instance *ini_parse(FILE *ini, unsigned *res_size);
void free_all_run_instances(struct run_instance *runs, unsigned size);

#endif // INI_PARSER_H
