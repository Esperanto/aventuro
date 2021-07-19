#!/usr/bin/python3

import sys

N_SALONOJ = 149
N_DIREKTOJ = 499


def kreu_direktojn():
    for d in range(N_DIREKTOJ):
        cela_salono = d % (N_SALONOJ - 1) + 1
        direktonomo = ("d" +
                       chr(ord('a') + d // 26) +
                       chr(ord('a') + d % 26) +
                       "o")
        print("DIREKTO {} s{}; Vi vidas direkton {} @\r".format(
            direktonomo,
            cela_salono,
            direktonomo))


print("NOMO MultajSalonoj 2021 Sr_Salono;\r")

for i in range(N_SALONOJ):
    print(("SALONO s{}; Salono {} @\r\n"
           "LUMA;\r").format(i, i))

    if i == 0:
        kreu_direktojn()

    print("FINO_SALONO;\r")

print("FINO_PROGRAMO;;\r")
