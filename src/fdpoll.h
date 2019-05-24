#ifndef FDPOLL_H
#define FDPOLL_H

#include <stdbool.h>

struct runs_list;

void fdpoll_init(void);
void fdpoll_add(int fd, void(*func_handle)(void *data), void *data);
bool fdpoll_run(struct runs_list *runs);

#endif // FDPOLL_H
