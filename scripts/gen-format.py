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

from sys import argv

res = [0] * 8
for arg in argv[1:]:
    for char in arg:
        val = ord(char)
        res[val >> 5] |= (1 << (val & 31))
print('// generaterd using command', ' '.join(argv))
print('{' + ', '.join(map('0x{0:08X}'.format, res)) + '}')
