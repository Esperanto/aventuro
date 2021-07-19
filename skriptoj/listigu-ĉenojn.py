#!/usr/bin/python3

import sys
from elkodigu import elkodigu_ĉenon

GRANDECO_ĈENO = 128

for fn in sys.argv[1:]:
    with open(fn, 'rb') as f:
        nombro = 1

        f.seek(0xca80)

        while True:
            ĉeno = f.read(GRANDECO_ĈENO)

            if len(ĉeno) < 1:
                break

            longo = ĉeno[0]

            print("{:3d} {}".format(
                nombro,
                elkodigu_ĉenon(ĉeno[1 : 1 + longo])))

            nombro += 1
