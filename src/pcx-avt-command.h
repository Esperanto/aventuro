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

struct pcx_avt_command_word {
        const char *start;
        size_t length;
};

struct pcx_avt_command_noun {
        bool article;
        bool plural;
        bool accusative;
        struct pcx_avt_command_word name;
        /* Can be NULL */
        struct pcx_avt_command_word adjective;
};

struct pcx_avt_command {
        bool has_subject;
        bool has_object;
        bool has_tool;
        bool has_direction;
        bool has_in;
        bool has_verb;

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

#endif /* PCX_AVT_COMMAND */
