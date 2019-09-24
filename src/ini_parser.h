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

#ifndef INI_PARSER_H
#define INI_PARSER_H

struct cmd;
struct cmd_data_base;

struct run_instance {
	const struct cmd *vtable;
	struct cmd_data_base *data;
	char *instance;
#define MAX_INSTANCE_LEN 64
};

struct runs_list {
	struct run_instance *runs_begin;
	struct run_instance *runs_end;
};
#define FOREACH_RUN(iter,runs) for (struct run_instance *(iter) = (runs)->runs_begin; (iter) != (runs)->runs_end; (iter)++)

struct runs_list ini_parse(const char *argv_path) __attribute__ ((cold));
void free_all_run_instances(struct runs_list *runs);

#ifdef TESTS
int test_cmd_array_correct(void);
#endif

#endif // INI_PARSER_H
