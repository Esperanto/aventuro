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

#ifndef PCX_AVT_H
#define PCX_AVT_H

#include <stdint.h>
#include <stdlib.h>

#define PCX_AVT_N_DIRECTIONS 7
#define PCX_AVT_DIRECTION_BLOCKED UINT8_MAX

enum pcx_avt_pronoun {
        PCX_AVT_PRONOUN_MAN,
        PCX_AVT_PRONOUN_WOMAN,
        PCX_AVT_PRONOUN_ANIMAL,
        PCX_AVT_PRONOUN_PLURAL,
};

enum pcx_avt_location_type {
        PCX_AVT_LOCATION_TYPE_IN_ROOM = 0x00,
        PCX_AVT_LOCATION_TYPE_CARRYING = 0x10,
        PCX_AVT_LOCATION_TYPE_NOWHERE = 0x0f,
        PCX_AVT_LOCATION_TYPE_WITH_MONSTER = 0x03,
        PCX_AVT_LOCATION_TYPE_IN_OBJECT = 0x01,
};

struct pcx_avt_object {
        /* Owned by this */
        char *name, *adjective;
        /* Owned by the parent pcx_avt. Can be NULL. */
        const char *description;
        /* Text that gets shown when the object is read. Owned by the
         * parent pcx_avt. Can be NULL.
         */
        const char *read_text;

        enum pcx_avt_pronoun pronoun;

        uint8_t points;
        uint8_t weight;
        uint8_t size;
        uint8_t shot_damage;
        uint8_t shots;
        uint8_t hit_damage;
        uint8_t stab_damage;
        uint8_t food_points;
        uint8_t trink_points;
        uint8_t burn_time;
        uint8_t end;

        /* The total size of things that this object can contain, or
         * zero if it can’t contain anything.
         */
        uint8_t container_size;
        /* The room number that the player gets teleported to if this
         * object is entered, or PCX_AVT_DIRECTION_BLOCKED if it can’t
         * be entered.
         */
        uint8_t enter_room;

        enum pcx_avt_location_type location_type;
        uint8_t location;
};

struct pcx_avt_monster {
        /* Owned by this */
        char *name, *adjective;
        /* Owned by the parent pcx_avt. Can be NULL. */
        const char *description;

        enum pcx_avt_pronoun pronoun;

        /* The object that replaces the monster when it dies */
        uint8_t dead_object;

        uint8_t hunger;
        uint8_t thrist;
        int16_t aggression;
        uint8_t attack;
        uint8_t protection;
        uint8_t lives;
        uint8_t escape;
        uint8_t wander;

        enum pcx_avt_location_type location_type;
        uint8_t location;
};

struct pcx_avt_direction {
        /* Owned by this. This is the root word without any endings */
        char *name;
        /* Owned by the parent pcx_avt. Can be NULL */
        const char *description;
        uint8_t target;
};

struct pcx_avt_room {
        /* Short name of the room. Owned by this struct. */
        char *name;
        /* Long description of the room. The string is one of
         * pcx_avt->strings
         */
        const char *description;

        /* Room numbers for the directions that can be moved to from
         * here. These are offsets into pcx_avt->rooms or
         * PCX_AVT_DIRECTION_BLOCKED if the player can’t move in that
         * direction.
         */
        uint8_t movements[PCX_AVT_N_DIRECTIONS];

        size_t n_directions;
        struct pcx_avt_direction *directions;
};

struct pcx_avt {
        size_t n_strings;
        char **strings;
        size_t n_rooms;
        struct pcx_avt_room *rooms;
        size_t n_objects;
        struct pcx_avt_object *objects;
        size_t n_monsters;
        struct pcx_avt_monster *monsters;
};

void
pcx_avt_free(struct pcx_avt *avt);

#endif /* PCX_AVT_H */
