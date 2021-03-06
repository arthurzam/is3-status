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

#ifndef FDPOLL_H
#define FDPOLL_H

#include <stdbool.h>

/**
 * @brief fdpoll_add add watch for @arg fd and call the callback function
 *
 * @param fd file descriptor to watch for POLLIN events
 * @param func_handle the callback function
 * @param data arg to pass for callback function
 */
void fdpoll_add(int fd, bool(*func_handle)(void *data), void *data);
int fdpoll_run(void);

#endif // FDPOLL_H
