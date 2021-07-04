#!/usr/bin/python3

import sys
from elkodigu import elkodigu_ĉenon

N_SINONIMOJ = 18
GRANDECO_SINONIMO = 23

TIPOJ = {
    1: "aĵo",
    3: "monstro",
}

def elkodigu_sinonimon(sinonimo):
    p = 0
    nomlongo = sinonimo[p]
    p += 1
    nomo = elkodigu_ĉenon(sinonimo[p : p + nomlongo])
    p += 20
    tipo = sinonimo[p]
    p += 1
    try:
        tipo = TIPOJ[tipo]
    except KeyError:
        raise Exception("Nekonata tipo {} por sinonimo {}".format(tipo, nomo))
    elemento = sinonimo[p]
    p += 1

    rest = ' '.join('{:02x}'.format(x) for x in sinonimo[p : GRANDECO_SINONIMO])

    print("Nomo: {} {}, elemento: {}".
          format(nomo,
                 tipo,
                 elemento))

for fn in sys.argv[1:]:
    with open(fn, 'rb') as f:
        f.seek(0x6853)
        for i in range(N_SINONIMOJ):
            sinonimo = f.read(GRANDECO_SINONIMO)
            elkodigu_sinonimon(sinonimo)
