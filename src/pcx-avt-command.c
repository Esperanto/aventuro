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

#include "pcx-avt-command.h"

#include <string.h>

#include "pcx-utf8.h"

struct parse_pos {
        const char *p;
};

struct noun_part {
        bool accusative;
        bool plural;
        bool adjective;
        struct pcx_avt_command_word word;
};

static bool
is_alphabetic(uint32_t ch)
{
        if (ch >= 'a' && ch <= 'z')
                return true;

        if (ch >= 'A' && ch <= 'Z')
                return true;

        switch (ch) {
        case 0x0125: /* ĥ */
        case 0x015d: /* ŝ */
        case 0x011d: /* ĝ */
        case 0x0109: /* ĉ */
        case 0x0135: /* ĵ */
        case 0x016d: /* ŭ */
        case 0x0124: /* Ĥ */
        case 0x015c: /* Ŝ */
        case 0x011c: /* Ĝ */
        case 0x0108: /* Ĉ */
        case 0x0134: /* Ĵ */
        case 0x016c: /* Ŭ */
                return true;
        }

        return false;
}

static uint32_t
to_lower(uint32_t ch)
{
        if (ch >= 'A' && ch <= 'Z')
                return ch - 'A' + 'a';

        switch (ch) {
        case 0x0124: /* Ĥ */
        case 0x015c: /* Ŝ */
        case 0x011c: /* Ĝ */
        case 0x0108: /* Ĉ */
        case 0x0134: /* Ĵ */
        case 0x016c: /* Ŭ */
                return ch + 1;
        }

        return ch;
}

static bool
is_word(const char *word,
        size_t length,
        const char *test)
{
        while (length > 0) {
                if (*test == '\0')
                        return false;

                uint32_t ch = to_lower(pcx_utf8_get_char(word));
                const char *next = pcx_utf8_next(word);

                length -= next - word;
                word = next;

                if (length > 0 && (*word == 'x' || *word == 'X')) {
                        switch (ch) {
                        case 'h':
                                ch = 0x0125; /* ĥ */
                                break;
                        case 's':
                                ch = 0x015d; /* ŝ */
                                break;
                        case 'g':
                                ch = 0x011d; /* ĝ */
                                break;
                        case 'c':
                                ch = 0x0109; /* ĉ */
                                break;
                        case 'j':
                                ch = 0x0135; /* ĵ */
                                break;
                        case 'u':
                                ch = 0x016d; /* ŭ */
                                break;
                        default:
                                goto not_hat;
                        }

                        word++;
                        length--;

                not_hat:
                        (void) 0;
                }

                if (pcx_utf8_get_char(test) != ch)
                        return false;

                test = pcx_utf8_next(test);
        }

        return *test == '\0';
}

static bool
get_next_word(struct parse_pos *pos,
              struct pcx_avt_command_word *word)
{
        const char *p = pos->p;

        while (*p == ' ')
                p++;

        word->start = p;

        while (*p) {
                uint32_t ch = pcx_utf8_get_char(p);

                if (!is_alphabetic(ch))
                        break;

                p = pcx_utf8_next(p);
        }

        if (*p != '\0' && *p != ' ')
                return false;

        if (p == word->start)
                return false;

        word->length = p - word->start;
        pos->p = p;

        return true;
}

static bool
parse_noun_part(struct parse_pos *pos_in_out,
                struct noun_part *part)
{
        struct parse_pos pos = *pos_in_out;

        if (!get_next_word(&pos, &part->word))
                return false;

        if (to_lower(part->word.start[part->word.length - 1]) == 'n') {
                part->accusative = true;
                part->word.length--;
                if (part->word.length < 1)
                        return false;
        } else {
                part->accusative = false;
        }

        if (to_lower(part->word.start[part->word.length - 1]) == 'j') {
                part->plural = true;
                part->word.length--;
                if (part->word.length < 1)
                        return false;
        } else {
                part->plural = false;
        }

        if (part->word.length < 2)
                return false;

        switch (to_lower(part->word.start[part->word.length - 1])) {
        case 'a':
                part->adjective = true;
                break;
        case 'o':
                part->adjective = false;
                break;
        default:
                return false;
        }

        *pos_in_out = pos;
        part->word.length--;

        return true;
}

static bool
parse_noun(struct parse_pos *pos_in_out,
           struct pcx_avt_command_noun *noun)
{
        struct parse_pos pos = *pos_in_out;
        struct pcx_avt_command_word word;

        if (!get_next_word(&pos, &word))
                return false;

        if (is_word(word.start, word.length, "la")) {
                noun->article = true;
        } else {
                noun->article = false;
                pos = *pos_in_out;
        }

        struct noun_part part1;

        if (!parse_noun_part(&pos, &part1))
                return false;

        noun->accusative = part1.accusative;
        noun->plural = part1.plural;

        struct noun_part part2;
        struct parse_pos after_part2 = pos;

        if (parse_noun_part(&after_part2, &part2) &&
            part1.accusative == part2.accusative) {
                if (part1.adjective == part2.adjective ||
                    part1.plural != part2.plural)
                        return false;

                const struct noun_part *adjective, *name;

                if (part1.adjective) {
                        adjective = &part1;
                        name = &part2;
                } else {
                        adjective = &part2;
                        name = &part1;
                }

                noun->name = name->word;
                noun->adjective = adjective->word;
                pos = after_part2;
        } else if (part1.adjective) {
                return false;
        } else {
                noun->name = part1.word;
                noun->adjective.start = NULL;
                noun->adjective.length = 0;
        }

        *pos_in_out = pos;

        return true;
}

static bool
parse_verb(struct parse_pos *pos_in_out,
           struct pcx_avt_command_word *word_out)
{
        struct parse_pos pos = *pos_in_out;
        struct pcx_avt_command_word word;

        if (!get_next_word(&pos, &word))
                return false;

        if (word.length < 2)
                return false;

        switch (to_lower(word.start[word.length - 1])) {
        case 'u':
                word.length--;
                break;
        case 's':
                if (word.length < 3)
                        return false;
                switch (to_lower(word.start[word.length - 2])) {
                case 'o':
                case 'a':
                        break;
                default:
                        return false;
                }
                word.length -= 2;
                break;
        default:
                return false;
        }

        *word_out = word;
        *pos_in_out = pos;

        return true;
}

static bool
parse_tool(struct parse_pos *pos_in_out,
           struct pcx_avt_command_noun *noun_out)
{
        struct parse_pos pos = *pos_in_out;
        struct pcx_avt_command_noun noun;

        if (!get_next_word(&pos, &noun.name))
                return false;

        if (!is_word(noun.name.start, noun.name.length, "per"))
                return false;

        if (!parse_noun(&pos, &noun))
                return false;

        if (noun.accusative)
                return false;

        *pos_in_out = pos;
        *noun_out = noun;

        return true;
}

static bool
parse_direction(struct parse_pos *pos_in_out,
                struct pcx_avt_command_word *word_out)
{
        struct parse_pos pos = *pos_in_out;
        struct pcx_avt_command_noun noun;

        if (!get_next_word(&pos, &noun.name))
                return false;

        /* Accept directions like “maren” for “al la maro” */
        if (noun.name.length > 2 &&
            is_word(noun.name.start + noun.name.length - 2, 2, "en")) {
                *pos_in_out = pos;
                *word_out = noun.name;
                word_out->length -= 2;
                return true;
        }

        if (!is_word(noun.name.start, noun.name.length, "al"))
                return false;

        if (!parse_noun(&pos, &noun) ||
            noun.accusative ||
            noun.adjective.start)
                return false;

        *word_out = noun.name;
        *pos_in_out = pos;

        return true;
}

static bool
parse_in(struct parse_pos *pos_in_out,
         struct pcx_avt_command_noun *noun_out)
{
        struct parse_pos pos = *pos_in_out;
        struct pcx_avt_command_noun noun;

        if (!get_next_word(&pos, &noun.name))
                return false;

        if (!is_word(noun.name.start, noun.name.length, "en"))
                return false;

        if (!parse_noun(&pos, &noun))
                return false;

        /* The parser is deliberately lenient about forgetting the
         * accusative after “en”.
         */

        *noun_out = noun;
        *pos_in_out = pos;

        return true;
}

bool
pcx_avt_command_parse(const char *text,
                      struct pcx_avt_command *command)
{
        struct parse_pos pos = { .p = text };

        memset(command, 0, sizeof *command);

        while (true) {
                while (*pos.p == ' ')
                        pos.p++;

                if (*pos.p == '\0')
                        break;

                if (parse_verb(&pos, &command->verb)) {
                        if (command->has_verb)
                                return false;
                        command->has_verb = true;
                        continue;
                }

                struct pcx_avt_command_noun noun;

                if (parse_noun(&pos, &noun)) {
                        if (noun.accusative) {
                                if (command->has_object)
                                        return false;
                                command->has_object = true;
                                command->object = noun;
                                continue;
                        } else {
                                if (command->has_subject)
                                        return false;
                                command->has_subject = true;
                                command->subject = noun;
                                continue;
                        }
                }

                if (parse_tool(&pos, &command->tool)) {
                        if (command->has_tool)
                                return false;
                        command->has_tool = true;
                        continue;
                }

                if (parse_direction(&pos, &command->direction)) {
                        if (command->has_direction)
                                return false;
                        command->has_direction = true;
                        continue;
                }

                if (parse_in(&pos, &command->in)) {
                        if (command->has_in)
                                return false;
                        command->has_in = true;
                        continue;
                }

                return false;
        }

        return true;
}
