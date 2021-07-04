#!/usr/bin/python3

import sys
from elkodigu import elkodigu_ĉenon

N_FENOMENOJ = 4
GRANDECO_FENOMENO = 20

REGULOJ = {
    0x00: "negravas",
    0x01: "aĵo estas",
    0x02: "alia aĵo ĉeestas",
    0x03: "monstro estas",
    0x04: "alia monstro ĉeestas",
    0x06: "ŝargo",
    0x07: "pezo",
    0x08: "grando",
    0x09: "enhavo",
    0x0a: "fajrodaŭro",
    0x0b: "io",
    0x0c: "nenio",
    0x0f: "defaŭlto por loko",
    0x11: "eco de aĵo",
    0x12: "malvera eco de aĵo",
    0x13: "eco de salono",
    0x14: "malvera eco de salono",
    0x15: "eco de monstro",
    0x16: "malvera eco de monstro",
    0x17: "eco de ludanto",
    0x18: "malvera eco de ludanto",
    0x19: "ŝanco",
    0x1b: "la uzata aĵo havas saman adjektivon",
    0x1c: "la uzata monstro havas saman adjektivon",
    0x1d: "la uzata aĵo havas saman nomon",
    0x1e: "la uzata monstro havas saman nomon",
    0x1f: "la uzata aĵo havas samajn advektivon kaj nomon",
    0x20: "la uzata monstro havas samajn advektivon kaj nomon",
}

AGOJ = {
    0x00: "alien",
    0x01: "aĵo anstataŭiĝu",
    0x02: "aĵo estiĝu",
    0x03: "monstro anstataŭiĝu",
    0x04: "monstro estiĝu",
    0x05: "ŝanĝi finon",
    0x06: "ŝanĝi ŝargon",
    0x07: "ŝanĝi pezon",
    0x08: "ŝanĝi grandon",
    0x09: "ŝanĝi enhavon",
    0x0a: "ŝanĝi fajrodaŭro",
    0x0c: "nenio",
    0x0f: "defaŭlto por loko",
    0x10: "kunportiĝu",
    0x11: "aĵo ekhavu econ",
    0x12: "aĵo ekmalhavu econ",
    0x13: "salono ekhavu econ",
    0x14: "salono ekmalhavu econ",
    0x15: "monstro ekhavu econ",
    0x16: "monstro ekmalhavu econ",
    0x17: "ludanto ekhavu econ",
    0x18: "ludanto ekmalhavu econ",
    0x1b: "ŝanĝu adjektivon de aĵo",
    0x1c: "ŝanĝu adjektivon de monstro",
    0x1d: "ŝanĝu nomon de aĵo",
    0x1e: "ŝanĝu nomon de monstro",
    0x1f: "kopiu aĵon",
    0x20: "kopiu monstron",
}

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
    try:
        tiponomo = REGULOJ[tipo]
    except KeyError:
        tiponomo = "??(0x{:02x})".format(tipo)

    return "{} {:02x}".format(tiponomo, datumo)

def elkodigu_novaĵon(tipo, datumo):
    try:
        tiponomo = AGOJ[tipo]
    except KeyError:
        tiponomo = "??(0x{:02x})".format(tipo)

    return "{} {:02x}".format(tiponomo, datumo)

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
