#!/usr/bin/python3

import sys
from elkodigu import elkodigu_ĉenon

GRANDECO_SALONO = 31

def elkodigu_salonon(salono):
    p = 0
    nomlongo = salono[p]
    p += 1
    nomo = elkodigu_ĉenon(salono[p : p + nomlongo])
    p += 20
    ĉeno = salono[p] | (salono[p + 1] << 8)
    p += 2

    direktoj = []
    for d in ["?", "n", "or", "s", "ok", "supren", "suben", "elen"]:
        direktoj.append("{}: {:3d}".format(d, salono[p]))
        p += 1

    return "nomo: {:20s}, ĉeno: {:3d}, {}".format(
        nomo,
        ĉeno,
        ", ".join(direktoj))

for fn in sys.argv[1:]:
    with open(fn, 'rb') as f:
        f.seek(0x101)
        num = 1
        while True:
            salono = f.read(GRANDECO_SALONO)
            if salono[0] == 0:
                break
            print("{:3d}: {}".format(num, elkodigu_salonon(salono)))
            num += 1
