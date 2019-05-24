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

struct runs_list {
	struct run_instance *runs_begin;
	struct run_instance *runs_end;
};

struct runs_list ini_parse(FILE *ini);
void free_all_run_instances(struct runs_list *runs);

#endif // INI_PARSER_H
