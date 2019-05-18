#ifndef FDPOLL_H
#define FDPOLL_H

#include <stdbool.h>

struct run_instance;

void fdpoll_init(void);
void fdpoll_add(int fd, void(*func_handle)(void *data), void *data);
bool fdpoll_run(struct run_instance *runs_begin, struct run_instance *runs_end);

#endif // FDPOLL_H
