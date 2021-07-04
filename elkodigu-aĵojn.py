#!/usr/bin/python3

import sys

N_AĴOJ = 57
GRANDECO_AĴO = 62

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
    0: "viro",
    1: "ino",
    2: "besto",
    3: "pluralo",
}

LOKOTIPOJ = {
    0: "en loko",
    0x10: "kunportanta",
    0x0f: "nenie",
    3: "ĉe monstro",
    1: "en ajho",
}

ENECTIPOJ = {
    0: "enirebla",
    0x09: "enhava",
    0x0c: "normala eneco",
}

def elkodigu_ĉenobajton(bajto):
    try:
        return ĈENO_KODOJ[bajto]
    except KeyError:
        return chr(bajto)

def elkodigu_ĉenon(bajtoj):
    return "".join(map(elkodigu_ĉenobajton, bajtoj))

def elkodigu_aĵon(aĵo):
    p = 0
    nomlongo = aĵo[p]
    p += 1
    nomo = elkodigu_ĉenon(aĵo[p : p + nomlongo])
    p += 20
    adjektivolongo = aĵo[p]
    p += 1
    adjektivo = elkodigu_ĉenon(aĵo[p : p + adjektivolongo])
    p += 20
    ĉenonomo = aĵo[p] | (aĵo[p + 1] << 8)
    p += 2
    tipo = TIPOJ[aĵo[p]]
    p += 1
    poentoj = aĵo[p]
    p += 1
    pezo = aĵo[p]
    p += 1
    grando = aĵo[p]
    p += 1
    perpafi = aĵo[p]
    p += 1
    ŝargo = aĵo[p]
    p += 1
    perbati = aĵo[p]
    p += 1
    perpiki = aĵo[p]
    p += 1
    legebla = aĵo[p] | (aĵo[p + 1] << 8)
    p += 2
    mangheblo = aĵo[p]
    p += 1
    trinkeblo = aĵo[p]
    p += 1
    fajrodaŭro = aĵo[p]
    p += 1
    fino = aĵo[p]
    p += 1
    lokotipo = aĵo[p]
    try:
        lokotipo = LOKOTIPOJ[lokotipo]
    except KeyError:
        raise Exception("Nekonata lokotipo {} "
                        "por {} {}".format(lokotipo, adjektivo, nomo))
    p += 1
    loko = aĵo[p]
    p += 1
    enectipo = ENECTIPOJ[aĵo[p]]
    p += 1
    eneco = aĵo[p]
    p += 1

    rest = ' '.join('{:02x}'.format(x) for x in aĵo[p : GRANDECO_AĴO])

    print(("Nomo: {} {}, ĉeno: {}, {}, "
           "poentoj: {}, pezo: {}, grando: {}, "
           "perpafi: {}, ŝargo: {}, perbati: {}, perpiki: {}, legebla: {}, "
           "mangheblo: {}, trinkeblo: {}, farjodaŭro: {}, fino: {}, "
           "{}, loko: {}, {}, eneco: {}, "
           "rest: {}").
          format(adjektivo,
                 nomo,
                 ĉenonomo,
                 tipo,
                 poentoj,
                 pezo,
                 grando,
                 perpafi,
                 ŝargo,
                 perbati,
                 perpiki,
                 legebla,
                 mangheblo,
                 trinkeblo,
                 fajrodaŭro,
                 fino,
                 lokotipo,
                 loko,
                 enectipo,
                 eneco,
                 rest))

for fn in sys.argv[1:]:
    with open(fn, 'rb') as f:
        f.seek(0x43ff)
        while True:
            aĵo = f.read(GRANDECO_AĴO)
            if aĵo[0] == 0:
                break
            elkodigu_aĵon(aĵo)
