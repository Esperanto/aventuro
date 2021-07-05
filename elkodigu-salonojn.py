#!/usr/bin/python3

import sys

GRANDECO_SALONO = 31

def elkodigu_salonon(salono):
    p = 0
    nomlongo = salono[p]
    p += 1
    nomo = salono[p : p + nomlongo].decode('utf-8')
    p += 20
    ĉeno = salono[p]
    p += 1

    rest = ' '.join('{:02x}'.format(x) for x in salono[p : GRANDECO_SALONO])

    print("Nomo: {}, ĉeno: {}, rest: {}".format(nomo, ĉeno, rest))

for fn in sys.argv[1:]:
    with open(fn, 'rb') as f:
        f.seek(0x101)
        for i in range(N_SALONOJ):
            salono = f.read(GRANDECO_SALONO)
            elkodigu_salonon(salono)
