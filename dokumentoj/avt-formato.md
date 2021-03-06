# La formato AVT

## Alfabeto

Ŝajne la esperantaj literoj ne estas kodigitaj laŭ lia alfabeto en la dosiero.

```
kh = 0x92
sh = 0xa5
gh = 0x90
ch = 0x80
jh = jh ?
w  = 0x97

KH = 0x99
SH = 0xa7
GH = 0x91
CH = 0x8e
JH = JH ?
W  = 0x9a
```

Se oni skribas laŭ la alfabeto priskribita en `AVENTURO.DOK`, la kompililo ŝajne aŭtomate konvertas la suprajn literojn sed ial ne konvertas “jh”. Tamen la litero “ĵ” ja aperas en la dosiero de _La Insulo Texel_ kiel 0x96.

## Salonoj

La salonoj komenciĝas ĉe 0x101.

Estas 31 bajtoj por ĉiu salono:

| Bajtoj | Klarigoj |
| ------ | -------- |
| 1   | Longeco de nomo             |
| 20  | Nomo + rubaĵo ĝis 20 bajtoj |
| 2   | Ĉenonombro por la priskribo |
| 1   | Poentoj                     |
| 1   | Norden                      |
| 1   | Orienten                    |
| 1   | Suden                       |
| 1   | Okcidenten                  |
| 1   | Supren                      |
| 1   | Suben                       |
| 1   | Elen                        |

La numeroj de la ĉambroj komenciĝas je 1. Nulo signifas ke oni ne povas iri tiudirekten.

La maksimuma kvanto de salonoj ŝajne estas 149. Se estas malpli da salonoj estas rubaĵo post tio.

Post la lasta salono estas spaco por alia salono kun 0 por la longeco de la nomo por marki la finon. Tial estas 150×31 bajtoj por la salonoj.

## Direktoj

Sekve, ĉe 0x132b estas la direktoj.

Estas 25 bajtoj por ĉiu direkto:

| Bajtoj | Klarigoj |
| ------ | -------- |
| 1   | Longeco de nomo         |
| 20  | Nomo + rubaĵo ĝis 20 bajtoj. La nomo en la fontokodo devas finiĝi per /oj?n?/. La finaĵo ja inkluziĝas en la longeco. |
| 2   | Ĉenonombro por la vido. |
| 1   | Fonta salono.           |
| 1   | Cela salono.            |

La kompililo permesas maksimume 499 direktojn.

## Aĵoj

Ĉe 0x43ff

Estas 62 bajtoj por aĵo.

| Bajtoj | Klarigoj |
| ------ | -------- |
| 1   | Longeco de nomo                                                 |
| 20  | Nomo + rubaĵo. La longeco inkluzivas la “o”.                    |
| 1   | Longeco de adjektivo                                            |
| 20  | Adjektivo + rubaĵo. La longeco inkluzivas la “a”.               |
| 2   | Ĉenonombro por la priskribo.                                    |
| 1   | 0=Viro, 1=Ino, 2=Besto, 3=Pluralo                               |
| 1   | Poentoj                                                         |
| 1   | Pezo                                                            |
| 1   | Grando                                                          |
| 1   | Perpafi                                                         |
| 1   | Ŝargo                                                           |
| 1   | Perbati                                                         |
| 1   | Perpiki                                                         |
| 2   | Ĉenonombro por legi.                                            |
| 1   | Manĝeblo                                                        |
| 1   | Trinkeblo                                                       |
| 1   | Fajrodaŭro                                                      |
| 1   | Fino                                                            |
| 1   | 0=en loko, 0x10=Kunportanta, 0x0f=Nenie, 3=ĉe monstro, 1=en aĵo |
| 1   | Loko                                                            |
| 1   | 0x09=enhava, 0x0c=normala, 0x00=enirebla                        |
| 1   | Enhava kvanto aŭ enira salonnombro                              |

Se oni nomas la aĵon kun “j” la kompililo igas ĝin pluralo. Sed estas `PLURALO` en la kodo ĝi kontrolas ĉu estas “j”.

La numero de la aĵo ŝajne ne aperas en la rezulta dosiero. Eble ĝi simple estas por permesi havi dufoje la saman aĵon kaj la programo distingis inter ili per interna numero. Io stranga okazas al la cetero de la datumoj se estas numero. Ĉu nur la unua havas datumojn?

La kompililo permesas maksimume 149 aĵojn kaj estas spaco por 150 en la dosiero por inkluzivi la finan markilon.

## Sinonimoj

Komenciĝas ĉe 0x6853.

23 bajtoj.

| Bajtoj | Klarigoj |
| ------ | -------- |
| 1   | Nomolongo                              |
| 20  | Nomo + rubaĵo. Nomo inkluzivas la “o”. |
| 1   | 1=aĵo, 3=monstro                       |
| 1   | Aĵo/monstro                            |

Estas spaco por 400 sinonimoj sed la komputilo permesas maksimume 399.

## Monstroj

Komenciĝas ĉe 0x8c43.

57 bajtoj.

| Bajtoj | Klarigoj |
| ------ | -------- |
| 1   | Longeco de nomo                                                 |
| 20  | Nomo + rubaĵo. La longeco inkluzivas la “o”.                    |
| 1   | Longeco de adjektivo                                            |
| 20  | Adjektivo + rubaĵo. La longeco inkluzivas la “a”.               |
| 2   | Ĉenonombro por la priskribo.                                    |
| 1   | 0=Viro, 1=Ino, 2=Besto, 3=Pluralo                               |
| 1   | Mortinta aĵonumero                                              |
| 1   | Manĝemo                                                         |
| 1   | Trinkemo                                                        |
| 2   | Agreso                                                          |
| 1   | Atako                                                           |
| 1   | Protekto                                                        |
| 1   | Vivoj                                                           |
| 1   | Fuĝemo                                                          |
| 1   | Vagemo                                                          |
| 1   | 0=en loko, 0x10=Kunportanta, 0x0f=Nenie, 3=ĉe monstro, 1=en aĵo |
| 1   | Loko                                                            |

Estas spaco por 75 monstroj (73 de la kompililo (??)).

## Fenomenoj

Komenciĝas ĉe 0x9cf6. 20 bajtoj.

| Bajtoj | Klarigoj |
| ------ | -------- |
| 1   | Numero de verbo.                                                |
| 2   | regulo pri loko                                                 |
| 2   | regulo pri aĵo                                                  |
| 2   | regulo pri pero                                                 |
| 2   | regulo pri monstro                                              |
| 2   | ĉenonombro                                                      |
| 2   | nova loko                                                       |
| 2   | nova aĵo                                                        |
| 2   | nova pero                                                       |
| 2   | nova monstro                                                    |
| 1   | poentoj                                                         |

Reguloj:

| Numero | Regulo |
| ------ | ------ |
| 0x00 |   Negravas (ŝajne akceptata nur por loko)                       |
| 0x01 |   Aĵo estas                                                     |
| 0x02 |   Alia aĵo ĉeestas                                              |
| 0x03 |   Monstro estas                                                 |
| 0x04 |   Alia monstro ĉeestas                                          |
| 0x06 |   Ŝargo                                                         |
| 0x07 |   Pezo                                                          |
| 0x08 |   Grando                                                        |
| 0x09 |   Enhavo                                                        |
| 0x0a |   Fajrodaŭro                                                    |
| 0x0b |   Io                                                            |
| 0x0c |   Nenio (defaŭlto por ĉio krom loko)                            |
| 0x0f |   Defaŭlto por loko                                             |
| 0x11 |   Eco de aĵo                                                    |
| 0x12 |   Malvera eco de aĵo                                            |
| 0x13 |   Eco de salono                                                 |
| 0x14 |   Malvera eco de salono                                         |
| 0x15 |   Eco de monstro                                                |
| 0x16 |   Malvera eco de monstro                                        |
| 0x17 |   Eco de ludanto                                                |
| 0x18 |   Malvera eco de ludanto                                        |
| 0x19 |   Ŝanco                                                         |
| 0x1b |   La uzata aĵo havas saman adjektivon                           |
| 0x1c |   La uzata monstro havas saman adjektivon                       |
| 0x1d |   La uzata aĵo havas saman nomon                                |
| 0x1e |   La uzata monstro havas saman nomon                            |
| 0x1f |   La uzata aĵo havas samajn advektivon kaj nomon                |
| 0x20 |   La uzata monstro havas samajn advektivon kaj nomon            |

Agoj:

| Numero | Ago |
| ------ | --- |
| 0x00 |  Alien                                                         |
| 0x01 |  Aĵo anstataŭiĝu                                               |
| 0x02 |  Aĵo estiĝu                                                    |
| 0x03 |  Monstro anstataŭiĝu                                           |
| 0x04 |  Monstro estiĝu                                                |
| 0x05 |  Ŝanĝi finon                                                   |
| 0x06 |  Ŝanĝi ŝargon                                                  |
| 0x07 |  Ŝanĝi pezon                                                   |
| 0x08 |  Ŝanĝi grandon                                                 |
| 0x09 |  Ŝanĝi enhavon                                                 |
| 0x0a |  Ŝanĝi fajrodaŭro                                              |
| 0x0b |  Io (fari nenion)                                              |
| 0x0c |  Nenio (forigi aĵon) (defaŭlta por ĉio krom loko)              |
| 0x0f |  Defaŭlto por loko                                             |
| 0x10 |  Kunportiĝu                                                    |
| 0x11 |  Aĵo ekhavu econ                                               |
| 0x12 |  Aĵo ekmalhavu econ                                            |
| 0x13 |  Salono ekhavu econ                                            |
| 0x14 |  Salono ekmalhavu econ                                         |
| 0x15 |  Monstro ekhavu econ                                           |
| 0x16 |  Monstro ekmalhavu econ                                        |
| 0x17 |  Ludanto ekhavu econ                                           |
| 0x18 |  Ludanto ekmalhavu econ                                        |
| 0x1b |  Ŝanĝu adjektivon de aĵo                                       |
| 0x1c |  Ŝanĝu adjektivon de monstro                                   |
| 0x1d |  Ŝanĝu nomon de aĵo                                            |
| 0x1e |  Ŝanĝu nomon de monstro                                        |
| 0x1f |  Kopiu aĵon                                                    |
| 0x20 |  Kopiu monstron                                                |

La kompililo permesas maksimume 199 fenomenojn.

## Indikiloj al ĉenoj

Ĉe 0xac96 ŝajne estas listo de indikiloj al la ĉenoj. Estas po 4 bajtoj por ĉiu ĉeno. Mi supozas ke ili iel rilatas al la “far pointers” de DOS, sed mi ne scias ekzakte kiel. Mi kredas ke la listo finiĝas per adreso 0, kaj tiel oni povas uzi la liston por kalkuli kiom da ĉenoj estos ĉe la fino. Se oni aldonas pliajn ĉenojn ŝajne nur ĉi tiu tabelo ŝanĝiĝas do mi supozas ke tio estas la sola maniero por scii kiom da ĉenoj estas. Ne eblas simple rigardi la longecon de la dosiero ĉar post la lasta ĉeno estas plia ĉeno por la enkonduko al la ludo.

## Verboj

Ŝajne estas listo de verboj ĉe 0xbc41.

Estas 11 bajtoj:

| Numero | Regulo |
| ------ | ------ |
| 1   | Longeco                                                         |
| 10  | Verbo sen finaĵo + nuloj                                        |

La kompililo permesas maksimume 59 verbojn, sed ankaŭ estas aliaj verboj kiujn ĝi aldonas aŭtomate. Se mi aldonas 59 verbojn fine en la dosiero estas 198.

## Mistero

Ĉe 0xc4ce estas io kio aspektas kiel listo de demandovortoj.

## Ecoj

La ecoj estas unu listo de bitoj po eco. La nomoj de la ecoj ŝajne perdiĝas. Ĉiu bito reprezentas ĉu la koncerna objekto havas la econ. La bito 0 ne uziĝas ĉar la unua objekto estas numero 1.

La ecoj estas apartaj por ĉiu tipo de afero. Ĉiu tipo havas malsaman maksimuman kvanton de ecoj. Salonoj kaj aĵoj jam havas kelkajn ecojn antaŭdifinitajn de la kompililo.

| Tipo | Loko |
| ---- | ---- |
| aĵaj:     | ĉe 0xc51a, 19 bajtoj por ĉiu eco. |
| monstraj: | ĉe 0xc696, 10 bajtoj por ĉiu eco. |
| salonaj:  | ĉe 0xc75e, 19 bajtoj por ĉiu eco. |

Sekve ĉe 0xc8da estas 6 bajtoj por la ludaj ecoj. Unu bito por ĉiu eco. La kompililo permesas 49 ludajn ecojn.

## Informoj

| Adreso | Signifo                          |
| ------ | -------------------------------- |
| 0xc8e1 | Longeco de la nomo de la aventuro |
| 0xc8e2 | 20 bajtoj por la nomo de la aventuro |
| 0xc8f6 | Longeco de la nomo de la aŭtoro  |
| 0xc8f7 | 20 bajtoj por la nomo de la aŭtoro |
| 0xc90b | Longeco de la nomo de la jaro    |
| 0xc90c | 4 bajtoj por la jaro             |
| 0xc910 | 2 bajtoj por la komenca soifo    |
| 0xc912 | 2 bajtoj por la komenca malsato  |
| 0xc914 | 1=Debug, 0=Ne debug              |

## Ĉenoj

Komenciĝas ĉe 0xca80.

Estas 128 bajtoj por ĉiu ĉeno. Ĝi komenciĝas per bajto kiu montras la longecon inkluvize de la “@”.

Se oni skribas tekston pli longan ol 128 bajtoj en la fontokodo, ŝajne la kompililo dividos ĝin en plurajn ĉenojn. Eble se la ĉeno ne finiĝas per “@” la programo simple montros ankaŭ la sekvan ĉenon.

La ĉenonombro en aliaj lokoj ŝajne estas la logika numero de la ĉeno en la listo de ĉenoj, ne nombro de bajtoj de la komenco por facile trovi ĝin. Ekzemple se la unua ĉeno bezonas du arojn de 128 bajtoj, la sekva ĉeno komenciĝos ĉe la 3a aro sed ĝi havos la numeron 2.

Oni povas scii kiom da ĉenoj estas per la listo de indikiloj klarigita supre. Post la lasta ĉeno se ankoraŭ estas plia ĉeno la programo montros ĝin je la komenco kiel enkondukon.
