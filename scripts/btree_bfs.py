#! /usr/bin/env python

# This file is part of is3-status (https://github.com/arthurzam/is3-status).
# Copyright (C) 2019  Arthur Zamarin
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

from math import log2, ceil
from queue import Queue

def sub_tree(a,b): # range [a,b]
    if a == b:
        return (a, None, None)
    if a > b:
        return None

    c = ceil(log2(b-a+2)) # height of tree
    d = 2**c + a - b - 2 # missing to full last row

    if d == 0:
        val = (a + b) // 2
    elif d <= 2**(c-2): 
        val = a + 2**(c-1) - 1# Full left tree with height=c-1
    else:
        val = b - 2**(c-2) + 1# Full right tree with height=c-2

    return (val, sub_tree(a, val - 1), sub_tree(val + 1, b))

def bfs_output(t):
    q = Queue()
    q.put(t)
    while not q.empty():
        v,l,r = q.get()
        print(v)
        if l:
            q.put(l)
        if r:
            q.put(r)

from sys import argv
bfs_output(sub_tree(int(argv[1]), int(argv[2])))
