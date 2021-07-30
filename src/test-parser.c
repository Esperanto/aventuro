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

        return EXIT_SUCCESS;
}
