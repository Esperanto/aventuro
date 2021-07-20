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
#include "pcx-util.h"
#include "pcx-avt-hat.h"

struct parse_pos {
        const char *p;
};

struct noun_part {
        bool accusative;
        bool plural;
        bool adjective;
        bool is_pronoun;
        struct pcx_avt_command_pronoun pronoun;
        struct pcx_avt_command_word word;
};

struct pronoun_name {
        const char *word;
        struct pcx_avt_command_pronoun value;
};

static const struct pronoun_name
pronoun_names[] = {
        {
                "mi",
                {
                        .person = 1,
                        PCX_AVT_COMMAND_GENDER_MAN |
                        PCX_AVT_COMMAND_GENDER_WOMAN |
                        PCX_AVT_COMMAND_GENDER_THING,
                        .plural = false,
                },
        },
        {
                "ni",
                {
                        .person = 1,
                        PCX_AVT_COMMAND_GENDER_MAN |
                        PCX_AVT_COMMAND_GENDER_WOMAN |
                        PCX_AVT_COMMAND_GENDER_THING,
                        .plural = true,
                },
        },
        {
                "vi",
                {
                        .person = 2,
                        PCX_AVT_COMMAND_GENDER_MAN |
                        PCX_AVT_COMMAND_GENDER_WOMAN |
                        PCX_AVT_COMMAND_GENDER_THING,
                        .plural = false,
                },
        },
        {
                "li",
                {
                        .person = 3,
                        PCX_AVT_COMMAND_GENDER_MAN,
                        .plural = false,
                },
        },
        {
                "ŝi",
                {
                        .person = 3,
                        PCX_AVT_COMMAND_GENDER_WOMAN,
                        .plural = false,
                },
        },
        {
                "ĝi",
                {
                        .person = 3,
                        PCX_AVT_COMMAND_GENDER_THING,
                        .plural = false,
                },
        },
        {
                "ri",
                {
                        .person = 3,
                        PCX_AVT_COMMAND_GENDER_MAN |
                        PCX_AVT_COMMAND_GENDER_WOMAN,
                        .plural = false,
                },
        },
        {
                "ili",
                {
                        .person = 3,
                        PCX_AVT_COMMAND_GENDER_MAN |
                        PCX_AVT_COMMAND_GENDER_WOMAN |
                        PCX_AVT_COMMAND_GENDER_THING,
                        .plural = true,
                },
        },
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

static bool
is_word(const char *word,
        size_t length,
        const char *test)
{
        struct pcx_avt_hat_iter iter;

        pcx_avt_hat_iter_init(&iter, word, length);

        while (!pcx_avt_hat_iter_finished(&iter)) {
                if (*test == '\0')
                        return false;

                uint32_t ch = pcx_avt_hat_iter_next(&iter);

                if (pcx_avt_hat_to_lower(pcx_utf8_get_char(test)) !=
                    pcx_avt_hat_to_lower(ch))
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

        const char *word = part->word.start;

        if (pcx_avt_hat_to_lower(word[part->word.length - 1]) == 'n') {
                part->accusative = true;
                part->word.length--;
                if (part->word.length < 1)
                        return false;
        } else {
                part->accusative = false;
        }

        if (pcx_avt_hat_to_lower(word[part->word.length - 1]) == 'j') {
                part->plural = true;
                part->word.length--;
                if (part->word.length < 1)
                        return false;
        } else {
                part->plural = false;
        }

        if (part->word.length < 2)
                return false;

        switch (pcx_avt_hat_to_lower(word[part->word.length - 1])) {
        case 'a':
                /* “La” is not an adjective */
                if (is_word(word, part->word.length, "la"))
                        return false;
                part->adjective = true;
                part->is_pronoun = false;
                part->word.length--;
                break;
        case 'o':
                part->adjective = false;
                part->is_pronoun = false;
                part->word.length--;
                break;
        default:
                if (part->plural)
                        return false;

                for (int i = 0; i < PCX_N_ELEMENTS(pronoun_names); i++) {
                        if (is_word(part->word.start,
                                    part->word.length,
                                    pronoun_names[i].word)) {
                                part->pronoun = pronoun_names[i].value;
                                goto found;
                        }
                }

                return false;

        found:
                part->adjective = false;
                part->is_pronoun = true;
                part->plural = part->pronoun.plural;
                break;
        }

        *pos_in_out = pos;

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
        const struct noun_part *adjective, *name;

        if (parse_noun_part(&after_part2, &part2) &&
            part1.accusative == part2.accusative &&
            part1.adjective != part2.adjective &&
            part1.plural == part2.plural) {
                if (part1.adjective) {
                        adjective = &part1;
                        name = &part2;
                } else {
                        adjective = &part2;
                        name = &part1;
                }

                pos = after_part2;
        } else if (part1.adjective) {
                return false;
        } else {
                name = &part1;
                adjective = NULL;
        }

        noun->name = name->word;
        noun->is_pronoun = name->is_pronoun;
        if (noun->is_pronoun) {
                if (noun->article)
                        return false;

                noun->pronoun = name->pronoun;
        }

        if (adjective) {
                if (noun->is_pronoun)
                        return false;

                noun->adjective = adjective->word;
        } else {
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

        switch (pcx_avt_hat_to_lower(word.start[word.length - 1])) {
        case 'i':
        case 'u':
                word.length--;
                break;
        case 's':
                if (word.length < 3)
                        return false;
                switch (pcx_avt_hat_to_lower(word.start[word.length - 2])) {
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

static const char *
handle_shortcut(const char *text)
{
        while (*text == ' ')
                text++;

        int len = strlen(text);

        while (len > 0 && text[len - 1] == ' ')
                len--;

        static const struct {
                const char *in;
                const char *out;
        } shortcuts[] = {
                { "n", "mi iras norden" },
                { "s", "mi iras suden" },
                { "o", "mi iras orienten" },
                { "okc", "mi iras okcidenten" },
                { "e", "mi iras orienten" },
                { "u", "mi iras okcidenten" },
                { "ĉ", "mi ĉesas" },
                { "h", "helpu min" },
                { "r", "rigardu" },
                { "v", "mi vidas" },
        };

        for (int i = 0; i < PCX_N_ELEMENTS(shortcuts); i++) {
                if (is_word(text, len, shortcuts[i].in))
                        return shortcuts[i].out;
        }

        return text;
}

static bool
is_end(const char *p)
{
        /* Skip any punctuation */
        switch (*p) {
        case '.':
        case '?':
        case '!':
                p++;
        }

        while (*p == ' ')
                p++;

        return *p == '\0';
}

bool
pcx_avt_command_parse(const char *text,
                      struct pcx_avt_command *command)
{
        text = handle_shortcut(text);

        struct parse_pos pos = { .p = text };

        memset(command, 0, sizeof *command);

        while (true) {
                while (*pos.p == ' ')
                        pos.p++;

                if (is_end(pos.p))
                        break;

                struct pcx_avt_command_noun noun;

                if (parse_noun(&pos, &noun)) {
                        if (noun.accusative) {
                                if ((command->has & PCX_AVT_COMMAND_HAS_OBJECT))
                                        return false;
                                command->has |= PCX_AVT_COMMAND_HAS_OBJECT;
                                command->object = noun;
                                continue;
                        } else {
                                if ((command->has &
                                     PCX_AVT_COMMAND_HAS_SUBJECT))
                                        return false;
                                command->has |= PCX_AVT_COMMAND_HAS_SUBJECT;
                                command->subject = noun;
                                continue;
                        }
                }

                if (parse_verb(&pos, &command->verb)) {
                        if ((command->has & PCX_AVT_COMMAND_HAS_VERB))
                                return false;
                        command->has |= PCX_AVT_COMMAND_HAS_VERB;
                        continue;
                }

                if (parse_tool(&pos, &command->tool)) {
                        if ((command->has & PCX_AVT_COMMAND_HAS_TOOL))
                                return false;
                        command->has |= PCX_AVT_COMMAND_HAS_TOOL;
                        continue;
                }

                if (parse_direction(&pos, &command->direction)) {
                        if ((command->has & PCX_AVT_COMMAND_HAS_DIRECTION))
                                return false;
                        command->has |= PCX_AVT_COMMAND_HAS_DIRECTION;
                        continue;
                }

                if (parse_in(&pos, &command->in)) {
                        if ((command->has & PCX_AVT_COMMAND_HAS_IN))
                                return false;
                        command->has |= PCX_AVT_COMMAND_HAS_IN;
                        continue;
                }

                return false;
        }

        return true;
}

bool
pcx_avt_command_word_equal(const struct pcx_avt_command_word *word1,
                           const char *word2)
{
        return is_word(word1->start, word1->length, word2);
}
