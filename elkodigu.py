#!/usr/bin/python3

import sys

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

def elkodigu_ĉenobajton(bajto):
    try:
        return ĈENO_KODOJ[bajto]
    except KeyError:
        return chr(bajto)

def elkodigu_ĉenon(bajtoj):
    return "".join(map(elkodigu_ĉenobajton, bajtoj))
