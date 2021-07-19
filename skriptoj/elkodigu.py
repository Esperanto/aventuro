#!/usr/bin/python3

import sys

ĈENO_KODOJ = {
    0x92: "ĥ",
    0xa5: "ŝ",
    0x90: "ĝ",
    0x80: "ĉ",
    0x96: "ĵ",
    0x97: "ŭ",
    0x99: "Ĥ",
    0xa7: "Ŝ",
    0x91: "Ĝ",
    0x8e: "Ĉ",
    0x9a: "Ŭ",
}

def elkodigu_ĉenobajton(bajto):
    try:
        return ĈENO_KODOJ[bajto]
    except KeyError:
        return chr(bajto)

def elkodigu_ĉenon(bajtoj):
    return "".join(map(elkodigu_ĉenobajton, bajtoj))
