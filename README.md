# Aventuro

Ĉi tiu projekto celas rekrei malnovan sistemon por tekstaventuroj kiu nomiĝas [Aventuro](https://eo.wikipedia.org/wiki/Aventuro_%28tekstaventuro%29). La originala programo estas elŝutebla [ĉi tie](http://ifarchive.org/if-archive/interpreters-other/aventuro/). Oni verkis ĝin en 1987, kaj tial ĝi estas programo por DOS. Ĝi tamen ankoraŭ funkcias facile nun en [DOSBox](https://www.dosbox.com/).

Parto de la tasko estas kompreni la originalan sistemon. La originala sistemo inkluzivas kompililon por krei proprajn ludojn. La programlingvo estas dokumentita jam en la originala programo. Ĉi tiu deponejo ankaŭ havas [dokumenton](dokumentoj/avt-formato.md) por provi kompreni la duuman formaton de la dosieroj kiujn la kompililo kreas. Estas pli facile kompreni la formaton de la duumaj dosieroj per la du dokumentoj kune.

## Interpretilo

La projekto ankaŭ havas novan realigon de la interpretilo. Oni povas kompili ĝin ĉi tiel:

    mkdir build && cd build
    meson ..
    ninja

Sekve, se oni elŝutas la originalan zip-dosieron de Aventuro, oni povas ludi la ludon _La Insulo Texel_ ĉi tiel:

    ./play-avt TEXEL.AVT

La interpretilo ankoraŭ ne estas tute finita do ne eblas plene ludi la ludon.

La plano por la fina rezulto de la interpretilo estas traduki la bibliotekon al JavaScript per emscripten kaj krei modernan interfacon en retpaĝo por facile ludi ĝin.

Sekve estus bone prilabori la formaton de la duumaj dosieroj por aldoni novajn kapablojn por krei pli diversajn ludojn.
