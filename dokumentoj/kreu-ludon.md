# Kiel krei ludon

Por krei propran ludon per la sistemo de Aventuro, vi povas uzi la [redaktilon](https://gemelo.org/redaktilo) rekte en via retumilo. Ĉe tiu paĝo, vi vidos du flankojn. Per la maldekstra flanko vi povas tajpi la programkodon de via ludo, kaj en la dekstra flanko vi povas testi ĝin. Se vi alklakas la butonon ▶ super la redaktilo la retpaĝo legos vian programon kaj rulos ĝin en la dekstra flanko.

Dum vi legas la jenan dokumenton, povas esti utile rigardi la fontokodon de la kompleta ludo [_La Kongreso_](https://github.com/Esperanto/aventuro/blob/main/ludoj/kongreso1.avt) por vidi kiel la aferoj funkcias en vera ludo.

## Ejoj

La programlingvo de Aventuro ne estas kiel plena programlingvo kiel Pitono aŭ C. Anstataŭe ĝi nur priskribas la aferojn en la ludo kaj la interagojn inter ili. La ĉefa afero en tekstaventuro estas la ejoj. Ejo povas esti ia ajn loko al kiu la ludanto povas iri, do ekzemple ĝi povas esti en konstruaĵo kiel ĉambro en domo aŭ ĝi povas esti ekstere kiel unu flanko de kampo. Kutime oni ligas la ejojn per kompasdirektoj kiel «norde», «sude» ktp, por ke la ludantoj povu libere moviĝi inter ili. Do jen, ni kreu nian unuan ejon. Kopiu la jenan programon al la redaktilo kaj alklaku la butonon ▶ por ludi ĝin:

    ejo salono {
      priskribo "Vi estas en via salono."
      luma
    }

La unua vorto `ejo` indikas al la retpaĝo ke ni priskribos ejon. Poste ni vidos ke ankaŭ eblas priskribi aliajn aferojn de la ludo. La vorto `salono` post tio estas la nomo kiun ni elektis por ĉi tiu ejo. Kiam vi ludas la ludon vi vidos tiun nomon ĉe la supro de la paĝo. Sekve estas la `priskribo`. Post la vorto priskribo vi povas verki vian tekston inter `"citiloj"`. Tiu teksto aperos kiam ajn ludanto eniras la ejon. La sekva vorto `luma` indikas ke la ejo havas propran lumon kaj la ludanto ne bezonas lumigilon por vidi la priskribon. Vi povas forigi tiun vorton por vidi kio okazas se ejo ne havas lumon.

La teksto de la priskribo povas esti tiel longa kiel vi volas. La programtradukilo ignoros troajn spacetojn en la teksto, do vi povas libere aldoni ilin por faciligi la tajpadon en la programkodo. Se vi volas havi plurajn alineojn, vi povas meti blankan linion por marki tion. Ekzemple, pli longa priskribo povus aspekti jene:

    ejo salono {
      priskribo "
    Vi estas en via salono. Vi ne plu memoras kial vi venis ĉi tien. Ĉu vi
    serĉis ion? Nuntempe ofte okazas al vi ke vi forgesas aferojn.

    Tio memorigas vin pri la okazo kiam vi iris al la laboro kaj forgesis vian
    ŝlosilojn. Ktp.
    "
      luma
    }

### Direktoj

Bone, nia ludo ankoraŭ ne estas tre interesa, do ni aldonu pliajn ejojn por ke la ludanto povu esplori. Ni povas konekti ilin per vortoj kiel `norden`, `suden`, ktp. Ekzemple:

    ejo salono {
      priskribo "Vi estas en via salono."
      norden kuirejo
      orienten koridoro
      luma
    }

    ejo koridoro {
      priskribo "Vi estas en la koridoro de via domo."
      okcidenten salono
      luma
    }

    ejo kuirejo {
      priskribo "Vi estas en la kuirejo. Ĝi estas tre malpura."
      suden salono
      luma
    }

Nun la ludando povas tajpi `norden` kiam ri estas en la salono por iri al la kuirejo. Kutime se la ludanto povas iri en unu direkton por iri al alia loko, vi volos ke ri povu tajpi la malan direkton por reveni. Tamen vi rajtas forlasi la revenan direkton ekzemple se vi volas krei komplikan labirinton aŭ se vi volas ke kiam vi iras en kavernon ŝtono falas kaj ne plu eblas eliri.

La direktoj kiujn oni povas uzi estas:

* `norden`
* `orienten`
* `suden`
* `okcidenten`
* `supren`
* `suben`
* `elen`

La direkton `elen` la ludanto aliros se ri tajpas `eliri` aŭ `mi iras elen`, ekzemple. Rimarku ke ne estas direkto `enen`. Poste ni vidos kiel oni povas eniri aĵon por uzi tion.

### Nomoj

La nomo post la vorto `ejo` estas la nomo kiun oni uzas ene de la programkodo por referenci la ejon kaj krei la ligojn al aliaj ejoj. Pro tio ĝi devas esti unika por ke la tradukilo sciu kiun ejon vi indikas. Defaŭlte ĝi ankaŭ estas la nomo kiu aperos en la ludo ĉe la supro de la paĝo. Tamen vi ankaŭ povas meti alian nomon por la ejo kiu estas nur en la supro de la paĝo. Tiu nomo ne bezonas esti unika. Tio estas utila se vi volas havi du lokojn kiuj aspektas samaj al la ludantoj, sed ene de la programo ili estas malsamaj. Ekzemple:

    ejo salono1 {
      luma
      nomo "salono"
      priskribo "Vi estas en via salono. Norde vi vidas strangan bluan pordon."
      norden bluejo
    }

    ejo bluejo {
      luma
      priskribo "Vi estas en stranga blua ejo. Ĉio aspektas blua. La blueco
                 eniras vian korpon kaj donas al via magian povon."
      suden salono2
    }

    ejo salono2 {
      luma
      nomo "salono"
      priskribo salono1
      norden bluejo
      supren kosmo
    }

    ejo kosmo {
      luma
      priskribo "Vi uzas vian bluecon por lanĉi vin en la kosmon."
    }

Do nun estas du salonoj. Unu havas la internan nomon `salono1` kaj la alia `salono2`. La nomo kiu aperas en la ludo por ambaŭ ejoj estas «salono», ĉar la lino kun `nomo` precizigas la veran nomon. Se la ludantoj iras norden de la unua salono, ri troviĝos en la bluejo. Kiam ri revenos ri kredos ke ri estas denove en la salono sed interne laŭ la programo ri fakte estas en alia ejo kiu havas la saman nomon. La diferenco pri la dua salono estas ke ri nun povas iri supren al alia ejo.

Vi rimarkos ankaŭ ke la priskribo por `salono2` ne estas inter `"citiloj"`. Anstataŭ meti tekston por la priskribo, vi povas meti la nomon de alia ejo. Tiel ĝi havos la saman priskribon kiel tiu ejo. Poste vi vidos ke vi povas uzi ankaŭ la nomon de aliaj aferoj de la ludo, ne nur de ejoj.

### Poentoj

Vi povas aldoni linion kiel `poentoj [nombro]` en la difino de ejo. Tiel, kiam la ludanto unuafoje vizitas tiun ejon, ri ricevos tiun nombron da poentoj. La poentojn la ludsistemo montros je la fino de la ludo.

## Aĵoj

La dua plej grava afero en tekstaventuroj estas la aĵoj. Tiuj estas aferoj kun kiuj la ludanto povas interagi, ekzemple eble ri volos preni ilin kaj porti ilin al alia ejo, aŭ doni ilin al rolulo.

Por krei aĵon ni uzos la ŝlosilvorton `aĵo` kiu priskribos ĝin simile kiel por la ejoj. Atentu ke la vorto enhavas ĉapelan literon, do necesos povi tajpi tion per via klavaro. Ekzemple:

    ejo salono {
      luma
      priskribo "Vi estas en via salono."

      aĵo tranĉilo {
        priskribo "Ĝi estas tre akra."
      }
    }

Nun la ludo havas tranĉilon. Ni metis la kodon por la tranĉilo inter la krampoj `{}` de la salono. Tio signifas ke komence la tranĉilo troviĝos en la salono. Kiam la ludanto estas en la salono, aperos en la priskribo de la ejo ankaŭ la mesaĝo «vi vidas tranĉilon». Sekve ri povas porti ĝin kun si se ri tajpas «preni tranĉilon». Se ri tajpas «rigardi tranĉilon» ri vidos la priskribon.

La aĵo ankaŭ povas komenciĝi en aliaj lokoj. Se ni metos la kodon ekster la krampoj de ejo, ĝi komenciĝos «nenie». Tio estas utila se ni volas aperigi la aĵon nur post specifa okazaĵo en la ludo. Anstataŭ meti la aĵon inter la krampoj de salono, oni ankaŭ povas mencii la komencan lokon per la ŝlosilvorto `loko`. La du manieroj havas la saman efikon. Alia eblo estas meti la vorton `kunportata` en la kodo. Tiel la aĵo komenciĝos portate de la ludanto:

    ejo salono { luma priskribo "Vi ankoraŭ estas en via salono." }

    # La kreditkarto komenciĝos portate de la ludanto
    aĵo kreditkarto {
      priskribo "Vi ĉiam portas vian utilan kreditkarton."
      kunportata
    }

    # La pilko komenciĝos en la salono
    aĵo pilko {
      priskribo "Ĝi estas pilko por la plaĝo."
      loko salono
    }
    
### Nomo de aĵo

Kiel por la ejoj, la aĵoj havas du nomojn; unu kiu estas interna al la programkodo kaj la alia kiun uzas la ludanto. Se vi volas ke la du nomoj estu malsamaj, vi povas aldoni ekzemple `nomo "tondilo"` en la kodo por la aĵo. La nomo devas esti substantivo kiu finiĝas per ‘o’. Ĝi ankaŭ povas havi adjektivon kiu finiĝas per ‘a’. Se oni aldonas la adjektivon, la ludanto ankoraŭ povas indiki la aĵon tajpante aŭ nur la substantivon aŭ la adjektivon kun la substantivo. Por aldoni adjektivon en la interna nomo post la `aĵo` oni devas uzi substrekon ‘_’. Alikaze en la `nomo` oni devas uzi normalan spacon. Ekzemple:

    ejo salono {
      luma
      priskribo "Vi neniam foriros de via salono."

      aĵo rompita_televidilo {
        priskribo "Vi devus ĵeti ĝin."
      }

      aĵo meblo {
        nomo "malpura sofo"
        priskribo "Ĝi havas makulon de vino."
      }
    }

Per tiu kodo la ludanto povus interagi kun la sofo kaj la televidilo jene:

> Vi neniam foriros de via salono. Vi vidas rompitan televidilon kaj malpuran sofon.
>  
> \> rigardi la televidilon
>  
> Vi devus ĵeti ĝin.
>  
> \> rigardi la rompitan televidilon
>  
> Vi devus ĵeti ĝin.
>  
> \> rigardi la sofon malpuran
>  
> Ĝi havas makulon de vino.  

### Alinomoj

Aĵoj ankaŭ povas havi plurajn nomojn. Tio helpas la ludanton trovi la aĵon anstataŭ devi trovi la ekzaktan vorton kiun vi elektis por la aĵo. Por aldoni alinomon al aĵo, simple aldonu linion kun `alinomo` al la kodo:

    aĵo blua_skribilo {
      alinomo "blua krajono"
      alinomo "blua globkrajono"
    }

Tiel la ludanto povas indiki la skribilon ekzemple per «skribilo», «krajono», «globkrajono blua» ktp.

### Neporteblaj aĵoj

Ofte en la priskribo de ejo, oni priskribas aferojn kiujn la ludanto devus ne preni sed tamen ri devus povi interagi kun ĝi. Oni povas ebligi tion en la programkodo se oni aldonas la vorton `neportebla`. Tiam la retpaĝo ne aŭtomate mencios la aĵon en la priskribo de la ejo. Se la ludanto provas preni ĝin, ri ricevas la mesaĝon «Vi ne povas porti …». Ekzemple:

    ejo salono {
      priskribo "Via mondo konsistas nur el malluksa salono kun seĝo en 
                 la angulo."
      luma

      aĵo seĝo {
        priskribo "Ĝi estas malaltkosta ligna seĝo."
        neportebla
      }
    }

Ekzempla partio de tiu ludo povus esti:

> Via mondo konsistas nur el malluksa salono kun seĝo en la angulo.
>
> \> rigardi la seĝon
>
> Ĝi estas malaltkosta ligna seĝo.
>
> \> preni la seĝon
>
> Vi ne povas porti la seĝon.  

### Enhavo

Aĵoj povas ankaŭ enhavi aliajn aĵojn. Per la ŝlosilvorto `enhavo` oni povas agordi la maksimuman grandecon kiun la aĵo povas enhavi. Se oni ne mencias tiun vorton la aĵo havas nulan enhavkapablon kaj la ludanto ne povas meti aferojn en ĝin. La grandecon de la aĵoj oni povas agordi per la ŝlosilvorto `grando`. Ekzemple:

    ejo salono {
      priskribo "Denove salono."
      luma

      aĵo sako {
        priskribo "Ĝi estas malgranda plasta sako."
        enhavo 3
      }

      aĵo pilko {
        priskribo "Ĝi estas plasta pilko por la plaĝo."
        grando 3
      }

      aĵo ŝovelilo {
        priskribo "Ĝi estas plasta ŝovelilo por infanoj."
        grando 2
      }
    }

Nun la ludanto povas meti la pilkon aŭ la ŝovelilon en la sakon, sed ne ambaŭ samtempe ĉar la sumo de la grando estus pli alta ol 3.

> Denove salono. Vi vidas sakon, pilkon kaj ŝovelilon.
>
> \> metu la pilkon en la sakon
>
> Vi prenis la pilkon.
>
> Vi metis la pilkon en la sakon.
>
> \> rigardi la sakon
>
> Ĝi estas malgranda plasta sako. En ĝi vi vidas pilkon.
>
> \> metu la ŝovelilon en la sakon
>
> Vi prenis la ŝovelilon.
>
> La ŝovelilo estas tro granda por eniri la sakon.
>
> \> preni la pilkon
>
> Vi prenis la pilkon.
>
> \> metu la ŝovelilon en la sakon
>
> Vi metis la ŝovelilon en la sakon.  

### Pezo

Krom la grando, aĵo ankaŭ povas havi pezon. La maksimuma suma pezo kiun la ludanto povas porti en la manoj estas 100. Oni povas agordi la pezon de aĵo per la ŝlosilvorto `pezo`, simile kiel por la grandeco. Se oni volas ignori tiun mekanikon por sia ludo, oni povas eviti tiun agordon kaj tiel ĉiu aĵo havas nulan pezon kaj la ludanto povas porti ĉion samtempe.

La diferenco inter `grando` kaj `pezo` estas ke kiam oni metas aĵon en alian aĵon, la pezo de la enhavanta aĵo estas la sumo de la du aĵoj. Tamen la grando de la enhavanta aĵo restas kiel ĝi estis sen la enhavaĵo. Tiel oni povus krei ekzemple dorsosakon kiu havas malgrandan grandecon sed grandan enhavon kaj ludanto povus meti aĵojn en tion por povi porti pli da grando ol cent.

### Fermebleco

Oni povas marki objekton kiel fermitan per la ŝlosilvorto `fermita`. Se oni ankaŭ aldonas `fermebla` la ludanto povos fermi kaj malfermi ĝin.

### Lumo

Se oni ne aldonas la ŝlosilvorton `luma` al ejo, la ludanto ne povas vidi ion ajn kiam ri estas en ĝi. Tio signifas ke ri ne vidos la priskribon de la ejo nek la aĵojn. Ri ne povas uzi aĵon de la ejo eĉ se ri scias ke ĝi estas tie kaj uzas la ĝustan vorton. Ri tamen ja povas uzi la aferojn kiujn ri portas.

Por vidi en malluma ejo, la ludanto bezonas aĵon kun la ŝlosilvorto `lumigita`. La aĵo devas aŭ esti portata de la ludanto, aŭ esti en la ejo. Ĝi povas esti en alia aĵo, sed se tiu aĵo estas fermita la lumo ne helpos.

Oni ankaŭ povas aldoni la ŝlosilvorton `lumigebla` al aĵo. Tiuokaze la ludanto povas ŝalti aŭ malŝalti la aĵon por ekhavi lumon. Tiel ĝi funkcius ekzemple kiel poŝlampo.

### Brulado

Alia maniero por havi lumon estas bruligi ion. Oni povas bruligi nur aferojn kiuj havas la ŝlosilvorton `fajrebla`. Sekve la ludanto bezonas aĵon kiu havas la ŝlosilvorton `fajrilo`. La _fajrebla_ aĵo brulas dum certa nombro da vicoj. Oni povas indiki tiun daŭron per la ŝlosilvorto `fajrodaŭro`. Jen ekzemplo kiu montras kiel tio funkcias:

    ejo arbaro {
      priskribo "Vi estas en arbaro dum la nokto."
    }

    aĵo alumeto {
      kunportata
      fajrilo
    }

    aĵo papero {
      kunportata
      fajrebla
      fajrodaŭro 3
    }

> Estas mallume. Vi vidas nenion.
>
> \> bruligi la paperon per la alumeto
>
> La papero ekbrulas.
>
> Vi estas en arbaro dum la nokto.
>
> \> rigardi la paperon
>
> Ĝi nun fajras kaj forbrulas.
>
> \> rigardi
>
> Vi estas en arbaro dum la nokto.
>
> La papero elbrulis.
>
> \> rigardi
>
> Estas mallume. Vi vidas nenion.

### Pronomo

La ludanto povas uzi pronomojn por indiki aferojn kiujn ri menciis en antaŭaj komandoj. Kelkfoje la ludsistemo mem ankaŭ uzas pronomojn en la aŭtomata teksto se ĝi indikas aferon plurfoje por eviti ripeti la nomon. Por ke tiu funkciu, la sistemo devas scii kiun pronomon uzi por la aĵo. Plej ofte, la aĵoj vere estas aĵoj kaj tial la pronomo `ĝi` taŭgas. Tiuokaze oni ne bezonas aldoni pronomon al la aĵo ĉar tio jam estas la defaŭlto.

Tamen, kelkfoje estas «aĵo» en la ludo kiu fakte estas persono. Tiam oni povas indiki la pronomon de la persono aldonante la ŝlosilvorton `viro` aŭ `ino`. Tiuokaze la ludanto povas uzi _li_ aŭ _ŝi_. Ri ankaŭ povas uzi _ri_ por ambaŭ genroj kiujn la ludsistemo rekonas.

### Aliaj ŝlosilvortoj por aĵoj

* `legebla`: Teksto kiun la ludo montros se la ludanto provas legi la aĵon.
* `poentoj`: Poentoj kiujn la ludanto gajnos je la fino de la ludo se ri portas tiun aĵon.
* `enen`: Indikas ejon al kiu la ludanto iros se ri eniras la objekton.

## Tekstoj

En kelkaj lokoj de la fontokodo oni devas aldoni tekston, kiel por la priskribo de ejo aŭ aĵo, aŭ por la legebla teksto de aĵo. En la antaŭaj ekzemploj, ni simple metis la tekston inter `"citiloj"` rekte kie ĝi aperas. Tamen, kelkfoje tio ne estas tre konvena ĉar oni volas havi la saman tekston en pluraj lokoj. Anstataŭ retajpi la tutan tekston, alia ebleco estas uzi la nomon de alia aĵo aŭ ejo por uzi la saman tekston. Ekzemple:

    ejo kuirejo {
      priskribo "Vi estas en la kuirejo."
      luma
    }

    # Ĉi tiu estas alia kuirejo kiu aspektos tute same al la ludanto
    # sed ĝi fakte estas alia ejo. Eble oni volus fari tion por krei
    # alternativan mondon post certaj okazoj.
    
    ejo fuŝita_kuirejo {
      nomo "kuirejo"
      # Ĉi tiu kuirejo havos la saman priskribon kiel la alia.
      priskribo kuirejo
      luma
    }

Ankoraŭ alia ebleco estas rekte nomi la tekston en aparta loko en la fontokodo kaj poste uzi ĝin en aliaj lokoj. Ekzemple:

    # Ĉi tiu estos la priskribo de pluraj aĵoj.

    teksto verda_aĵo "La aĵo estas verda."

    aĵo vinbero {
      priskribo verda_aĵo
    }

    aĵo pomo {
      priskribo verda_aĵo
    }

    aĵo folio {
      priskribo verda_aĵo
    }

## Fenomenoj

La supra teksto sufiĉas por krei bazan ludon sed ĝi ne povas havi puzlojn kaj ĝi estus tre malinteresa. Por aldoni originalecon al la ludo oni bezonas fenomenojn. Per tiuj oni povas aldoni proprajn verbojn kaj agojn kiam la ludanto interagas kun la ludo. Jen ekzempla ludo kun fenomeno:

    ejo salono {
      priskribo "Vi estas en via salono."
      luma

      aĵo ruĝa_butono {
         priskribo "Estas granda ruĝa butono super kiu estas
                    skribita «NE TUŜU»."
      }
    }

    fenomeno {
      verbo "premi"
      verbo "puŝi"
      verbo "tuŝi"

      aĵo ruĝa_butono

      mesaĝo "Vi puŝis la $An. Koko aperas en la salono."

      nova apero koko
    }

    aĵo koko {
    }

Tiu ludo nun havas butonon kun la speciala fenomeno ke kiam iu puŝas ĝin koko aperas. La fenomeno havas 4 partojn:

Unue estas la verboj. La fenomeno okazas nur kiam la ludanto tajpas unu el la verboj en la listoj. Ĉiu fenomeno devas havi almenaŭ unu verbon.

Due estas la kondiĉoj por la fenomeno. En la supra ekzemplo la kondiĉo estas ke la objekto (la «aĵo») de la frazo kiun la ludanto tajpis devas indiki la ruĝan butonon. Fenomeno povas havi plurajn (aŭ eĉ nul) kondiĉojn.

Trie estas la mesaĝo kiun la ludanto vidos kiam la fenomeno okazas. Ĝi estas nedeviga. Se la fenomeno ne havas mesaĝon ĝi simple okazos silente. La mesaĝo povas enhavi specialajn simbolojn kiuj estos priskribitaj poste.

Laste estas la agoj. Tiuj estas la rezultoj de la fenomeno. En ĉi tiu ekzemplo la aĵo _koko_ aperas en la ejo kie la ludanto troviĝas.

Dum fenomeno okazas, estas kelkaj konceptoj kiujn oni povas uzi:

* _aĵo_: Ĉi tio estas la objekto de la komando kiun la ludanto tajpis. T.e. la aĵo kun la akuzativo.
* _ujo_: La aĵo post la vorto _en_ de la komando.
* _pero_: La aĵo post la vorto _per_ de la komando.
* _direkto_: La aĵo post la vorto _al_ de la komando.
* _ejo_: La ejo de la fenomeno. Plej ofte tio estos la ejo kie troviĝas la ludanto, sed ekzemple se la fenomenon kaŭzis la elbrulo de aĵo, la ejo estos kie troviĝas la aĵo.

### Nomo

La fenomeno povas havi nomon antaŭ la signo `{`. Tio estas utila se oni volas indikon ĝian mesaĝon kiel tekston en alia loko (ekzemple por havi la saman mesaĝon en pluraj fenomenoj) aŭ por reuzi la samajn agojn.

### Ecoj

Ejoj, aĵoj kaj la ludanto mem povas havi _ecojn_. Tiuj estas markiloj kiujn la afero povas havi aŭ malhavi. La fenomenoj povas ŝanĝi tiun staton por forigi aŭ aldoni la econ. La ecoj estas utilaj por marki la progreson de la ludo. Ekzemple, eble en la ludo vi volus aldoni econ al la ludanto por marki ke ri jam solvis puzlon, kaj aĵo en alia loko reagos malsame post tio. Aŭ eble vi havas plurajn aĵojn kiuj reagas simile kaj vi volas la samajn fenomenojn por ili ĉiuj. Vi povas doni al ili specialan econ kaj fari unu fenomenon por ĉiu aĵo kiu havas la econ anstataŭ devi ripeti ĝin. Vi povas aldoni econ al afero per linio kiel `eco solvis_puzlon` en la difino de la afero. Vi povas elekti iun ajn nomon por la eco.

Kelkaj ecoj havas specialan efikon en la ludsistemo. Vi jam renkontis kelkajn el ili en la supraj klarigoj. Ekzemple la eco `luma` estas eco de ejo kiu indikas ke ĝi havas lumon. Do, la ŝlosilvorto `luma` en la difino de ejo estas ekvivalento al `eco luma`. Tio signifas ke per la fenomenoj vi povas forigi la lumecon de ejo post certaj kondiĉoj se tio havas sencon por via ludo.

#### Specialaj ecoj por ejoj

* `luma`: La ludanto povas vidi en la ejo.
* `nelumigebla`: La ejo ne havos lumon, eĉ se la ludanto portas lumigilon.
* `ludfino`: Kiam la ludanto eniras tiun ejon, la ludo finiĝos. Vi povas uzi la priskribon de la ejo kiel finan mesaĝon por la ludo. Pluraj ejoj povas havi tiun econ por havi plurajn finojn de la ludo.

#### Specialaj ecoj por aĵoj

* `portebla`: La ludanto povas porti la aĵon. La aĵoj defaŭlte havas tiun econ, kaj por ne havi ĝin vi povas meti la ŝlosilvorton `neportebla` en la difino de la aĵo.
* `fermebla`: La ludanto povas fermi kaj malfermi la aĵon.
* `fermita`: La aĵo estas aktuale fermita.
* `lumigebla`: La ludanto povas uzi komandojn kun _ŝalti_ aŭ _malŝalti_ por aldoni aldoni la econ `lumigita` al la aĵo.
* `lumigita`: La aĵo aktuale eligas lumon.
* `fajrebla`: Oni povas bruligi la aĵon.
* `fajrilo`: Oni povas uzi la aĵon kiel ilon por bruligi alian aĵon.
* `brulanta`: Aĵo aŭtomate gajnos ĉi tiun econ kiam ĝi ekbrulas.
* `bruligita`: Aĵo aŭtomate gajnos ĉi tiun econ kiam ĝi forbrulas.

### Aĵofinoj

Ĉiu aĵo havas nombrilon por sia «fino». La nombrilo aŭtomate malpliiĝas post ĉiu komando de la ludanto. Vi povas uzi tion en via ludo por okazigi specialan fenemonon per la speciala verbo `fini`. Ekzemple, vi povus havi muson en la ludo kiun la ludanto povas porti, sed se ri portas ĝin tro longe la muso forkuros al sia komenca loko.

### Poentoj

Fenomeno povas ankaŭ havi poentojn per linio kiel `poentoj [nombro]`. Tiam la ludsistemo donos poentojn al la ludanto kiam la fenomeno okazas. Atentu ke la fenomeno povas okazi plurfoje kaj tial la ludanto povas ricevi la poentojn plurfoje. Se vi ne volas ke tio okazu, vi povas agordi la fenomenon por ke ĝi okazu nur unu fojon, ekzemple per la ludantaj ecoj.

### Verboj

Ĉiu fenomeno devas havi almenaŭ unu verbon. Oni povas ankaŭ aldoni plurajn alternativojn por la sama fenomeno. Tio estas utila se la verbo havas plurajn sinonimojn ĉar tiel la ludanto malpli frustriĝos serĉante la «ĝustan» vorton. La verbo devas esti la infinitivo (t.e. ke ĝi finiĝas per «i») sed la fenomeno okazos ankaŭ se la ludanto uzas alian formon.

#### Specialaj verboj

Estas kelkaj specialaj verboj kies fenomenoj okazas eĉ se la ludantoj ne tajpas la verbon. Ili estas:

* `priskribi`: La fenomeno okazas ĉiufoje kiam la ludsistemo montras priskribon de ejo.
* `fajrigi`: La fenomeno okazas kiam aĵo elbruliĝas. La aĵo de la fenomeno estas la forbrulinta aĵo.
* `bruligi`: La fenomeno okazas kiam aĵo ekbrulas. La aĵo de la fenomeno estas la ekbrulinta aĵo.
* `fini`: La fina nombrilo de aĵo atingas nulon. Vidu supre por pli da detaloj.
* `esti`: La fenomeno okazas post ĉiu komando de la uzanto.
* `nordeniri`, `orienteniri`, `sudeniri`, `okcidenteniri`, `supreniri`, `subeniri`, `subeniri` kaj `eliri`: La fenomeno okazas kiam la ludanto provas iri en tiun direkton. Se estas tia fenomeno, la ludsistemo ne aŭtomate movos la ludanton al la nova loko. Tiel oni povas aldoni kondiĉon por povi translokiĝi.
* `rigardi`: La fenomono okazas post kiam la ludanto rigardas aĵon. La aĵo de la fenomeno estos la rigardita aĵo.
* `pren`: La fenomeno okazas kiam la ludanto provas ekhavi objekton. Se estas tia fenomeno la aŭtomata preno ne okazos, sed la agoj mem de la fenomeno povas kaŭzi ekhavon de la aĵo.

### Kondiĉoj

La fenomeno povas havi iun ajn nombron de kondiĉoj. Ili estas en kvar kategorioj:

#### Objektaj kondiĉoj

Ĉi tiaj kondiĉoj komenciĝas per la vortoj _aĵo_, _pero_, _direkto_ aŭ _ujo_ por indiki unu el la konceptoj en la supra listo. Ekzemple, se oni volas limigi la fenomenon al tio kio okazas kiam la ludanto pafas la kuglon al la celo en la arbaro per la pafilo, oni povus fari fenomenon kiel la jenan:

    fenomeno {
        verbo "pafi"

        aĵo kuglo
        direkto celo
        ujo arbaro
        pero pafilo

        mesaĝo "La celo en la arbaro pafiĝis."
    }

Tiuj estas ekzemploj de la unua tipo de objekta kondiĉo. Ankaŭ estas aliaj tipoj. En la subaj ekzemplaj, oni povas anstataŭigi _aĵo_ per iu ajn el la aliaj objektaj vortoj.

* `aĵo [nomo de aĵo]`: La aĵo de la komando estas la nomita aĵo.
* `aĵo io`: La komando enhavas aĵon (aŭ direkton, aŭ ujon ktp).
* `aĵo nenio`: La komando ne enhavas aĵon (aŭ direkton, aŭ ujon ktp).
* `aĵo pezo [nombro]`: La pezo de la aĵo estas almenaŭ `nombro`.
* `aĵo grando [nombro]`: La grandeco de la aĵo estas almenaŭ `nombro`.
* `aĵo enhavo [nombro]`: La enhaveco de la aĵo estas almenaŭ `nombro`.
* `aĵo fajrodaŭro [nombro]`: La restanta fajrodaŭro de la aĵo estas almenaŭ `nombro`.
* `aĵo adjektivo [nomo de alia aĵo]`: La aĵo havas la saman adjektivon kiel la alia aĵo.
* `aĵo nomo [nomo de alia aĵo]`: La aĵo havas la saman nomon kiel la alia aĵo.
* `aĵo kopio [nomo de alia aĵo]`: La aĵo havas la samajn adjektivon kaj nomon kiel la alia aĵo.
* `aĵo eco [nomo de eco]`: La aĵo havas tiun econ.
* `aĵo eco malvera [nomo de eco]`: La aĵo ne havas tiun econ.

Se oni entute ne aldonas kondiĉon kiu komenciĝas per `aĵo`, la ludsistemo implice aldonas la regulon `aĵo nenio` por ke la fenomeno okazu nur kiam la ludanto tajpas komandon sen aĵo. La sama afero okazas per la aliaj tipoj de objekto.

#### Ejaj kondiĉoj

Ĉi tiuj kondiĉoj limigas la ejon de la fenomeno:

* `ejo [nomo de ejo]`: La ejo devas esti tiu ejo.
* `ejo eco [nomo de eco]`: La ejo devas havi tiun econ.
* `ejo eco malvera [nomo de eco]`: La ejo devas ne havi tiun econ.

#### Aliaj kondiĉoj

* `eco [nomo de eco]`: La ludanto havas la nomitan econ.
* `eco malvera [nomo de eco]`: La ludanto ne havas la nomitan econ.
* `ĉeestas [nomo de aĵo]`: La nomita aĵo devas aŭ esti kunportata de la ludanto aŭ esti en la ejo. Se la ejo estas malluma ĝi povas nur esti kunportata. La aĵo ankaŭ povas esti en alia aĵo se ĝi ne estas fermita.
* `ŝanco [elcento]`: La kondiĉo sukcesas je probableco de `elcento`.

### Mesaĝo

La mesaĝon de la fenomeno la ludsistemo montros antaŭ ol la agoj okazos. Ĝi povas havi la jenajn specialajn markilojn. La ludsistemo aŭtomate anstataŭigos ilin.

* `$A`: La aĵo de la komando.
* `$An`: La aĵo de la komando kun la akuzativo.
* `$P`: La pero de la komando.
* `$Pn`: La pero de la komando kun la akuzativo.
* `$E`: La ujo de la komando.
* `$En`: La ujo de la komando kun la akuzativo.
* `$>`: La direkto de la komando.
* `$>n`: La direkto de la komando kun la akuzativo.
* `$V`: La verbo de la komando, sen finaĵo.
* `$D`: Kiam tio aperas en mesaĝo, la ludsistemo iomete paŭzos antaŭ ol montri la sekvon de la teksto. Tio estas utila por krei draman etoson.
* `$S`: Kutime la ludsistemo ne montros la mesaĝon se la fenomeno ne okazas en la sama loko kie troviĝas la ludanto. Tamen, se oni aldonas `$S`, la sekva parto de la mesaĝo ja estos montrita tiuokaze.
* `$$`: Skribu tion se vi volas ke «$» aperu en la mesaĝo.

### Agoj

La kerno de la fenomeno estas la agoj. Tio estas la efikoj kiujn ĝi aplikos al la stato de la ludo. Fenomeno povas havi iun ajn nombron de agoj kaj ili okazos laŭ la ordo en kiu ili aperas en la fontokodo. Atentu ke kvankam oni povas miksi kondiĉojn kaj agojn en la fontokodo, la ludsistemo fakte kontrolos ĉiujn kondiĉojn antaŭ ol okazigi la agojn. Ĉiu ago komenciĝas per la ŝlosilvorto `nova`.

#### Objektaj agoj

Simile pri la objektaj kondiĉoj, ĉi tiuj agoj povas egale enhavi la vortojn _aĵo_, _pero_, _direkto_ aŭ _ujo_.

* `nova aĵo [nomo de aĵo]`: La aĵo malaperas kaj anstataŭas ĝin la nomita aĵo.
* `nova aĵo alien [nomo de ejo]`: La aĵo translokiĝos al la nomita ejo.
* `nova aĵo enen [nomo de aĵo]`: La aĵo translokiĝos en la nomitan aĵon.
* `nova aĵo fino [nombro]`: Ŝanĝas la valoron de la nombrilo de la «fino» de aĵo al la nombro.
* `nova aĵo pezo [nombro]`: Ŝanĝas la pezon de la aĵo.
* `nova aĵo grando [nombro]`: Ŝanĝas la grandecon de la aĵo.
* `nova aĵo enhavo [nombro]`: Ŝanĝas la enhavgrandecon de la aĵo.
* `nova aĵo fajrodaŭro [nombro]`: Ŝanĝas la fajrodaŭron de la aĵo.
* `nova aĵo adjektivo [nomo de aĵo]`: Ŝanĝas la adjektivon de la aĵo al tiu de la nomita aĵo.
* `nova aĵo nomo [nomo de aĵo]`: Ŝanĝas la nomon de la aĵo al tiu de la nomita aĵo.
* `nova aĵo kopio [nomo de aĵo]`: La aĵo fariĝas kopio de la nomita aĵo. Tio signifas ke ĝi ekhavos la saman nomon kaj ĉiun alian staton de la aĵo.
* `nova aĵo eco [nomo de eco]`: La aĵo ekhavos la nomitan econ.
* `nova aĵo eco malvera [nomo de eco]`: La aĵo ne plu havos la nomitan econ.
* `nova aĵo kunportata`: La ludanto ekportos la aĵon. Tio povas esti utila por permesi ekporti aĵon kiu estas neportebla, ekzemple se oni volas havi aĵon kiu ne aperos en la priskribo de la ejo.

#### Ejaj agoj

* `nova ejo [nomo de ejo]`: La ludanto translokiĝos al la nomita ejo.
* `nova ejo eco [nomo de eco]`: La ejo de la fenomeno ekhavos la nomitan econ.
* `nova ejo eco malvera [nomo de eco]`: La ejo de la fenomeno ne plu havos la nomitan econ.

#### Rulu la agojn de alia fenomeno

Vi povas nomi alian fenomenon ene de fenomeno. Tiuokaze la ludsistemo okazigos ankaŭ la agojn de la alia fenomeno, inkluzive ĝian mesaĝon. Se vi konas aliajn programlingvojn, vi povas pensi pri tio simile kiel pri proceduro de la programo. Ĝi estas utila ekzemple se vi volas ke io okazu per du malsamaj manieroj. Jen ekzemplo:

    ejo salono {
       priskribo "Ĉio okazas en salonoj."
       luma

       aĵo butono {
         fenomeno puŝas_butonon {
           verbo "puŝi"
           mesaĝo "Via domo eksplodis kaj vi mortis."
           nova ejo morto
         }
       }

       fenomeno {
         verbo "eksplodi"
         fenomeno puŝas_butonon
       }
    }

    ejo morto {
       priskribo "Vi mortis. Domaĝe."
       ludfino
    }

En tiu ludo estas butono en la salono kaj se la ludanto puŝas ĝin la domo eksplodas kaj ri mortos. Alternative, ri povas simple tajpi «eksplodu» kaj tiam la sama afero okazos. Anstataŭ devi retajpi la agojn de la unua fenomeno, la programo uzis la specialan agon `fenomeno puŝas_butonon` por reuzi la samajn agojn de la fenomeno `puŝas_butonon`.

#### Aliaj agoj

* `nova eco [nomo de eco]`: La ludanto ekhavos la nomitan econ.
* `nova eco malvera [nomo de eco]`: La ludanto ne plu havos la nomitan econ.
* `nova kunportata [nomo de aĵo]`: La ludanto ekportos la nomitan aĵon.
* `nova apero [nomo de aĵo]`: La nomita aĵo aperos en la ejo de la fenomeno.

### Aŭtomataj kondiĉoj

Vi povas meti la difinon de fenomeno rekte en la difinon de aĵo aŭ ejo. Tio aldonas korespondan aŭtomatan kondiĉon al la fenomeno. Ekzemple:

    aĵo dentopasto {
      fenomeno {
        verbo "preni"
        mesaĝo "La dentopasto estas tre malpura kaj vi ne volas tuŝi ĝin."
      }
    }

    ejo salono {
      luma
      priskribo "Vi estas en via salono. Urso staras apud la orienta pordo."

      fenomeno {
        verbo "okcidenteniri"
        mesaĝo "La urso malhelpas vin iri okcidenten de la salono."
      }
    }

La unua fenomeno aŭtomate havos la kondiĉon `aĵo dentopasto`. Tiel ĝi validas nur kiam la ludanto tajpas «preni dentopaston» kaj ne kiam ri prenas alian aĵon.

La dua fenomeno aŭtomate havos `ejo salono`. Do ĝi validas nur kiam la ludanto estas en la salono kaj ne okazas se ri provas iri orienten de alia ejo.

## Eksporti la ludon

Kiam vi finverkis vian ludon, vi povas eksporti ĝin al HTML-a dosiero per la butono ĉe la supro de la redaktilo. Tio eligos unu dosieron en formato HTML kiu enhavas kaj la ludsistemon kaj vian ludon. Vi povas doni tiun dosieron al aliaj homoj por ke ili povu ludi vian ludon.
