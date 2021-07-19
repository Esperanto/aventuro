#!/usr/bin/python3

import sys
import os

class Purigilo:
    def __init__(self, inf, outf):
        self._inf = inf
        self._outf = outf
        self._pos = 0

    def kopiu_blokon(self, ĝis_loko):
        grandeco = ĝis_loko - self._pos
        self._outf.write(self._inf.read(grandeco))
        self._pos = ĝis_loko

    def kopiu_nulfinan_aron(self, maksimumaj_eroj, erograndeco):
        n_eroj = 0

        while True:
            ero = self._inf.read(erograndeco)
            if ero[0] == 0:
                break
            self._outf.write(ero)

            n_eroj += 1

        self._outf.write(b'\0' * ((maksimumaj_eroj - n_eroj) * erograndeco))
        self._inf.seek((maksimumaj_eroj - n_eroj - 1) * erograndeco, 1)
        self._pos += maksimumaj_eroj * erograndeco


def filtru_dosieron(inf, outf):
    purigilo = Purigilo(inf, outf)

    purigilo.kopiu_blokon(0x101)
    # Salonoj
    purigilo.kopiu_nulfinan_aron(150, 31)
    # Direktoj
    purigilo.kopiu_nulfinan_aron(500, 25)
    # Aĵoj
    purigilo.kopiu_nulfinan_aron(150, 62)
    # Sinonimoj
    purigilo.kopiu_nulfinan_aron(400, 23)
    # Monstroj
    purigilo.kopiu_nulfinan_aron(75, 57)
    # Fenomenoj
    purigilo.kopiu_nulfinan_aron(200, 20)
    # Indikiloj
    purigilo.kopiu_blokon(0xbc41)
    # Verboj
    purigilo.kopiu_nulfinan_aron(199, 11)
    # Mistero
    purigilo.kopiu_blokon(0xc51a)
    # Ecoj
    purigilo.kopiu_blokon(0xc8e0)
    # Informoj
    purigilo.kopiu_blokon(0xca80)
    # Ĉenoj
    while True:
        parto = inf.read(128)
        if len(parto) == 0:
            break
        longo = parto[0]
        parto = parto[0 : longo + 1] + b'\0' * (128 - longo - 1)
        outf.write(parto)

            
for dosiero in sys.argv[1:]:
    tmp_dosiero = dosiero + ".tmp"

    with open(dosiero, "rb") as inf:
        with open(tmp_dosiero, "wb") as outf:
            filtru_dosieron(inf, outf)

    os.rename(tmp_dosiero, dosiero)
