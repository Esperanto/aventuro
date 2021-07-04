#!/usr/bin/python3

import sys
from elkodigu import elkodigu_ĉenon

N_FENOMENOJ = 4
GRANDECO_FENOMENO = 20

PARTOJ = [
    "v:verbo",
    "r:loko",
    "r:aĵo",
    "r:pero",
    "r:monstro",
    "w:ĉeno",
    "n:loko",
    "n:aĵo",
    "n:pero",
    "n:monstro",
    "b:poentoj",
]

def legu_verbojn(f):
    f.seek(0xbc41)

    while True:
        verbo = f.read(11)
        longo = verbo[0]

        if longo == 0:
            break

        yield(elkodigu_ĉenon(verbo[1 : 1 + longo]))

def elkodigu_regulon(tipo, datumo):
    return "{:02x} {:02x}".format(tipo, datumo)

def elkodigu_novaĵon(tipo, datumo):
    return "{:02x} {:02x}".format(tipo, datumo)

def elkodigu_fenomenon(verboj, fenomeno):
    p = 0

    for parto in PARTOJ:
        partotipo = parto[0]
        partonomo = parto[2:]

        if partotipo == 'b':
            val = fenomeno[p]
            p += 1
        elif partotipo == 'w':
            val = fenomeno[p] | (fenomeno[p + 1] << 8)
            p += 2
        elif partotipo == 'r':
            val = elkodigu_regulon(fenomeno[p], fenomeno[p + 1])
            p += 2
        elif partotipo == 'n':
            val = elkodigu_novaĵon(fenomeno[p], fenomeno[p + 1])
            p += 2
        elif partotipo == 'v':
            val = "{}({})".format(fenomeno[p], verboj[fenomeno[p] - 1])
            p += 1
        else:
            raise Exception("Nekonata partotipo {}".format(partotipo))

        print(" {}={}".format(partonomo, val))

for fn in sys.argv[1:]:
    with open(fn, 'rb') as f:
        verboj = list(legu_verbojn(f))

        f.seek(0x9cf6)
        while True:
            fenomeno = f.read(GRANDECO_FENOMENO)
            if fenomeno[0] == 0:
                break
            elkodigu_fenomenon(verboj, fenomeno)
            print()
