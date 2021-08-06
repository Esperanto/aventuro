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

#include "pcx-avt.h"

#include "pcx-util.h"

static void
free_aliases(struct pcx_avt_movable *movable)
{
        for (size_t i = 0; i < movable->n_aliases; i++) {
                pcx_free(movable->aliases[i].adjective);
                pcx_free(movable->aliases[i].name);
        }

        pcx_free(movable->aliases);
}

void
pcx_avt_free(struct pcx_avt *avt)
{
        for (size_t i = 0; i < avt->n_strings; i++)
                pcx_free(avt->strings[i]);

        pcx_free(avt->strings);

        for (size_t i = 0; i < avt->n_verbs; i++) {
                pcx_free(avt->verbs[i].name);
                pcx_free(avt->verbs[i].rules);
        }

        pcx_free(avt->verbs);

        for (size_t i = 0; i < avt->n_rooms; i++) {
                struct pcx_avt_room *room = avt->rooms + i;

                pcx_free(room->name);

                for (size_t j = 0; j < room->n_directions; j++)
                        pcx_free(room->directions[j].name);

                pcx_free(room->directions);
        }

        pcx_free(avt->rooms);

        for (size_t i = 0; i < avt->n_objects; i++) {
                struct pcx_avt_object *object = avt->objects + i;

                pcx_free(object->base.name);
                pcx_free(object->base.adjective);
                free_aliases(&object->base);
        }

        pcx_free(avt->objects);

        for (size_t i = 0; i < avt->n_monsters; i++) {
                struct pcx_avt_monster *monster = avt->monsters + i;

                pcx_free(monster->base.name);
                pcx_free(monster->base.adjective);
                free_aliases(&monster->base);
        }

        pcx_free(avt->monsters);

        for (size_t i = 0; i < avt->n_rules; i++) {
                struct pcx_avt_rule *rule = avt->rules + i;

                pcx_free(rule->conditions);
                pcx_free(rule->actions);
        }

        pcx_free(avt->rules);

        pcx_free(avt->introduction);
        pcx_free(avt->name);
        pcx_free(avt->author);
        pcx_free(avt->year);

        pcx_free(avt);
}
