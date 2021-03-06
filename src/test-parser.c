/*
 * Aventuro - A text aventure system in Esperanto
 * Copyright (C) 2021  Neil Roberts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <strings.h>

#include "pcx-parser.h"
#include "pcx-list.h"
#include "pcx-buffer.h"

struct load_data {
        struct pcx_source source;
        const char *data;
        size_t pos, size;
};

struct fail_check {
        const char *source;
        const char *error_message;
};

struct string_check {
        const char *source;
        const char *expected;
};

#define BLURB "nomo \"testnomo\" aŭtoro \"testaŭtoro\" jaro \"2021\"\n"

static const struct fail_check
fail_checks[] = {
        {
                BLURB,
                "La ludo bezonas almenaŭ unu ejon"
        },
        {
                BLURB
                "ejo { }",
                "linio 2: Atendis nomon de ejo",
        },
        {
                BLURB
                "ejo ejo1 { }",
                "linio 2: Mankas priskribo de la ejo",
        },
        {
                BLURB
                "ejo ejo1 {\n"
                "nomo \"ejo unu\"\n"
                "nomo \"ankoraŭ ejo unu\"\n"
                "}\n",
                "linio 4: La ejo jam havas nomon"
        },
        {
                BLURB
                "ejo ejo1 {\n"
                "priskribo \"ejo unu\"\n"
                "priskribo \"ankoraŭ ejo unu\"\n"
                "}\n",
                "linio 4: La ejo jam havas priskribon"
        },
        {
                BLURB
                "ejo ejo1 {\n"
                "poentoj -1\n"
                "}\n",
                "linio 3: Numero estas ekster la valida intervalo",
        },
        {
                BLURB
                "ejo ejo1 {\n"
                "poentoj 256\n"
                "}\n",
                "linio 3: Numero estas ekster la valida intervalo",
        },
        {
                BLURB
                "ejo ejo1 {\n"
                "eco 42\n"
                "}\n",
                "linio 3: Atendis nomon de eco",
        },
        {
                BLURB
                "ejo ejo1 {\n"
                "eco a4 eco a5 eco a6 eco a7\n"
                "eco a8 eco a9 eco a10 eco a11\n"
                "eco a12 eco a13 eco a14 eco a15\n"
                "eco a16 eco a17 eco a18 eco a19\n"
                "eco a20 eco a21 eco a22 eco a23\n"
                "eco a24 eco a25 eco a26 eco a27\n"
                "eco a28 eco a29 eco a30 eco a31\n"
                "eco a32\n"
                "}\n",
                "linio 10: Tro da unikaj ecoj",
        },
        {
                BLURB
                "ejo ejo1 {\n"
                "okcidenten 3\n"
                "}\n",
                "linio 3: Atendis nomon de afero",
        },
        {
                BLURB
                "ejo ejo1 {\n"
                "okcidenten ejo1\n"
                "okcidenten ejo1\n"
                "}\n",
                "linio 4: La ejo jam havas okcidentan direkton",
        },
        {
                BLURB
                "ejo ejo1\n"
                "{ priskribo \"!\" }\n"
                "ejo ejo1\n"
                "{ priskribo \"!\" }\n",
                "linio 4: Pluraj aferoj havas la saman nomon",
        },
        {
                BLURB
                "ejo ejo1 { priskribo 3 }",
                "linio 2: Atendis tekston",
        },
        {
                BLURB
                "ejo ejo1 {\n"
                "3\n"
                "}",
                "linio 3: Atendis eron de ejo aŭ ‘}’",
        },
        {
                BLURB
                "ejo ejo1 {\n",
                "linio 3: Atendis eron de ejo aŭ ‘}’",
        },
        {
                "teksto 3",
                "linio 1: Atendis nomon de la teksto",
        },
        {
                "teksto t\n"
                "3",
                "linio 2: Atendis tekston",
        },
        {
                "nomo t\n",
                "linio 1: Atendis tekston",
        },
        {
                "# This is a comment\n"
                "3",
                "linio 2: Atendis dosier-nivelan eron (kiel ejon, aĵon ktp)",
        },
        {
                BLURB
                "ejo ejo1 { priskribo ejo2 }\n"
                "ejo ejo2 { priskribo ejo3 }\n"
                "ejo ejo3 { priskribo ejo1 }\n",
                "linio 2: Detektis ciklan referencon",
        },
        {
                BLURB
                "ejo ejo1 { priskribo ejo1 }\n",
                "linio 2: Detektis ciklan referencon",
        },
        {
                BLURB
                "ejo ejo1 { priskribo ejo3 }\n",
                "linio 2: Nevalida referenco al teksto",
        },
        {
                BLURB
                /* The description of the kitchen is taken from an
                 * object which doesn’t have a description.
                 */
                "ejo kuirejo { priskribo anonima_objekto }\n"
                "aĵo anonima_objekto { }\n",
                "linio 2: Nevalida referenco al teksto",
        },
        {
                BLURB
                "ejo ejo1 {\n"
                " priskribo \"j\"\n"
                " norden wibble\n"
                "}\n",
                "linio 4: Nevalida referenco al ejo",
        },
        {
                BLURB
                "ejo ejo1 {\n"
                " priskribo \"j\"\n"
                " norden t\n"
                "}\n"
                "teksto t \"not a room\"\n",
                "linio 4: Nevalida referenco al ejo",
        },
        {
                "teksto t \"what \x80\"",
                "linio 1: Teksto enhavas nevalidan UTF-8"
        },
        {
                "ejo s\x80t",
                "linio 1: Trovis nevalidan UTF-8"
        },
        {
                "ejo s {\n"
                "(\n",
                "linio 2: Neatendita signo ‘(’"
        },
        {
                "42s",
                "linio 1: Nevalida numero “42s”"
        },
        {
                "\"\\n\"",
                "linio 1: Nevalida kodŝanĝa signo"
        },
        {
                "\"\\",
                "linio 1: Nevalida kodŝanĝa signo"
        },
        {
                BLURB
                "ejo s1 {\n"
                " priskribo \"I forgot to close the door\n"
                "}\n",
                "linio 3: Senfina teksto"
        },
        {
                BLURB
                "ejo s1 {\n"
                " direkto potato potato \"potato\"\n"
                "}\n",
                "linio 3: Atendis nomon de la direkto",
        },
        {
                BLURB
                "ejo s1 {\n"
                " direkto \"potat\" potato \"potato\"\n"
                "}\n",
                "linio 3: Nomo de direkto devas esti substantivo",
        },
        {
                BLURB
                "ejo s1 {\n"
                " direkto \"granda potato\" potato \"potato\"\n"
                "}\n",
                "linio 3: Nomo de direkto devas esti substantivo",
        },
        {
                BLURB
                "ejo s1 {\n"
                " direkto \"\" potato \"potato\"\n"
                "}\n",
                "linio 3: Nomo de direkto devas esti substantivo",
        },
        {
                BLURB
                "ejo s1 {\n"
                " direkto \"potato\" 3 \"potato\"\n"
                "}\n",
                "linio 3: Atendis nomon de ejo",
        },
        {
                BLURB
                "ejo s1 {\n"
                " direkto \"potato\" s2 \"potato\"\n"
                " priskribo \"j\"\n"
                "}\n",
                "linio 3: Nevalida referenco al ejo",
        },
        {
                BLURB
                "ejo s1 {\n"
                " direkto \"potato\" s1 s3\n"
                " priskribo \"j\"\n"
                "}\n",
                "linio 3: Nevalida referenco al teksto",
        },
        {
                "aĵo skribilo {\n"
                " viro\n"
                " ino\n"
                "}",
                "linio 3: La pronomo jam estas difinita",
        },
        {
                "aĵo\n"
                " {\n",
                "linio 2: Atendis nomon de aĵo",
        },
        {
                "aĵo skribilo\n"
                " { (\n",
                "linio 2: Neatendita signo ‘(’"
        },
        {
                "aĵo skribilo {\n"
                " poentoj \"6\" \n"
                "}\n",
                "linio 2: Atendis numeron",
        },
        {
                "aĵo skribilo {\n"
                " ejo { }\n"
                "}\n",
                "linio 2: Atendis eron de aĵo aŭ ‘}’",
        },
        {
                "ejo s1 { aĵo skribilo {\n"
                " ejo { }\n"
                "} }\n",
                "linio 2: Atendis eron de aĵo aŭ ‘}’",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "aĵo ruza_juvelo {\n"
                "  loko t1\n"
                "}\n"
                "teksto t1 \"hi\"\n",
                "linio 4: Nevalida referenco al loko"
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "aĵo ruza_juvelo {\n"
                "  loko t1\n"
                "}\n",
                "linio 4: Nevalida referenco al loko"
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝo juvelo\"\n"
                "}\n",
                "linio 3: La nomo de la aĵo devas havi nedevigan "
                "adjektivon kaj substantivon",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"a juvelo\"\n"
                "}\n",
                "linio 3: La nomo de la aĵo devas havi nedevigan "
                "adjektivon kaj substantivon",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝa juvel\"\n"
                "}\n",
                "linio 3: La nomo de la aĵo devas havi nedevigan "
                "adjektivon kaj substantivon",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝa \"\n"
                "}\n",
                "linio 3: La nomo de la aĵo devas havi nedevigan "
                "adjektivon kaj substantivon",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝaj juvelo\"\n"
                "}\n",
                "linio 3: La adjektivo kaj la substantivo de la aĵo "
                "ne havas la saman plurecon",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝaj &juveloj\"\n"
                "}\n",
                "linio 3: La nomo de la aĵo devas havi nedevigan "
                "adjektivon kaj substantivon",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝaj juveloj\"\n"
                "  besto\n"
                "}\n",
                "linio 3: La aĵo havas pluralan nomon "
                "sed nepluralan pronomon",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝaj juveloj\"\n"
                "  priskribo t\n"
                "}\n",
                "linio 5: Nevalida referenco al teksto",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝaj juveloj\"\n"
                "  legebla t\n"
                "}\n",
                "linio 5: Nevalida referenco al teksto",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "aĵo ruĝa_juvelo {\n"
                "   enen t\n"
                "}\n",
                "linio 4: Nevalida referenco al ejo",
        },
        {
                BLURB
                "ejo s1 {\n"
                "  priskribo \"j\"\n"
                "  aĵo ruĝa_juvelo {\n"
                "    kunportata\n"
                "  }\n"
                "}\n",
                "linio 4: La aĵo estas markita kiel kunportata "
                "sed ĝi ankaŭ havas lokon",
        },
        {
                "aĵo a { aĵo b{",
                "linio 1: Atendis eron de aĵo aŭ ‘}’",
        },
        {
                "aĵo a { alinomo \"a\" }",
                "linio 1: La nomo de la aĵo devas havi nedevigan "
                "adjektivon kaj substantivon",
        },
        {
                "aĵo a { alinomo \"fluga\" }",
                "linio 1: La nomo de la aĵo devas havi nedevigan "
                "adjektivon kaj substantivon",
        },
        {
                "aĵo a { alinomo \"flu3a melono\" }",
                "linio 1: La nomo de la aĵo devas havi nedevigan "
                "adjektivon kaj substantivon",
        },
        {
                "fenomeno { aĵo ŝargo -3 }",
                "linio 1: Numero estas ekster la valida intervalo",
        },
        {
                "fenomeno {\n"
                "eco a1 eco a2 eco a3\n"
                "eco a4 eco a5 eco a6 eco a7\n"
                "eco a8 eco a9 eco a10 eco a11\n"
                "eco a12 eco a13 eco a14 eco a15\n"
                "eco a16 eco a17 eco a18 eco a19\n"
                "eco a20 eco a21 eco a22 eco a23\n"
                "eco a24 eco a25 eco a26 eco a27\n"
                "eco a28 eco a29 eco a30 eco a31\n"
                "eco a32\n"
                "}\n",
                "linio 10: Tro da unikaj ecoj",
        },
        {
                "fenomeno { verbo 3 }\n",
                "linio 1: Atendis tekston",
        },
        {
                "fenomeno { nova eco 6 }\n",
                "linio 1: Atendis nomon de eco aŭ “malvera”",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "fenomeno { verbo \"talk\" }\n",
                "linio 3: La verbo devas finiĝi per ‘i’",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "fenomeno {\n"
                "   verbo \"paroli\"\n"
                "   aĵo s1\n"
                "}\n",
                "linio 5: Atendis nomon de aĵo",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "aĵo ruĝa_beto { }\n"
                "fenomeno {\n"
                "   verbo \"paroli\"\n"
                "   nova ejo ruĝa_beto\n"
                "}\n",
                "linio 6: Atendis nomon de ejo",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "fenomeno {\n"
                "  mesaĝo \"Vi forgesis la verbon!\"\n"
                "}\n",
                "linio 3: Ĉiu fenomeno bezonas almenaŭ unu verbon",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "fenomeno {\n"
                "  mesaĝo \"Vi forgesis la verbon!\"\n"
                "  3\n"
                "}\n",
                "linio 5: Atendis eron de fenomeno aŭ ‘}’",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "fenomeno {\n"
                "  mesaĝo 3\n"
                "}\n",
                "linio 4: Atendis tekston",
        },
        {
                BLURB
                "ejo s1 { priskribo \"j\" }\n"
                "fenomeno {\n"
                "  mesaĝo t23\n"
                "  verbo \"kuri\"\n"
                "}\n",
                "linio 4: Nevalida referenco al teksto",
        },
        {
                "fenomeno \"potato\" {\n"
                "  verbo \"kuri\"\n"
                "}",
                "linio 1: Atendis nomon de fenomeno aŭ ‘{’",
        },
        {
                "fenomeno mia_regulo\n"
                "         \"potato\" {\n"
                "  verbo \"kuri\"\n"
                "}",
                "linio 2: Atendis ‘{’",
        },
        {
                "teksto mia_regulo \"strange\"\n"
                "fenomeno mia_regulo\n"
                "         \"potato\" {\n"
                "  verbo \"kuri\"\n"
                "}",
                "linio 2: Pluraj aferoj havas la saman nomon",
        },
        {
                "fenomeno {\n"
                "  verbo \"kuri\"\n"
                "  &amp;\n"
                "}",
                "linio 3: Neatendita signo ‘&’"
        },
        {
                "fenomeno {\n"
                "  fenomeno 3\n"
                "}\n",
                "linio 2: Atendis nomon de fenomeno",
        },
        {
                BLURB
                "ejo salojo { priskribo \"salono\" }\n"
                "fenomeno {\n"
                "  fenomeno t1\n"
                "  verbo \"kuri\"\n"
                "}\n"
                "teksto t1 \"hi\"\n",
                "linio 4: Atendis nomon de fenomeno",
        },
        {
                BLURB
                "ejo salono { priskribo \"salono\" }\n"
                "fenomeno {\n"
                "  nova verbo \"kuri\"\n"
                "}\n",
                "linio 4: Nekonata ago “verbo”",
        },
};

static const struct string_check
string_checks[] = {
        {
                "hello",
                "hello"
        },
        {
                "  hello  ",
                "hello"
        },
        {
                "  hello  there  ",
                "hello there"
        },
        {
                "  hello\nthere  ",
                "hello there"
        },
        {
                "  hello \n there \n ",
                "hello there"
        },
        {
                "  hello \r\n there \n ",
                "hello there"
        },
        {
                "\nhello\n",
                "hello"
        },
        {
                "\n\n\nhello\n\n\n",
                "hello"
        },
        {
                "hello\n\nthere",
                "hello\n\nthere"
        },
        {
                "hello\r\n\r\nthere",
                "hello\n\nthere"
        },
        {
                "hello  \n    \n  there  \n",
                "hello\n\nthere"
        },
        {
                "hello\n\n\n\n\nthere",
                "hello\n\n\n\n\nthere"
        },
        {
                "\\\\o//",
                "\\o//"
        },
};

static bool
seek_source_cb(struct pcx_source *source,
               long pos,
               struct pcx_error **error)
{
        struct load_data *data =
                pcx_container_of(source, struct load_data, source);

        data->pos = pos;

        return true;
}

static size_t
read_source_cb(struct pcx_source *source,
               void *ptr,
               size_t length)
{
        struct load_data *data =
                pcx_container_of(source, struct load_data, source);

        if (data->pos >= data->size)
                return 0;

        if (length + data->pos > data->size)
                length = data->size - data->pos;

        memcpy(ptr, data->data + data->pos, length);

        data->pos += length;

        return length;
}

static struct pcx_avt *
load_from_string(const char *str,
                 struct pcx_error **error)
{
        struct load_data data = {
                .source = {
                        .seek_source = seek_source_cb,
                        .read_source = read_source_cb,
                },
                .data = str,
                .pos = 0,
                .size = strlen(str),
        };

        return pcx_parser_parse(&data.source, error);
}

static struct pcx_avt *
expect_success(const char *str)
{
        struct pcx_error *error = NULL;
        struct pcx_avt *avt = load_from_string(str, &error);

        if (avt == NULL) {
                fprintf(stderr,
                        "Parsing unexpectedly failed for source:\n"
                        " Error: %s\n"
                        "%s\n",
                        error->message,
                        str);
                pcx_error_free(error);
        }

        return avt;
}

static bool
check_error_message(const char *source,
                    const char *message)
{
        struct pcx_error *error = NULL;
        struct pcx_avt *avt = load_from_string(source, &error);
        bool ret = true;

        if (avt) {
                fprintf(stderr,
                        "Expected error message but source compiled:\n"
                        "%s\n",
                        source);
                pcx_avt_free(avt);
                return false;
        }

        assert(error);

        if (strcmp(error->message, message)) {
                fprintf(stderr,
                        "Error message does not match expected:\n"
                        "  Expected: %s\n"
                        "  Received: %s\n"
                        "\n"
                        "Source:\n"
                        "%s\n",
                        message,
                        error->message,
                        source);
                ret = false;
        }

        pcx_error_free(error);

        return ret;
}

static bool
check_strings(void)
{
        bool ret = true;

        for (unsigned i = 0; i < PCX_N_ELEMENTS(string_checks); i++) {
                struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

                pcx_buffer_append_printf(&buf,
                                         "%s\n"
                                         "ejo s1 {\n"
                                         "  priskribo \"%s\"\n"
                                         "}",
                                         BLURB,
                                         string_checks[i].source);

                struct pcx_avt *avt = expect_success((const char *) buf.data);

                if (avt == NULL) {
                        ret = false;
                } else {
                        if (strcmp(string_checks[i].expected,
                                   avt->rooms[0].description)) {
                                fprintf(stderr,
                                        "String parsing failed:\n"
                                        "  Expected: \"%s\"\n"
                                        "  Received: \"%s\"\n"
                                        "  Source:   \"%s\"\n"
                                        "\n",
                                        string_checks[i].expected,
                                        avt->rooms[0].description,
                                        string_checks[i].source);
                                ret = false;
                        }
                        pcx_avt_free(avt);
                }

                pcx_buffer_destroy(&buf);
        }

        return ret;
}

int
main(int argc, char **argv)
{
        for (unsigned i = 0; i < PCX_N_ELEMENTS(fail_checks); i++) {
                if (!check_error_message(fail_checks[i].source,
                                         fail_checks[i].error_message))
                        return EXIT_FAILURE;
        }

        struct pcx_avt *avt;

        avt = expect_success(BLURB
                             "ejo ejo1 { priskribo ejo2 }\n"
                             "ejo ejo2 { priskribo la_nomo }\n"
                             "teksto la_nomo \"hi\"\n");
        assert(avt->n_rooms == 2);
        assert(avt->rooms[0].description == avt->rooms[1].description);
        assert(!strcmp(avt->rooms[0].description, "hi"));
        assert(!strcmp(avt->rooms[0].name, "ejo1"));
        assert(avt->rooms[0].points == 0);
        assert(avt->rooms[0].attributes == 0);
        assert(!strcmp(avt->rooms[1].name, "ejo2"));
        assert(!strcmp(avt->name, "testnomo"));
        assert(!strcmp(avt->author, "testaŭtoro"));
        assert(!strcmp(avt->year, "2021"));
        assert(avt->introduction == NULL);
        pcx_avt_free(avt);

        avt = expect_success(BLURB
                             "ejo ruĝa_ejo { priskribo \"j\" }\n");
        assert(avt->n_rooms == 1);
        assert(!strcmp(avt->rooms[0].description, "j"));
        assert(!strcmp(avt->rooms[0].name, "ruĝa ejo"));
        pcx_avt_free(avt);

        avt = expect_success(BLURB
                             "ejo ejo1 {\n"
                             " norden n\n"
                             " orienten e\n"
                             " suden s\n"
                             " okcidenten w\n"
                             " supren up\n"
                             " suben down\n"
                             " elen out\n"
                             " priskribo t\n"
                             "}\n"
                             "ejo n { priskribo t }\n"
                             "ejo e { priskribo t }\n"
                             "ejo s { priskribo t }\n"
                             "ejo w { priskribo t }\n"
                             "ejo up { priskribo t }\n"
                             "ejo down { priskribo t }\n"
                             "ejo out { priskribo t }\n"
                             "teksto t \"j\"\n");
        assert(avt->n_rooms == 8);
        assert(!strcmp(avt->rooms[0].description, "j"));
        for (int i = 0; i < PCX_AVT_N_DIRECTIONS; i++)
                assert(avt->rooms[0].movements[i] == i + 1);
        for (int i = 1; i < avt->n_rooms; i++) {
                for (int j = 0; j < PCX_AVT_N_DIRECTIONS; j++)
                        assert(avt->rooms[i].movements[j] ==
                               PCX_AVT_DIRECTION_BLOCKED);
        }
        pcx_avt_free(avt);

        avt = expect_success(BLURB
                             "enkonduko \"Jen la ludo!\"\n"
                             "ejo ruĝa_ejo {\n"
                             " priskribo \"j\"\n"
                             " nomo \"ŝanĝita nomo\"\n"
                             " poentoj 42\n"
                             " luma\n"
                             " nelumigebla\n"
                             " ludfino\n"
                             " eco ruĝa\n"
                             "}\n"
                             "ejo verda_ejo {\n"
                             " priskribo \"  j  \"\n"
                             " eco verda\n"
                             " poentoj 4_2"
                             "}\n"
                             "ejo verdruĝa_ejo {\n"
                             " priskribo \"j    \n   \\\"j\\\"\"\n"
                             " eco verda\n"
                             " eco ruĝa\n"
                             "}\n");
        assert(!strcmp(avt->introduction, "Jen la ludo!"));
        assert(avt->n_rooms == 3);
        assert(!strcmp(avt->rooms[0].description, "j"));
        assert(!strcmp(avt->rooms[1].description, "j"));
        assert(!strcmp(avt->rooms[2].description, "j \"j\""));
        assert(!strcmp(avt->rooms[0].name, "ŝanĝita nomo"));
        assert(avt->rooms[0].points == 42);
        assert(avt->rooms[0].attributes ==
               (PCX_AVT_ROOM_ATTRIBUTE_LIT |
                PCX_AVT_ROOM_ATTRIBUTE_UNLIGHTABLE |
                PCX_AVT_ROOM_ATTRIBUTE_GAME_OVER |
                (1 << 4)));
        assert(avt->rooms[1].attributes == (1 << 5));
        assert(avt->rooms[1].points == 42);
        assert(avt->rooms[2].attributes == ((1 << 4) | (1 << 5)));
        pcx_avt_free(avt);

        avt = expect_success(BLURB
                             "ejo vendejaro {\n"
                             " priskribo \"Multe da vendejoj\"\n"
                             " direkto \"librejo\" librejo\n"
                             "         \"Ĝi estas brokanta librejo.\"\n"
                             " direkto \"gitaroj\" muzikejo nenio\n"
                             "}\n"
                             "ejo librejo {\n"
                             " priskribo \"Estas nur 3 libroj aĉeteblaj.\"\n"
                             " direkto \"koridoro\" vendejaro vendejaro\n"
                             "}\n"
                             "ejo muzikejo {\n"
                             " priskribo \"Ĉi tie estas ĉefe gitaroj.\"\n"
                             "}\n");
        assert(avt->n_rooms == 3);
        assert(avt->rooms[0].n_directions == 2);
        assert(!strcmp(avt->rooms[0].directions[0].name, "librej"));
        assert(!strcmp(avt->rooms[0].directions[0].description,
                       "Ĝi estas brokanta librejo."));
        assert(avt->rooms[0].directions[0].target == 1);
        assert(!strcmp(avt->rooms[0].directions[1].name, "gitar"));
        assert(avt->rooms[0].directions[1].description == NULL);
        assert(avt->rooms[0].directions[1].target == 2);
        assert(avt->rooms[1].n_directions == 1);
        assert(!strcmp(avt->rooms[1].directions[0].name, "koridor"));
        assert(!strcmp(avt->rooms[1].directions[0].description,
                       "Multe da vendejoj"));
        assert(avt->rooms[1].directions[0].target == 0);
        assert(avt->rooms[2].n_directions == 0);
        pcx_avt_free(avt);

        avt = expect_success(BLURB
                             "ejo j { priskribo \"j\" }\n"
                             "aĵo skribilo {\n"
                             "   priskribo \"Ĝi estas skribilo.\"\n"
                             "   nomo \"blua skribilo\"\n"
                             "   besto\n"
                             "   poentoj 1\n"
                             "   pezo 2\n"
                             "   grando 3\n"
                             "   enhavo 4\n"
                             "   perpafo 5\n"
                             "   ŝargo 6\n"
                             "   perbato 7\n"
                             "   perpiko 8\n"
                             "   manĝeblo 9\n"
                             "   trinkeblo 10\n"
                             "   fajrodaŭro 11\n"
                             "   fino 12\n"
                             "   neportebla\n"
                             "   fermebla\n"
                             "   fermita\n"
                             "   lumigebla\n"
                             "   lumigita\n"
                             "   fajrebla\n"
                             "   fajrilo\n"
                             "   brulanta\n"
                             "   bruligita\n"
                             "   manĝebla\n"
                             "   trinkebla\n"
                             "   venena\n"
                             "   eco bloba\n"
                             "}\n");
        assert(avt->n_objects == 1);
        assert(!strcmp(avt->objects[0].base.adjective, "blu"));
        assert(!strcmp(avt->objects[0].base.name, "skribil"));
        assert(!strcmp(avt->objects[0].base.description, "Ĝi estas skribilo."));
        assert(avt->objects[0].base.pronoun == PCX_AVT_PRONOUN_ANIMAL);
        assert(avt->objects[0].base.attributes ==
               (PCX_AVT_OBJECT_ATTRIBUTE_CLOSABLE |
                PCX_AVT_OBJECT_ATTRIBUTE_CLOSED |
                PCX_AVT_OBJECT_ATTRIBUTE_LIGHTABLE |
                PCX_AVT_OBJECT_ATTRIBUTE_LIT |
                PCX_AVT_OBJECT_ATTRIBUTE_FLAMMABLE |
                PCX_AVT_OBJECT_ATTRIBUTE_LIGHTER |
                PCX_AVT_OBJECT_ATTRIBUTE_BURNING |
                PCX_AVT_OBJECT_ATTRIBUTE_BURNT_OUT |
                PCX_AVT_OBJECT_ATTRIBUTE_EDIBLE |
                PCX_AVT_OBJECT_ATTRIBUTE_DRINKABLE |
                PCX_AVT_OBJECT_ATTRIBUTE_POISONOUS |
                (1 << 13)));
        assert(avt->objects[0].points == 1);
        assert(avt->objects[0].weight == 2);
        assert(avt->objects[0].size == 3);
        assert(avt->objects[0].container_size == 4);
        assert(avt->objects[0].shot_damage == 5);
        assert(avt->objects[0].shots == 6);
        assert(avt->objects[0].hit_damage == 7);
        assert(avt->objects[0].stab_damage == 8);
        assert(avt->objects[0].food_points == 9);
        assert(avt->objects[0].trink_points == 10);
        assert(avt->objects[0].burn_time == 11);
        assert(avt->objects[0].end == 12);
        assert(avt->objects[0].enter_room == PCX_AVT_DIRECTION_BLOCKED);
        assert(avt->objects[0].base.location_type ==
               PCX_AVT_LOCATION_TYPE_NOWHERE);
        assert(avt->objects[0].read_text == NULL);
        pcx_avt_free(avt);

        avt = expect_success(BLURB
                             "ejo j {\n"
                             "   priskribo \"j\"\n"
                             "   aĵo zingebraj_rizeroj {\n"
                             "      pluralo\n"
                             "   }\n"
                             "}\n"
                             "ejo k {\n"
                             "   priskribo \"k\"\n"
                             "}\n"
                             "aĵo blua_skatolo {\n"
                             "   kunportata\n"
                             "   ino\n"
                             "   portebla\n"
                             "   aĵo akra_tranĉilo {\n"
                             "      viro\n"
                             "   }\n"
                             "}\n"
                             "aĵo ruĝa_pilko {\n"
                             "   loko blua_skatolo\n"
                             "   priskribo verda_pilko\n"
                             "   enen k\n"
                             "}\n"
                             "aĵo verda_pilko {\n"
                             "   loko k\n"
                             "   legebla \"IKEA\"\n"
                             "   eco fermita\n"
                             "   priskribo \"pilkeca\"\n"
                             "}\n");
        assert(avt->n_objects == 5);

        assert(!strcmp(avt->objects[0].base.adjective, "zingebr"));
        assert(!strcmp(avt->objects[0].base.name, "rizer"));
        assert(avt->objects[0].base.description == NULL);
        assert(avt->objects[0].base.pronoun == PCX_AVT_PRONOUN_PLURAL);
        assert(avt->objects[0].base.location_type ==
               PCX_AVT_LOCATION_TYPE_IN_ROOM);
        assert(avt->objects[0].base.location == 0);
        assert(avt->objects[0].base.attributes ==
               PCX_AVT_OBJECT_ATTRIBUTE_PORTABLE);

        assert(!strcmp(avt->objects[1].base.adjective, "blu"));
        assert(!strcmp(avt->objects[1].base.name, "skatol"));
        assert(avt->objects[1].base.description == NULL);
        assert(avt->objects[1].base.pronoun == PCX_AVT_PRONOUN_WOMAN);
        assert(avt->objects[1].base.location_type ==
               PCX_AVT_LOCATION_TYPE_CARRYING);
        assert(avt->objects[1].base.attributes ==
               PCX_AVT_OBJECT_ATTRIBUTE_PORTABLE);

        assert(!strcmp(avt->objects[2].base.adjective, "akr"));
        assert(!strcmp(avt->objects[2].base.name, "tranĉil"));
        assert(avt->objects[2].base.description == NULL);
        assert(avt->objects[2].base.pronoun == PCX_AVT_PRONOUN_MAN);
        assert(avt->objects[2].base.location_type ==
               PCX_AVT_LOCATION_TYPE_IN_OBJECT);
        assert(avt->objects[2].base.location == 1);

        assert(!strcmp(avt->objects[3].base.adjective, "ruĝ"));
        assert(!strcmp(avt->objects[3].base.name, "pilk"));
        assert(avt->objects[3].base.pronoun == PCX_AVT_PRONOUN_ANIMAL);
        assert(avt->objects[3].base.location_type ==
               PCX_AVT_LOCATION_TYPE_IN_OBJECT);
        assert(avt->objects[3].base.location == 1);
        assert(!strcmp(avt->objects[3].base.description, "pilkeca"));
        assert(avt->objects[3].enter_room == 1);

        assert(!strcmp(avt->objects[4].base.adjective, "verd"));
        assert(!strcmp(avt->objects[4].base.name, "pilk"));
        assert(avt->objects[4].base.location_type ==
               PCX_AVT_LOCATION_TYPE_IN_ROOM);
        assert(avt->objects[4].base.location == 1);
        assert(avt->objects[4].base.description ==
               avt->objects[3].base.description);
        assert(!strcmp(avt->objects[4].read_text, "IKEA"));
        assert(avt->objects[4].base.attributes ==
               (PCX_AVT_OBJECT_ATTRIBUTE_CLOSED |
                PCX_AVT_OBJECT_ATTRIBUTE_PORTABLE));

        pcx_avt_free(avt);

        avt = expect_success(BLURB
                             "ejo nenio {\n"
                             "   priskribo \"j\"\n"
                             "}\n"
                             "aĵo blua_skatolo {\n"
                             "   alinomo \"kartono\"\n"
                             "   alinomo \"randoj\"\n"
                             "   alinomo \"bluverda skatolo\"\n"
                             "}"
                             "aĵo timiga_pupo {\n"
                             "   alinomo \"pupeto\"\n"
                             "}");
        assert(avt->n_objects == 2);

        assert(!strcmp(avt->rooms[0].name, "nenio"));

        assert(avt->objects[0].base.n_aliases == 3);

        assert(!avt->objects[0].base.aliases[0].plural);
        assert(avt->objects[0].base.aliases[0].adjective == NULL);
        assert(!strcmp(avt->objects[0].base.aliases[0].name, "karton"));

        assert(avt->objects[0].base.aliases[1].plural);
        assert(avt->objects[0].base.aliases[1].adjective == NULL);
        assert(!strcmp(avt->objects[0].base.aliases[1].name, "rand"));

        assert(!avt->objects[0].base.aliases[2].plural);
        assert(!strcmp(avt->objects[0].base.aliases[2].adjective, "bluverd"));
        assert(!strcmp(avt->objects[0].base.aliases[2].name, "skatol"));

        assert(avt->objects[1].base.n_aliases == 1);

        assert(!avt->objects[1].base.aliases[0].plural);
        assert(avt->objects[1].base.aliases[0].adjective == NULL);
        assert(!strcmp(avt->objects[1].base.aliases[0].name, "pupet"));

        pcx_avt_free(avt);

        avt = expect_success(BLURB
                             "ejo kuirejo {\n"
                             "   priskribo \"j\"\n"
                             "}\n"
                             "ejo dormĉambro {\n"
                             "   priskribo \"k\"\n"
                             "   fenomeno dormaverto {\n"
                             "      verbo \"esti\"\n"
                             "      mesaĝo \"Ne dormu!\"\n"
                             "   }\n"
                             "}\n"
                             "aĵo bruna_terpomo { }\n"
                             "aĵo blua_terpomo { }\n"
                             "aĵo verda_terpomo { }\n"
                             "aĵo ruĝa_terpomo {\n"
                             "   fenomeno {\n"
                             "      verbo \"rigardi\"\n"
                             "      mesaĝo dormaverto\n"
                             "   }\n"
                             "}\n"
                             "aĵo purpura_terpomo { }\n"
                             "teksto t12 \"Vi manĝis!\"\n"
                             "fenomeno {\n"
                             "   verbo \"manĝi\"\n"
                             "   verbo \"trinki\"\n"
                             "   verbo \"rigardi\"\n"
                             "   poentoj 128\n"
                             "   mesaĝo t12\n"
                             "\n"
                             "   aĵo io\n"
                             "   aĵo nenio\n"
                             "   aĵo ŝargo 1\n"
                             "   aĵo pezo 2\n"
                             "   aĵo grando 3\n"
                             "   aĵo enhavo 4\n"
                             "   aĵo fajrodaŭro 5\n"
                             "   aĵo bruna_terpomo\n"
                             "   aĵo adjektivo blua_terpomo\n"
                             "   aĵo nomo verda_terpomo\n"
                             "   aĵo kopio ruĝa_terpomo\n"
                             "   aĵo eco portebla\n"
                             "   aĵo eco bluba\n"
                             "   aĵo eco malvera brulanta\n"
                             "   aĵo eco malvera bluba\n"
                             "   pero enhavo 7\n"
                             "   ejo dormĉambro\n"
                             "   ejo eco luma\n"
                             "   ejo eco malvera ludfino\n"
                             "   ejo eco bluba\n"
                             "   ejo eco malvera bluba\n"
                             "   eco bluba\n"
                             "   eco malvera bluba\n"
                             "   ŝanco 56\n"
                             "   ĉeestas purpura_terpomo\n"
                             "\n"
                             "   nova aĵo nenio\n"
                             "   nova aĵo alien dormĉambro\n"
                             "   nova aĵo kunportata\n"
                             "   nova aĵo bruna_terpomo\n"
                             "   nova aĵo eco portebla\n"
                             "   nova aĵo eco malvera portebla\n"
                             "   nova aĵo adjektivo blua_terpomo\n"
                             "   nova aĵo nomo verda_terpomo\n"
                             "   nova aĵo kopio ruĝa_terpomo\n"
                             "   nova aĵo fino 89\n"
                             "   nova aĵo ŝargo 90\n"
                             "   nova aĵo pezo 91\n"
                             "   nova aĵo grando 92\n"
                             "   nova aĵo enhavo 93\n"
                             "   nova aĵo fajrodaŭro 94\n"
                             "   nova pero nenio\n"
                             "   nova ejo dormĉambro\n"
                             "   nova ejo eco luma\n"
                             "   nova ejo eco malvera luma\n"
                             "   nova eco bluba\n"
                             "   nova eco malvera bluba\n"
                             "   fenomeno dormaverto\n"
                             "   nova kunportata ruĝa_terpomo\n"
                             "   nova apero blua_terpomo\n"
                             "}");

        assert(avt->n_verbs == 4);

        assert(avt->n_rules == 3);
        const struct pcx_avt_rule *rule = avt->rules;

        assert(!strcmp(avt->verbs[0].name, "est"));
        assert(avt->verbs[0].n_rules == 1);
        assert(avt->verbs[0].rules[0] == 0);
        assert(!strcmp(rule->text, "Ne dormu!"));
        assert(rule->points == 0);
        /* Implicitly added condition because the rule is in a room */
        assert(rule->n_conditions == 6);
        assert(rule->conditions[0].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->conditions[0].condition == PCX_AVT_CONDITION_IN_ROOM);
        assert(rule->conditions[0].data == 1);
        /* Implicitly added conditions for the objects */
        assert(rule->conditions[1].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[1].condition == PCX_AVT_CONDITION_NOTHING);
        assert(rule->conditions[2].subject == PCX_AVT_RULE_SUBJECT_TOOL);
        assert(rule->conditions[2].condition == PCX_AVT_CONDITION_NOTHING);
        assert(rule->conditions[3].subject == PCX_AVT_RULE_SUBJECT_MONSTER);
        assert(rule->conditions[3].condition == PCX_AVT_CONDITION_NOTHING);
        assert(rule->conditions[4].subject == PCX_AVT_RULE_SUBJECT_DIRECTION);
        assert(rule->conditions[4].condition == PCX_AVT_CONDITION_NOTHING);
        assert(rule->conditions[5].subject == PCX_AVT_RULE_SUBJECT_IN);
        assert(rule->conditions[5].condition == PCX_AVT_CONDITION_NOTHING);

        rule++;

        assert(!strcmp(avt->verbs[1].name, "rigard"));
        assert(avt->verbs[1].n_rules == 2);
        assert(avt->verbs[1].rules[0] == 1);
        assert(avt->verbs[1].rules[1] == 2);
        assert(rule->text == avt->rules[0].text);
        assert(rule->points == 0);
        /* Implicitly added condition because the rule is in an object */
        assert(rule->n_conditions == 5);
        assert(rule->conditions[0].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[0].condition == PCX_AVT_CONDITION_OBJECT_IS);
        assert(rule->conditions[0].data == 3);
        /* Implicitly added conditions for the objects */
        assert(rule->conditions[1].subject == PCX_AVT_RULE_SUBJECT_TOOL);
        assert(rule->conditions[1].condition == PCX_AVT_CONDITION_NOTHING);
        assert(rule->conditions[2].subject == PCX_AVT_RULE_SUBJECT_MONSTER);
        assert(rule->conditions[2].condition == PCX_AVT_CONDITION_NOTHING);
        assert(rule->conditions[3].subject == PCX_AVT_RULE_SUBJECT_DIRECTION);
        assert(rule->conditions[3].condition == PCX_AVT_CONDITION_NOTHING);
        assert(rule->conditions[4].subject == PCX_AVT_RULE_SUBJECT_IN);
        assert(rule->conditions[4].condition == PCX_AVT_CONDITION_NOTHING);

        rule++;

        assert(!strcmp(avt->verbs[2].name, "manĝ"));
        assert(avt->verbs[2].n_rules == 1);
        assert(avt->verbs[2].rules[0] == 2);
        assert(!strcmp(avt->verbs[3].name, "trink"));
        assert(avt->verbs[3].n_rules == 1);
        assert(avt->verbs[3].rules[0] == 2);
        assert(!strcmp(rule->text, "Vi manĝis!"));
        assert(rule->points == 128);

        assert(rule->n_conditions == 28);
        assert(rule->conditions[0].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[0].condition == PCX_AVT_CONDITION_SOMETHING);
        assert(rule->conditions[1].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[1].condition == PCX_AVT_CONDITION_NOTHING);
        assert(rule->conditions[2].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[2].condition == PCX_AVT_CONDITION_SHOTS);
        assert(rule->conditions[2].data == 1);
        assert(rule->conditions[3].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[3].condition == PCX_AVT_CONDITION_WEIGHT);
        assert(rule->conditions[3].data == 2);
        assert(rule->conditions[4].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[4].condition == PCX_AVT_CONDITION_SIZE);
        assert(rule->conditions[4].data == 3);
        assert(rule->conditions[5].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[5].condition ==
               PCX_AVT_CONDITION_CONTAINER_SIZE);
        assert(rule->conditions[5].data == 4);
        assert(rule->conditions[6].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[6].condition == PCX_AVT_CONDITION_BURN_TIME);
        assert(rule->conditions[6].data == 5);
        assert(rule->conditions[7].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[7].condition == PCX_AVT_CONDITION_OBJECT_IS);
        assert(rule->conditions[7].data == 0);
        assert(rule->conditions[8].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[8].condition ==
               PCX_AVT_CONDITION_OBJECT_SAME_ADJECTIVE);
        assert(rule->conditions[8].data == 1);
        assert(rule->conditions[9].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[9].condition ==
               PCX_AVT_CONDITION_OBJECT_SAME_NAME);
        assert(rule->conditions[9].data == 2);
        assert(rule->conditions[10].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[10].condition ==
               PCX_AVT_CONDITION_OBJECT_SAME_NOUN);
        assert(rule->conditions[10].data == 3);
        assert(rule->conditions[11].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[11].condition ==
               PCX_AVT_CONDITION_OBJECT_ATTRIBUTE);
        assert(rule->conditions[11].data ==
               ffs(PCX_AVT_OBJECT_ATTRIBUTE_PORTABLE) - 1);
        assert(rule->conditions[12].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[12].condition ==
               PCX_AVT_CONDITION_OBJECT_ATTRIBUTE);
        assert(rule->conditions[12].data == 13);
        assert(rule->conditions[13].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[13].condition ==
               PCX_AVT_CONDITION_NOT_OBJECT_ATTRIBUTE);
        assert(rule->conditions[13].data ==
               ffs(PCX_AVT_OBJECT_ATTRIBUTE_BURNING) - 1);
        assert(rule->conditions[14].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->conditions[14].condition ==
               PCX_AVT_CONDITION_NOT_OBJECT_ATTRIBUTE);
        assert(rule->conditions[14].data == 13);
        assert(rule->conditions[15].subject == PCX_AVT_RULE_SUBJECT_TOOL);
        assert(rule->conditions[15].condition ==
               PCX_AVT_CONDITION_CONTAINER_SIZE);
        assert(rule->conditions[15].data == 7);
        assert(rule->conditions[16].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->conditions[16].condition == PCX_AVT_CONDITION_IN_ROOM);
        assert(rule->conditions[16].data == 1);
        assert(rule->conditions[17].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->conditions[17].condition ==
               PCX_AVT_CONDITION_ROOM_ATTRIBUTE);
        assert(rule->conditions[17].data ==
               ffs(PCX_AVT_ROOM_ATTRIBUTE_LIT) - 1);
        assert(rule->conditions[18].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->conditions[18].condition ==
               PCX_AVT_CONDITION_NOT_ROOM_ATTRIBUTE);
        assert(rule->conditions[18].data ==
               ffs(PCX_AVT_ROOM_ATTRIBUTE_GAME_OVER) - 1);
        assert(rule->conditions[19].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->conditions[19].condition ==
               PCX_AVT_CONDITION_ROOM_ATTRIBUTE);
        assert(rule->conditions[19].data == 4);
        assert(rule->conditions[20].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->conditions[20].condition ==
               PCX_AVT_CONDITION_NOT_ROOM_ATTRIBUTE);
        assert(rule->conditions[20].data == 4);
        assert(rule->conditions[21].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->conditions[21].condition ==
               PCX_AVT_CONDITION_PLAYER_ATTRIBUTE);
        assert(rule->conditions[21].data == 1);
        assert(rule->conditions[22].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->conditions[22].condition ==
               PCX_AVT_CONDITION_NOT_PLAYER_ATTRIBUTE);
        assert(rule->conditions[22].data == 1);
        assert(rule->conditions[23].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->conditions[23].condition == PCX_AVT_CONDITION_CHANCE);
        assert(rule->conditions[23].data == 56);
        assert(rule->conditions[24].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->conditions[24].condition ==
               PCX_AVT_CONDITION_ANOTHER_OBJECT_PRESENT);
        assert(rule->conditions[24].data == 4);
        /* Implicitly added condition */
        assert(rule->conditions[25].subject == PCX_AVT_RULE_SUBJECT_MONSTER);
        assert(rule->conditions[25].condition == PCX_AVT_CONDITION_NOTHING);
        assert(rule->conditions[26].subject == PCX_AVT_RULE_SUBJECT_DIRECTION);
        assert(rule->conditions[26].condition == PCX_AVT_CONDITION_NOTHING);
        assert(rule->conditions[27].subject == PCX_AVT_RULE_SUBJECT_IN);
        assert(rule->conditions[27].condition == PCX_AVT_CONDITION_NOTHING);

        assert(rule->n_actions == 24);
        assert(rule->actions[0].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[0].action == PCX_AVT_ACTION_NOTHING);
        assert(rule->actions[1].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[1].action == PCX_AVT_ACTION_MOVE_TO);
        assert(rule->actions[1].data == 1);
        assert(rule->actions[2].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[2].action == PCX_AVT_ACTION_CARRY);
        assert(rule->actions[3].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[3].action == PCX_AVT_ACTION_REPLACE_OBJECT);
        assert(rule->actions[3].data == 0);
        assert(rule->actions[4].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[4].action == PCX_AVT_ACTION_SET_OBJECT_ATTRIBUTE);
        assert(rule->actions[4].data ==
               ffs(PCX_AVT_OBJECT_ATTRIBUTE_PORTABLE) - 1);
        assert(rule->actions[5].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[5].action ==
               PCX_AVT_ACTION_UNSET_OBJECT_ATTRIBUTE);
        assert(rule->actions[5].data ==
               ffs(PCX_AVT_OBJECT_ATTRIBUTE_PORTABLE) - 1);
        assert(rule->actions[6].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[6].action ==
               PCX_AVT_ACTION_CHANGE_OBJECT_ADJECTIVE);
        assert(rule->actions[6].data == 1);
        assert(rule->actions[7].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[7].action ==
               PCX_AVT_ACTION_CHANGE_OBJECT_NAME);
        assert(rule->actions[7].data == 2);
        assert(rule->actions[8].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[8].action == PCX_AVT_ACTION_COPY_OBJECT);
        assert(rule->actions[8].data == 3);
        assert(rule->actions[9].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[9].action == PCX_AVT_ACTION_CHANGE_END);
        assert(rule->actions[9].data == 89);
        assert(rule->actions[10].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[10].action == PCX_AVT_ACTION_CHANGE_SHOTS);
        assert(rule->actions[10].data == 90);
        assert(rule->actions[11].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[11].action == PCX_AVT_ACTION_CHANGE_WEIGHT);
        assert(rule->actions[11].data == 91);
        assert(rule->actions[12].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[12].action == PCX_AVT_ACTION_CHANGE_SIZE);
        assert(rule->actions[12].data == 92);
        assert(rule->actions[13].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[13].action ==
               PCX_AVT_ACTION_CHANGE_CONTAINER_SIZE);
        assert(rule->actions[13].data == 93);
        assert(rule->actions[14].subject == PCX_AVT_RULE_SUBJECT_OBJECT);
        assert(rule->actions[14].action == PCX_AVT_ACTION_CHANGE_BURN_TIME);
        assert(rule->actions[14].data == 94);
        assert(rule->actions[15].subject == PCX_AVT_RULE_SUBJECT_TOOL);
        assert(rule->actions[15].action == PCX_AVT_ACTION_NOTHING);
        assert(rule->actions[16].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->actions[16].action == PCX_AVT_ACTION_MOVE_TO);
        assert(rule->actions[16].data == 1);
        assert(rule->actions[17].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->actions[17].action == PCX_AVT_ACTION_SET_ROOM_ATTRIBUTE);
        assert(rule->actions[17].data ==
               ffs(PCX_AVT_ROOM_ATTRIBUTE_LIT) - 1);
        assert(rule->actions[18].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->actions[18].action == PCX_AVT_ACTION_UNSET_ROOM_ATTRIBUTE);
        assert(rule->actions[18].data ==
               ffs(PCX_AVT_ROOM_ATTRIBUTE_LIT) - 1);
        assert(rule->actions[19].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->actions[19].action ==
               PCX_AVT_ACTION_SET_PLAYER_ATTRIBUTE);
        assert(rule->actions[19].data == 1);
        assert(rule->actions[20].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->actions[20].action ==
               PCX_AVT_ACTION_UNSET_PLAYER_ATTRIBUTE);
        assert(rule->actions[20].data == 1);
        assert(rule->actions[21].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->actions[21].action == PCX_AVT_ACTION_RUN_RULE);
        assert(rule->actions[21].data == 0);
        assert(rule->actions[22].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->actions[22].action == PCX_AVT_ACTION_CARRY_ANOTHER_OBJECT);
        assert(rule->actions[22].data == 3);
        assert(rule->actions[23].subject == PCX_AVT_RULE_SUBJECT_ROOM);
        assert(rule->actions[23].action ==
               PCX_AVT_ACTION_ANOTHER_OBJECT_APPEAR);
        assert(rule->actions[23].data == 1);

        pcx_avt_free(avt);

        if (!check_strings())
                return EXIT_FAILURE;

        return EXIT_SUCCESS;
}
