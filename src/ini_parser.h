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
#define FOREACH_RUN(iter,runs) for(struct run_instance *(iter) = (runs)->runs_begin; (iter) != (runs)->runs_end; (iter)++)

struct runs_list ini_parse(FILE *ini);
void free_all_run_instances(struct runs_list *runs);

#ifdef TESTS
int test_cmd_array_correct(void);
#endif

#endif // INI_PARSER_H
