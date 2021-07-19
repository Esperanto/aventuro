#!/usr/bin/python3

import sys
from elkodigu import elkodigu_ĉenon

GRANDECO_VERBO = 11

for fn in sys.argv[1:]:
    with open(fn, 'rb') as f:
        nombro = 1

        f.seek(0xbc41)

        while True:
            verbo = f.read(GRANDECO_VERBO)
            longo = verbo[0]

            if longo == 0:
                break

            print("{:3d} {}".format(
                nombro,
                elkodigu_ĉenon(verbo[1 : 1 + longo])))

            nombro += 1
