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

#include "pcx-parser.h"
#include "pcx-list.h"

struct load_data {
        struct pcx_source source;
        const char *data;
        size_t pos, size;
};

struct fail_check {
        const char *source;
        const char *error_message;
};

#define BLURB "nomo \"testnomo\" aŭtoro \"testaŭtoro\" jaro \"2021\"\n"

static const struct fail_check
fail_checks[] = {
        {
                "",
                "The game is missing the “nomo”"
        },
        {
                "nomo \"testnomo\"",
                "The game is missing the “aŭtoro”"
        },
        {
                "nomo \"testnomo\" aŭtoro \"testaŭtoro\"",
                "The game is missing the “jaro”"
        },
        {
                BLURB,
                "The game needs at least one room"
        },
        {
                BLURB
                "salono { }",
                "Expected room name at line 2",
        },
        {
                BLURB
                "salono salono1 { }",
                "Room is missing description at line 2",
        },
        {
                BLURB
                "salono salono1 {\n"
                "nomo \"salono unu\"\n"
                "nomo \"ankoraŭ salono unu\"\n"
                "}\n",
                "Room already has a name at line 4"
        },
        {
                BLURB
                "salono salono1 {\n"
                "priskribo \"salono unu\"\n"
                "priskribo \"ankoraŭ salono unu\"\n"
                "}\n",
                "Room already has a description at line 4"
        },
        {
                BLURB
                "salono salono1 {\n"
                "poentoj -1\n"
                "}\n",
                "Number out of range at line 3"
        },
        {
                BLURB
                "salono salono1 {\n"
                "poentoj 256\n"
                "}\n",
                "Number out of range at line 3"
        },
        {
                BLURB
                "salono salono1 {\n"
                "eco 42\n"
                "}\n",
                "Attribute name expected at line 3"
        },
        {
                BLURB
                "salono salono1 {\n"
                "eco a4 eco a5 eco a6 eco a7\n"
                "eco a8 eco a9 eco a10 eco a11\n"
                "eco a12 eco a13 eco a14 eco a15\n"
                "eco a16 eco a17 eco a18 eco a19\n"
                "eco a20 eco a21 eco a22 eco a23\n"
                "eco a24 eco a25 eco a26 eco a27\n"
                "eco a28 eco a29 eco a30 eco a31\n"
                "eco a32\n"
                "}\n",
                "Too many unique attributes at line 10"
        },
        {
                BLURB
                "salono salono1 {\n"
                "okcidenten 3\n"
                "}\n",
                "Item name expected at line 3"
        },
        {
                BLURB
                "salono salono1 {\n"
                "okcidenten salono1\n"
                "okcidenten salono1\n"
                "}\n",
                "Room already has a west direction at line 4"
        },
        {
                BLURB
                "salono salono1\n"
                "{ priskribo \"!\" }\n"
                "salono salono1\n"
                "{ priskribo \"!\" }\n",
                "Same name used for multiple objects at line 4",
        },
        {
                BLURB
                "salono salono1 { priskribo 3 }",
                "Expected text item at line 2"
        },
        {
                BLURB
                "salono salono1 {\n"
                "3\n"
                "}",
                "Expected room item or ‘}’ at line 3"
        },
        {
                BLURB
                "salono salono1 {\n",
                "Expected room item or ‘}’ at line 3"
        },
        {
                "teksto 3",
                "Expected text name at line 1"
        },
        {
                "teksto t\n"
                "3",
                "Expected string at line 2"
        },
        {
                "nomo t\n",
                "String expected at line 1"
        },
        {
                "# This is a comment\n"
                "3",
                "Expected toplevel item at line 2"
        },
        {
                BLURB
                "salono salono1 { priskribo salono2 }\n"
                "salono salono2 { priskribo salono3 }\n"
                "salono salono3 { priskribo salono1 }\n",
                "Cyclic reference detected at line 2"
        },
        {
                BLURB
                "salono salono1 { priskribo salono1 }\n",
                "Cyclic reference detected at line 2"
        },
        {
                BLURB
                "salono salono1 { priskribo salono3 }\n",
                "Invalid text reference at line 2"
        },
        {
                BLURB
                "salono salono1 {\n"
                " priskribo \"j\"\n"
                " norden wibble\n"
                "}\n",
                "Invalid room reference on line 4"
        },
        {
                BLURB
                "salono salono1 {\n"
                " priskribo \"j\"\n"
                " norden t\n"
                "}\n"
                "teksto t \"not a room\"\n",
                "Invalid room reference on line 4"
        },
        {
                "teksto t \"what \x80\"",
                "String contains invalid UTF-8 at line 1"
        },
        {
                "salono s\x80t",
                "Invalid UTF-8 encountered at line 1"
        },
        {
                "salono s {\n"
                "(\n",
                "Unexpected character ‘(’ on line 2"
        },
        {
                "42s",
                "Invalid number “42s” on line 1"
        },
        {
                "\"\\n\"",
                "Invalid escape sequence in string on line 1"
        },
        {
                "\"\\",
                "Invalid escape sequence in string on line 1"
        },
        {
                BLURB
                "salono s1 {\n"
                " priskribo \"I forgot to close the door\n"
                "}\n",
                "Unterminated string sequence at line 3"
        },
        {
                BLURB
                "salono s1 {\n"
                " direkto potato potato \"potato\"\n"
                "}\n",
                "Expected direction name at line 3"
        },
        {
                BLURB
                "salono s1 {\n"
                " direkto \"potat\" potato \"potato\"\n"
                "}\n",
                "Direction name must be a noun at line 3"
        },
        {
                BLURB
                "salono s1 {\n"
                " direkto \"granda potato\" potato \"potato\"\n"
                "}\n",
                "Direction name must be a noun at line 3"
        },
        {
                BLURB
                "salono s1 {\n"
                " direkto \"\" potato \"potato\"\n"
                "}\n",
                "Direction name must be a noun at line 3"
        },
        {
                BLURB
                "salono s1 {\n"
                " direkto \"potato\" 3 \"potato\"\n"
                "}\n",
                "Expected room name at line 3"
        },
        {
                BLURB
                "salono s1 {\n"
                " direkto \"potato\" s2 \"potato\"\n"
                " priskribo \"j\"\n"
                "}\n",
                "Invalid room reference on line 3"
        },
        {
                BLURB
                "salono s1 {\n"
                " direkto \"potato\" s1 s3\n"
                " priskribo \"j\"\n"
                "}\n",
                "Invalid text reference at line 3"
        },
        {
                "aĵo skribilo {\n"
                " viro\n"
                " ino\n"
                "}",
                "Pronoun already specified at line 3"
        },
        {
                "aĵo\n"
                " {\n",
                "Expected object name at line 2"
        },
        {
                "aĵo skribilo\n"
                " { (\n",
                "Unexpected character ‘(’ on line 2"
        },
        {
                "aĵo skribilo {\n"
                " poentoj \"6\" \n"
                "}\n",
                "Number expected at line 2"
        },
        {
                "aĵo skribilo {\n"
                " salono { }\n"
                "}\n",
                "Expected object item or ‘}’ at line 2"
        },
        {
                "salono s1 { aĵo skribilo {\n"
                " salono { }\n"
                "} }\n",
                "Expected object item or ‘}’ at line 2"
        },
        {
                BLURB
                "salono s1 { priskribo \"j\" }\n"
                "aĵo ruza_juvelo {\n"
                "  loko t1\n"
                "}\n"
                "teksto t1 \"hi\"\n",
                "Invalid location reference on line 4"
        },
        {
                BLURB
                "salono s1 { priskribo \"j\" }\n"
                "aĵo ruza_juvelo {\n"
                "  loko t1\n"
                "}\n",
                "Invalid location reference on line 4"
        },
        {
                BLURB
                "salono s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "}\n",
                "The object name must be an adjective followed by a noun "
                "at line 3"
        },
        {
                BLURB
                "salono s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝo juvelo\"\n"
                "}\n",
                "The object name must be an adjective followed by a noun "
                "at line 3"
        },
        {
                BLURB
                "salono s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"a juvelo\"\n"
                "}\n",
                "The object name must be an adjective followed by a noun "
                "at line 3"
        },
        {
                BLURB
                "salono s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝa juvel\"\n"
                "}\n",
                "The object name must be an adjective followed by a noun "
                "at line 3"
        },
        {
                BLURB
                "salono s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝa \"\n"
                "}\n",
                "The object name must be an adjective followed by a noun "
                "at line 3"
        },
        {
                BLURB
                "salono s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝaj juvelo\"\n"
                "}\n",
                "Object’s adjective and noun don’t have the same plurality "
                "at line 3"
        },
        {
                BLURB
                "salono s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝaj &juveloj\"\n"
                "}\n",
                "The object name must be an adjective followed by a noun "
                "at line 3"
        },
        {
                BLURB
                "salono s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝaj juveloj\"\n"
                "  besto\n"
                "}\n",
                "Object has a plural name but a non-plural pronoun "
                "at line 3"
        },
        {
                BLURB
                "salono s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝaj juveloj\"\n"
                "  priskribo t\n"
                "}\n",
                "Invalid text reference at line 5"
        },
        {
                BLURB
                "salono s1 { priskribo \"j\" }\n"
                "aĵo juvelo {\n"
                "  nomo \"ruĝaj juveloj\"\n"
                "  legebla t\n"
                "}\n",
                "Invalid text reference at line 5"
        },
        {
                BLURB
                "salono s1 { priskribo \"j\" }\n"
                "aĵo ruĝa_juvelo {\n"
                "   enen t\n"
                "}\n",
                "Invalid room reference on line 4"
        },
        {
                BLURB
                "salono s1 {\n"
                "  priskribo \"j\"\n"
                "  aĵo ruĝa_juvelo {\n"
                "    kunportata\n"
                "  }\n"
                "}\n",
                "Object is marked as carried but it also has a location "
                "at line 4"
        },
        {
                "aĵo a { aĵo b{",
                "Expected object item or ‘}’ at line 1"
        },
        {
                "aĵo a { alinomo \"a\" }",
                "Alias name must be a noun at line 1"
        },
        {
                "aĵo a { alinomo \"fluga\" }",
                "Alias name must be a noun at line 1"
        },
        {
                "aĵo a { alinomo \"fluga aviadilo\" }",
                "Alias name must be a noun at line 1"
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
                             "salono salono1 { priskribo salono2 }\n"
                             "salono salono2 { priskribo la_nomo }\n"
                             "teksto la_nomo \"hi\"\n");
        assert(avt->n_rooms == 2);
        assert(avt->rooms[0].description == avt->rooms[1].description);
        assert(!strcmp(avt->rooms[0].description, "hi"));
        assert(!strcmp(avt->rooms[0].name, "salono1"));
        assert(avt->rooms[0].points == 0);
        assert(avt->rooms[0].attributes == 0);
        assert(!strcmp(avt->rooms[1].name, "salono2"));
        assert(!strcmp(avt->name, "testnomo"));
        assert(!strcmp(avt->author, "testaŭtoro"));
        assert(!strcmp(avt->year, "2021"));
        assert(avt->introduction == NULL);
        pcx_avt_free(avt);

        avt = expect_success(BLURB
                             "salono ruĝa_salono { priskribo \"j\" }\n");
        assert(avt->n_rooms == 1);
        assert(!strcmp(avt->rooms[0].description, "j"));
        assert(!strcmp(avt->rooms[0].name, "ruĝa salono"));
        pcx_avt_free(avt);

        avt = expect_success(BLURB
                             "salono salono1 {\n"
                             " norden n\n"
                             " orienten e\n"
                             " suden s\n"
                             " okcidenten w\n"
                             " supren up\n"
                             " suben down\n"
                             " elen out\n"
                             " priskribo t\n"
                             "}\n"
                             "salono n { priskribo t }\n"
                             "salono e { priskribo t }\n"
                             "salono s { priskribo t }\n"
                             "salono w { priskribo t }\n"
                             "salono up { priskribo t }\n"
                             "salono down { priskribo t }\n"
                             "salono out { priskribo t }\n"
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
                             "salono ruĝa_salono {\n"
                             " priskribo \"j\"\n"
                             " nomo \"ŝanĝita nomo\"\n"
                             " poentoj 42\n"
                             " luma\n"
                             " nelumigebla\n"
                             " ludfino\n"
                             " eco ruĝa\n"
                             "}\n"
                             "salono verda_salono {\n"
                             " priskribo \"  j  \"\n"
                             " eco verda\n"
                             " poentoj 4_2"
                             "}\n"
                             "salono verdruĝa_salono {\n"
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
                             "salono vendejaro {\n"
                             " priskribo \"Multe da vendejoj\"\n"
                             " direkto \"librejo\" librejo\n"
                             "         \"Ĝi estas brokanta librejo.\"\n"
                             " direkto \"gitaroj\" muzikejo nenio\n"
                             "}\n"
                             "salono librejo {\n"
                             " priskribo \"Estas nur 3 libroj aĉeteblaj.\"\n"
                             " direkto \"koridoro\" vendejaro vendejaro\n"
                             "}\n"
                             "salono muzikejo {\n"
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
                             "salono j { priskribo \"j\" }\n"
                             "aĵo skribilo {\n"
                             "   priskribo \"Ĝi estas skribilo.\"\n"
                             "   nomo \"blua skribilo\"\n"
                             "   besto\n"
                             "   poentoj 1\n"
                             "   pezo 2\n"
                             "   grando 3\n"
                             "   enhavo 4\n"
                             "   perpafi 5\n"
                             "   ŝargo 6\n"
                             "   perbati 7\n"
                             "   perpiki 8\n"
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
               (PCX_AVT_OBJECT_ATTRIBUTE_PORTABLE |
                PCX_AVT_OBJECT_ATTRIBUTE_CLOSABLE |
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
                             "salono j {\n"
                             "   priskribo \"j\"\n"
                             "   aĵo zingebraj_rizeroj {\n"
                             "      pluralo\n"
                             "   }\n"
                             "}\n"
                             "salono k {\n"
                             "   priskribo \"k\"\n"
                             "}\n"
                             "aĵo blua_skatolo {\n"
                             "   kunportata\n"
                             "   ino\n"
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

        assert(!strcmp(avt->objects[1].base.adjective, "blu"));
        assert(!strcmp(avt->objects[1].base.name, "skatol"));
        assert(avt->objects[1].base.description == NULL);
        assert(avt->objects[1].base.pronoun == PCX_AVT_PRONOUN_WOMAN);
        assert(avt->objects[1].base.location_type ==
               PCX_AVT_LOCATION_TYPE_CARRYING);

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

        pcx_avt_free(avt);

        avt = expect_success(BLURB
                             "salono j {\n"
                             "   priskribo \"j\"\n"
                             "}\n"
                             "aĵo blua_skatolo {\n"
                             "   alinomo \"kartono\"\n"
                             "   alinomo \"randoj\"\n"
                             "}"
                             "aĵo timiga_pupo {\n"
                             "   alinomo \"pupeto\"\n"
                             "}");
        assert(avt->n_aliases == 3);

        assert(avt->aliases[0].type == PCX_AVT_ALIAS_TYPE_OBJECT);
        assert(!avt->aliases[0].plural);
        assert(avt->aliases[0].index == 0);
        assert(!strcmp(avt->aliases[0].name, "karton"));

        assert(avt->aliases[1].type == PCX_AVT_ALIAS_TYPE_OBJECT);
        assert(avt->aliases[1].plural);
        assert(avt->aliases[1].index == 0);
        assert(!strcmp(avt->aliases[1].name, "rand"));

        assert(avt->aliases[2].type == PCX_AVT_ALIAS_TYPE_OBJECT);
        assert(!avt->aliases[2].plural);
        assert(avt->aliases[2].index == 1);
        assert(!strcmp(avt->aliases[2].name, "pupet"));

        pcx_avt_free(avt);

        return EXIT_SUCCESS;
}
