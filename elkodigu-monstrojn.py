#!/usr/bin/python3

import sys
from elkodigu import elkodigu_ĉenon

N_MONSTROJ = 4
GRANDECO_MONSTRO = 57

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

PARTOJ = [
    "t:tipo",
    "b:mortinta",
    "b:manĝemo",
    "b:trinkemo",
    "w:agreso",
    "b:atako",
    "b:protekto",
    "b:vivoj",
    "b:fuĝemo",
    "b:vagemo",
    "l:lokotipo",
    "b:loko",
]

def elkodigu_monstron(monstro):
    p = 0
    nomlongo = monstro[p]
    p += 1
    nomo = elkodigu_ĉenon(monstro[p : p + nomlongo])
    p += 20
    adjektivolongo = monstro[p]
    p += 1
    adjektivo = elkodigu_ĉenon(monstro[p : p + adjektivolongo])
    p += 20
    ĉenonomo = monstro[p] | (monstro[p + 1] << 8)
    p += 2

    print("Nomo: {} {}, ĉeno: {}".format(adjektivo, nomo, ĉenonomo))

    for parto in PARTOJ:
        partotipo = parto[0]
        partonomo = parto[2:]

        if partotipo == 'b':
            val = monstro[p]
            p += 1
        elif partotipo == 'w':
            val = monstro[p] | (monstro[p + 1] << 8)
            p += 2
        elif partotipo == 't':
            val = TIPOJ[monstro[p]]
            p += 1
        elif partotipo == 'l':
            val = LOKOTIPOJ[monstro[p]]
            p += 1
        else:
            raise Exception("Nekonata partotipo {}".format(partotipo))

        print(" {}={}".format(partonomo, val))

    rest = ' '.join('{:02x}'.format(x) for x in monstro[p : GRANDECO_MONSTRO])

    print(" rest: {}".format(rest))
    
for fn in sys.argv[1:]:
    with open(fn, 'rb') as f:
        f.seek(0x8c43)
        while True:
            monstro = f.read(GRANDECO_MONSTRO)
            if monstro[0] == 0:
                break
            elkodigu_monstron(monstro)
