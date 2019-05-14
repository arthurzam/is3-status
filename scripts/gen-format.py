#! /usr/bin/env python

from sys import argv

res = [0] * 8
for arg in argv[1:]:
    for char in arg:
        val = ord(char)
        res[val >> 5] |= (1 << (val & 31))
print('// generaterd using command', ' '.join(argv))
print('{' + ', '.join(map('0x{0:08X}'.format, res)) + '}')
