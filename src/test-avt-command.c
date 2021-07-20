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

#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static void
check_word_order(const char *phrase)
{
        struct pcx_avt_command command;

        assert(pcx_avt_command_parse(phrase, &command));

        assert(command.has & PCX_AVT_COMMAND_HAS_SUBJECT);
        assert(!command.subject.article);
        assert(!command.subject.plural);
        assert(!command.subject.accusative);
        assert(!command.subject.is_pronoun);
        assert(command.subject.name.length == 4);
        assert(!memcmp(command.subject.name.start, "hund", 4));
        assert(command.subject.adjective.length == 5);
        assert(!memcmp(command.subject.adjective.start, "blank", 5));

        assert(command.has & PCX_AVT_COMMAND_HAS_OBJECT);
        assert(!command.object.article);
        assert(!command.object.plural);
        assert(command.object.accusative);
        assert(!command.object.is_pronoun);
        assert(command.object.name.length == 3);
        assert(!memcmp(command.object.name.start, "kat", 3));
        assert(command.object.adjective.length == 4);
        assert(!memcmp(command.object.adjective.start, "nigr", 4));
}

static void
check_word_order_no_adjective(const char *phrase, bool article)
{
        struct pcx_avt_command command;

        assert(pcx_avt_command_parse(phrase, &command));

        assert(command.has & PCX_AVT_COMMAND_HAS_SUBJECT);
        assert(command.subject.article == article);
        assert(!command.subject.plural);
        assert(!command.subject.accusative);
        assert(!command.subject.is_pronoun);
        assert(command.subject.name.length == 4);
        assert(!memcmp(command.subject.name.start, "hund", 4));
        assert(command.subject.adjective.start == NULL);

        assert(command.has & PCX_AVT_COMMAND_HAS_OBJECT);
        assert(command.object.article == article);
        assert(!command.object.plural);
        assert(command.object.accusative);
        assert(!command.object.is_pronoun);
        assert(command.object.name.length == 3);
        assert(!memcmp(command.object.name.start, "kat", 3));
        assert(command.object.adjective.start == NULL);
}

static void
check_pronoun(const char *pronoun,
              int person,
              int genders,
              bool plural)
{
        struct pcx_avt_command command;

        assert(pcx_avt_command_parse(pronoun, &command));
        assert(command.has & PCX_AVT_COMMAND_HAS_SUBJECT);
        assert(!command.subject.article);
        assert(command.subject.plural == plural);
        assert(!command.subject.accusative);
        assert(command.subject.is_pronoun);
        assert(command.subject.adjective.start == NULL);
        assert(command.subject.name.length == strlen(pronoun));
        assert(!memcmp(command.subject.name.start, pronoun, strlen(pronoun)));

        assert(command.subject.pronoun.person == person);
        assert(command.subject.pronoun.genders == genders);
        assert(command.subject.pronoun.plural == plural);
}

int
main(int argc, char **argv)
{
        struct pcx_avt_command command;

        assert(pcx_avt_command_parse("la hundo "
                                     "ĵetu "
                                     "la raton "
                                     "al la plaĝo "
                                     "per la aviadilo",
                                     &command));

        assert(command.has ==
               (PCX_AVT_COMMAND_HAS_SUBJECT |
                PCX_AVT_COMMAND_HAS_VERB |
                PCX_AVT_COMMAND_HAS_OBJECT |
                PCX_AVT_COMMAND_HAS_DIRECTION |
                PCX_AVT_COMMAND_HAS_TOOL));

        assert(command.subject.article);
        assert(!command.subject.plural);
        assert(!command.subject.accusative);
        assert(!command.subject.is_pronoun);
        assert(command.subject.name.length == 4);
        assert(!memcmp(command.subject.name.start, "hund", 4));
        assert(command.subject.adjective.start == NULL);

        assert(command.verb.length == 4);
        assert(!memcmp(command.verb.start, "ĵet", 4));

        assert(command.object.article);
        assert(!command.object.plural);
        assert(command.object.accusative);
        assert(!command.object.is_pronoun);
        assert(command.object.name.length == 3);
        assert(!memcmp(command.object.name.start, "rat", 3));
        assert(command.object.adjective.start == NULL);

        assert(command.direction.length == 5);
        assert(!memcmp(command.direction.start, "plaĝ", 5));

        assert(command.tool.article);
        assert(!command.tool.plural);
        assert(!command.tool.accusative);
        assert(!command.tool.is_pronoun);
        assert(command.tool.name.length == 7);
        assert(!memcmp(command.tool.name.start, "aviadil", 7));
        assert(command.tool.adjective.start == NULL);

        assert(pcx_avt_command_parse("la griza hundo", &command));
        assert(command.has == PCX_AVT_COMMAND_HAS_SUBJECT);
        assert(command.subject.article);
        assert(!command.subject.plural);
        assert(!command.subject.accusative);
        assert(!command.subject.is_pronoun);
        assert(command.subject.adjective.start);
        assert(command.subject.adjective.length == 4);
        assert(!memcmp(command.subject.adjective.start, "griz", 4));
        assert(command.subject.name.length == 4);
        assert(!memcmp(command.subject.name.start, "hund", 4));

        assert(pcx_avt_command_parse("grizaj hundoj", &command));
        assert(command.has == PCX_AVT_COMMAND_HAS_SUBJECT);
        assert(!command.subject.article);
        assert(command.subject.plural);
        assert(!command.subject.accusative);
        assert(!command.subject.is_pronoun);
        assert(command.subject.adjective.start);
        assert(command.subject.adjective.length == 4);
        assert(!memcmp(command.subject.adjective.start, "griz", 4));
        assert(command.subject.name.length == 4);
        assert(!memcmp(command.subject.name.start, "hund", 4));

        assert(pcx_avt_command_parse("hundoj grizaj", &command));
        assert(command.has == PCX_AVT_COMMAND_HAS_SUBJECT);
        assert(!command.subject.article);
        assert(command.subject.plural);
        assert(!command.subject.accusative);
        assert(!command.subject.is_pronoun);
        assert(command.subject.adjective.start);
        assert(command.subject.adjective.length == 4);
        assert(!memcmp(command.subject.adjective.start, "griz", 4));
        assert(command.subject.name.length == 4);
        assert(!memcmp(command.subject.name.start, "hund", 4));

        assert(!pcx_avt_command_parse("griza hundoj", &command));

        assert(pcx_avt_command_parse("la hundon grizan", &command));
        assert(command.has == PCX_AVT_COMMAND_HAS_OBJECT);
        assert(command.object.article);
        assert(!command.object.plural);
        assert(command.object.accusative);
        assert(!command.object.is_pronoun);
        assert(command.object.adjective.start);
        assert(command.object.adjective.length == 4);
        assert(!memcmp(command.object.adjective.start, "griz", 4));
        assert(command.object.name.length == 4);
        assert(!memcmp(command.object.name.start, "hund", 4));

        assert(!pcx_avt_command_parse("la hundo grizan", &command));

        assert(pcx_avt_command_parse("en la skatolon blankan", &command));
        assert(command.has == PCX_AVT_COMMAND_HAS_IN);
        assert(command.in.article);
        assert(!command.in.plural);
        assert(command.in.accusative);
        assert(!command.in.is_pronoun);
        assert(command.in.adjective.start);
        assert(command.in.adjective.length == 5);
        assert(!memcmp(command.in.adjective.start, "blank", 5));
        assert(command.in.name.length == 6);
        assert(!memcmp(command.in.name.start, "skatol", 6));

        assert(pcx_avt_command_parse("en la skatoloj", &command));
        assert(command.has == PCX_AVT_COMMAND_HAS_IN);
        assert(command.in.article);
        assert(command.in.plural);
        assert(!command.in.accusative);
        assert(!command.in.is_pronoun);
        assert(command.in.adjective.start == NULL);
        assert(command.in.name.length == 6);
        assert(!memcmp(command.in.name.start, "skatol", 6));

        assert(!pcx_avt_command_parse("al la blanka skatolo", &command));

        assert(pcx_avt_command_parse("AL LA    skatolo   ", &command));
        assert(command.has == PCX_AVT_COMMAND_HAS_DIRECTION);
        assert(command.direction.length == 6);
        assert(!memcmp(command.direction.start, "skatol", 6));

        assert(pcx_avt_command_parse("  merden   ", &command));
        assert(command.has == PCX_AVT_COMMAND_HAS_DIRECTION);
        assert(command.direction.length == 4);
        assert(!memcmp(command.direction.start, "merd", 4));

        assert(!pcx_avt_command_parse("hundo hundo", &command));
        assert(!pcx_avt_command_parse("hundon hundon", &command));
        assert(!pcx_avt_command_parse("per hundo per hundo", &command));
        assert(!pcx_avt_command_parse("merden merden", &command));
        assert(!pcx_avt_command_parse("en la maro en la maro", &command));
        assert(!pcx_avt_command_parse("iru iru", &command));

        assert(pcx_avt_command_parse("iras", &command));
        assert(command.has == PCX_AVT_COMMAND_HAS_VERB);
        assert(command.verb.length == 2);
        assert(!memcmp(command.verb.start, "ir", 2));

        assert(pcx_avt_command_parse("iros", &command));
        assert(command.has == PCX_AVT_COMMAND_HAS_VERB);
        assert(command.verb.length == 2);
        assert(!memcmp(command.verb.start, "ir", 2));

        assert(pcx_avt_command_parse("iri", &command));
        assert(command.has == PCX_AVT_COMMAND_HAS_VERB);
        assert(command.verb.length == 2);
        assert(!memcmp(command.verb.start, "ir", 2));

        assert(!pcx_avt_command_parse("iris", &command));

        check_word_order("blanka hundo nigran katon");
        check_word_order("hundo blanka nigran katon");
        check_word_order("blanka hundo katon nigran");
        check_word_order("hundo blanka katon nigran");
        check_word_order("nigran katon blanka hundo");
        check_word_order("nigran katon hundo blanka");
        check_word_order("katon nigran blanka hundo");
        check_word_order("katon nigran hundo blanka");

        check_word_order_no_adjective("hundo katon", false);
        check_word_order_no_adjective("katon hundo", false);

        check_word_order_no_adjective("la hundo la katon", true);
        check_word_order_no_adjective("la katon la hundo", true);

        assert(pcx_avt_command_parse("al la urbo hundo iros", &command));
        assert(command.has == (PCX_AVT_COMMAND_HAS_VERB |
                               PCX_AVT_COMMAND_HAS_SUBJECT |
                               PCX_AVT_COMMAND_HAS_DIRECTION));
        assert(command.direction.length == 3);
        assert(!memcmp(command.direction.start, "urb", 3));
        assert(!command.subject.article);
        assert(!command.subject.plural);
        assert(!command.subject.accusative);
        assert(!command.subject.is_pronoun);
        assert(command.subject.adjective.start == NULL);
        assert(command.subject.name.length == 4);
        assert(!memcmp(command.subject.name.start, "hund", 4));
        assert(command.verb.length == 2);
        assert(!memcmp(command.verb.start, "ir", 2));

        check_pronoun("mi",
                      1,
                      PCX_AVT_COMMAND_GENDER_MAN |
                      PCX_AVT_COMMAND_GENDER_WOMAN |
                      PCX_AVT_COMMAND_GENDER_THING,
                      false);
        check_pronoun("ni",
                      1,
                      PCX_AVT_COMMAND_GENDER_MAN |
                      PCX_AVT_COMMAND_GENDER_WOMAN |
                      PCX_AVT_COMMAND_GENDER_THING,
                      true);
        check_pronoun("vi",
                      2,
                      PCX_AVT_COMMAND_GENDER_MAN |
                      PCX_AVT_COMMAND_GENDER_WOMAN |
                      PCX_AVT_COMMAND_GENDER_THING,
                      false);
        check_pronoun("li",
                      3,
                      PCX_AVT_COMMAND_GENDER_MAN,
                      false);
        check_pronoun("ŝi",
                      3,
                      PCX_AVT_COMMAND_GENDER_WOMAN,
                      false);
        check_pronoun("ĝi",
                      3,
                      PCX_AVT_COMMAND_GENDER_THING,
                      false);
        check_pronoun("GXi",
                      3,
                      PCX_AVT_COMMAND_GENDER_THING,
                      false);
        check_pronoun("RI",
                      3,
                      PCX_AVT_COMMAND_GENDER_MAN |
                      PCX_AVT_COMMAND_GENDER_WOMAN,
                      false);
        check_pronoun("ili",
                      3,
                      PCX_AVT_COMMAND_GENDER_MAN |
                      PCX_AVT_COMMAND_GENDER_WOMAN |
                      PCX_AVT_COMMAND_GENDER_THING,
                      true);

        assert(!pcx_avt_command_parse("griza ĝi", &command));
        assert(!pcx_avt_command_parse("la ĝi", &command));
        assert(!pcx_avt_command_parse("ĝij", &command));

        pcx_avt_command_parse("iru!", &command);
        assert(command.has == PCX_AVT_COMMAND_HAS_VERB);
        assert(command.verb.length == 2);
        assert(!memcmp(command.verb.start, "iru", 2));

        assert(pcx_avt_command_parse("iru.", &command));
        assert(pcx_avt_command_parse("iru?", &command));
        assert(pcx_avt_command_parse("iru  !  ", &command));
        assert(!pcx_avt_command_parse("iru#", &command));
        assert(!pcx_avt_command_parse("iru  !!  ", &command));
        assert(!pcx_avt_command_parse("iru  ! vorto ", &command));
        assert(!pcx_avt_command_parse("norden-iru", &command));

        return EXIT_SUCCESS;
}
