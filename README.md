# Aventuro

Ĉi tiu projekto celas rekrei malnovan sistemon por tekstaventuroj kiu nomiĝas [Aventuro](https://eo.wikipedia.org/wiki/Aventuro_%28tekstaventuro%29). La originala programo estas elŝutebla [ĉi tie](http://ifarchive.org/if-archive/interpreters-other/aventuro/). Oni verkis ĝin en 1987, kaj tial ĝi estas programo por DOS. Ĝi tamen ankoraŭ funkcias facile nun en [DOSBox](https://www.dosbox.com/).

Se vi simple volas ludi tekstaventuron, vi povas ludi ĉe [Gemelo.org](https://gemelo.org/kongreso).

Parto de la tasko estas kompreni la originalan sistemon. La originala sistemo inkluzivas kompililon por krei proprajn ludojn. La programlingvo estas dokumentita jam en la originala programo. Ĉi tiu deponejo ankaŭ havas [dokumenton](dokumentoj/avt-formato.md) por provi kompreni la duuman formaton de la dosieroj kiujn la kompililo kreas. Estas pli facile kompreni la formaton de la duumaj dosieroj per la du dokumentoj kune.

## Interpretilo

La projekto ankaŭ havas novan realigon de la interpretilo. Oni povas kompili ĝin ĉi tiel:

    mkdir build && cd build
    meson ..
    ninja

Sekve, se oni elŝutas la originalan zip-dosieron de Aventuro, oni povas ludi la ludon _La Insulo Texel_ ĉi tiel:

    ./play-avt TEXEL.AVT

La interpretilo ankoraŭ ne estas tute finita do ne eblas plene ludi la ludon.

## Nova datumlingvo

Krom povi ludi la aventurojn de la originala sistemo, ĉi tiu interpretilo ankaŭ havas novan datumlingvon por krei ludojn. Ĝi povas rekte legi la datumlingvon kaj ne necesas programtradukilo por konverti ĝin al duuma dosiero. La lingvo ankoraŭ ne havas dokumentojn, sed oni povas vidi [ekzemplan ludon](ludoj/kongreso1.avt) en la deponejo.

## Retpaĝo

La interpretilo povas funkcii ankaŭ kiel retpaĝo. Por ebligi tion, oni devas unue kompili ĝin per emscripten. Por instali emscripten, fari la jenon:

    git clone https://github.com/emscripten-core/emsdk.git
    cd emsdk
    ./emsdk install latest
    ./emsdk activate latest
    source ./emsdk_env.sh

Sekve vi povas kompili Aventuron jene:

    cd aventuro
    meson --cross-file=emscripten-cross.txt ebuild -Dbuildtype=release \
          -Dprefix=~/aventuro-install
    ninja -C ebuild install
    
Post tio vi havos la necesajn dosierojn por la retpaĝo en `~/aventuro-install/share/web`. Mankas nur la AVT-dosiero. Vi povus kopii la originalan ludon jene:

    cp ~/dosgames/aventuro/TEXEL.AVT ~/aventuro-install/share/web/ludo.avt

Sevke vi povas ruli retpaĝan servilon ĉe tiu dosierujo kaj ludi la ludon. Ne eblas ludi ĝin per URL kiu komenciĝas per `file:///` ĉar retumiloj ne ŝatas permesi WebAssembly por lokaj dosieroj. Se vi havas twistd instalitan vi povus ekzemple fari:

    twistd -no web --path ~/aventuro-install/share/web

Tiam vi povas direkti vian retumilon al [http://localhost:8080/](http://localhost:8080/) por ludi.
