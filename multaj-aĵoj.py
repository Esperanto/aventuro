#!/usr/bin/python3

import sys

N_AĴOJ = 149

print("NOMO MultajSalonoj 2021 Sr_Salono;\r\n"
      "SALONO s0; Salono 0 @\r\n"
      "LUMA;\r")

for i in range(N_AĴOJ):
    print(("AJHO a{}a a{}o; ajho {} @\r\n"
           "FINO_AJHO;\r").format(i, i, i))

print("FINO_SALONO;\r")

print("FINO_PROGRAMO;;\r")
