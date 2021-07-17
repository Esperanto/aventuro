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

#ifndef PCX_AVT_COMMAND
#define PCX_AVT_COMMAND

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include "pcx-avt.h"

/* Parts of speech that the command can have */
enum pcx_avt_command_has {
        PCX_AVT_COMMAND_HAS_SUBJECT = (1 << 0),
        PCX_AVT_COMMAND_HAS_OBJECT = (1 << 1),
        PCX_AVT_COMMAND_HAS_TOOL = (1 << 2),
        PCX_AVT_COMMAND_HAS_DIRECTION = (1 << 3),
        PCX_AVT_COMMAND_HAS_IN = (1 << 4),
        PCX_AVT_COMMAND_HAS_VERB = (1 << 5),
};

struct pcx_avt_command_word {
        const char *start;
        size_t length;
};

enum pcx_avt_command_gender {
        PCX_AVT_COMMAND_GENDER_MAN = (1 << 0),
        PCX_AVT_COMMAND_GENDER_WOMAN = (1 << 1),
        PCX_AVT_COMMAND_GENDER_THING = (1 << 2),
};

struct pcx_avt_command_pronoun {
        int person; /* 1, 2 or 3 */
        int genders;
        bool plural;
};

struct pcx_avt_command_noun {
        bool article;
        bool plural;
        bool accusative;
        bool is_pronoun;

        struct pcx_avt_command_word name;
        /* Can be NULL */
        struct pcx_avt_command_word adjective;

        /* Only used if is_pronoun is true */
        struct pcx_avt_command_pronoun pronoun;
};

struct pcx_avt_command {
        enum pcx_avt_command_has has;

        struct pcx_avt_command_noun subject;
        struct pcx_avt_command_noun object;
        struct pcx_avt_command_noun tool;
        struct pcx_avt_command_word direction;
        struct pcx_avt_command_noun in;
        struct pcx_avt_command_word verb;
};

bool
pcx_avt_command_parse(const char *text,
                      struct pcx_avt_command *command);

bool
pcx_avt_command_word_equal(const struct pcx_avt_command_word *word1,
                           const char *word2);

#endif /* PCX_AVT_COMMAND */
