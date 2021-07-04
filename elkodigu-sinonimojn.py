#!/usr/bin/python3

import sys

N_SINONIMOJ = 18
GRANDECO_SINONIMO = 23

ĈENO_KODOJ = {
    0x92: "ĥ",
    0xa5: "ŝ",
    0x90: "ĝ",
    0x80: "ĉ",
    0x97: "ŭ",
    0xa7: "ŝ",
    0x91: "ĝ",
    0x9a: "ŭ",
}

TIPOJ = {
    1: "aĵo",
    3: "monstro",
}

def elkodigu_ĉenobajton(bajto):
    try:
        return ĈENO_KODOJ[bajto]
    except KeyError:
        return chr(bajto)

def elkodigu_ĉenon(bajtoj):
    return "".join(map(elkodigu_ĉenobajton, bajtoj))

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
