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
#include <stdbool.h>

#define PCX_AVT_N_DIRECTIONS 7
#define PCX_AVT_DIRECTION_NORTH 0
#define PCX_AVT_DIRECTION_EAST 1
#define PCX_AVT_DIRECTION_SOUTH 2
#define PCX_AVT_DIRECTION_WEST 3
#define PCX_AVT_DIRECTION_UP 4
#define PCX_AVT_DIRECTION_DOWN 5
#define PCX_AVT_DIRECTION_EXIT 6
#define PCX_AVT_DIRECTION_BLOCKED UINT8_MAX

#define PCX_AVT_OBJECT_ATTRIBUTE_PORTABLE (1 << 1)
#define PCX_AVT_OBJECT_ATTRIBUTE_CLOSABLE (1 << 2)
#define PCX_AVT_OBJECT_ATTRIBUTE_CLOSED (1 << 3)
#define PCX_AVT_OBJECT_ATTRIBUTE_LIGHTABLE (1 << 4)
#define PCX_AVT_OBJECT_ATTRIBUTE_LIT (1 << 5)
#define PCX_AVT_OBJECT_ATTRIBUTE_FLAMMABLE (1 << 6)
#define PCX_AVT_OBJECT_ATTRIBUTE_LIGHTER (1 << 7)
#define PCX_AVT_OBJECT_ATTRIBUTE_BURNING (1 << 8)
#define PCX_AVT_OBJECT_ATTRIBUTE_BURNT_OUT (1 << 9)
#define PCX_AVT_OBJECT_ATTRIBUTE_EDIBLE (1 << 10)
#define PCX_AVT_OBJECT_ATTRIBUTE_DRINKABLE (1 << 11)
#define PCX_AVT_OBJECT_ATTRIBUTE_POISONOUS (1 << 12)

#define PCX_AVT_ROOM_ATTRIBUTE_LIT (1 << 1)
#define PCX_AVT_ROOM_ATTRIBUTE_UNLIGHTABLE (1 << 2)
#define PCX_AVT_ROOM_ATTRIBUTE_GAME_OVER (1 << 3)

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

enum pcx_avt_condition {
        PCX_AVT_CONDITION_IN_ROOM = 0x00,
        PCX_AVT_CONDITION_OBJECT_IS = 0x01,
        PCX_AVT_CONDITION_ANOTHER_OBJECT_PRESENT = 0x02,
        PCX_AVT_CONDITION_MONSTER_IS = 0x03,
        PCX_AVT_CONDITION_ANOTHER_MONSTER_PRESENT = 0x04,
        PCX_AVT_CONDITION_SHOTS = 0x06,
        PCX_AVT_CONDITION_WEIGHT = 0x07,
        PCX_AVT_CONDITION_SIZE = 0x08,
        PCX_AVT_CONDITION_CONTAINER_SIZE = 0x09,
        PCX_AVT_CONDITION_BURN_TIME = 0x0a,
        PCX_AVT_CONDITION_SOMETHING = 0x0b,
        PCX_AVT_CONDITION_NOTHING = 0x0c,
        PCX_AVT_CONDITION_NONE = 0x0f,
        PCX_AVT_CONDITION_OBJECT_ATTRIBUTE = 0x11,
        PCX_AVT_CONDITION_NOT_OBJECT_ATTRIBUTE = 0x12,
        PCX_AVT_CONDITION_ROOM_ATTRIBUTE = 0x13,
        PCX_AVT_CONDITION_NOT_ROOM_ATTRIBUTE = 0x14,
        PCX_AVT_CONDITION_MONSTER_ATTRIBUTE = 0x15,
        PCX_AVT_CONDITION_NOT_MONSTER_ATTRIBUTE = 0x16,
        PCX_AVT_CONDITION_PLAYER_ATTRIBUTE = 0x17,
        PCX_AVT_CONDITION_NOT_PLAYER_ATTRIBUTE = 0x18,
        PCX_AVT_CONDITION_CHANCE = 0x19,
        PCX_AVT_CONDITION_OBJECT_SAME_ADJECTIVE = 0x1b,
        PCX_AVT_CONDITION_MONSTER_SAME_ADJECTIVE = 0x1c,
        PCX_AVT_CONDITION_OBJECT_SAME_NAME = 0x1d,
        PCX_AVT_CONDITION_MONSTER_SAME_NAME = 0x1e,
        PCX_AVT_CONDITION_OBJECT_SAME_NOUN = 0x1f,
        PCX_AVT_CONDITION_MONSTER_SAME_NOUN = 0x20,
};

enum pcx_avt_action {
        PCX_AVT_ACTION_MOVE_TO = 0x00,
        PCX_AVT_ACTION_REPLACE_OBJECT = 0x01,
        PCX_AVT_ACTION_CREATE_OBJECT = 0x02,
        PCX_AVT_ACTION_REPLACE_MONSTER = 0x03,
        PCX_AVT_ACTION_CREATE_MONSTER = 0x04,
        PCX_AVT_ACTION_CHANGE_END = 0x05,
        PCX_AVT_ACTION_CHANGE_SHOTS = 0x06,
        PCX_AVT_ACTION_CHANGE_WEIGHT = 0x07,
        PCX_AVT_ACTION_CHANGE_SIZE = 0x08,
        PCX_AVT_ACTION_CHANGE_CONTAINER_SIZE = 0x09,
        PCX_AVT_ACTION_CHANGE_BURN_TIME = 0x0a,
        PCX_AVT_ACTION_SOMETHING = 0x0b,
        PCX_AVT_ACTION_NOTHING = 0x0c,
        PCX_AVT_ACTION_NOTHING_ROOM = 0x0f,
        PCX_AVT_ACTION_CARRY = 0x10,
        PCX_AVT_ACTION_SET_OBJECT_ATTRIBUTE = 0x11,
        PCX_AVT_ACTION_UNSET_OBJECT_ATTRIBUTE = 0x12,
        PCX_AVT_ACTION_SET_ROOM_ATTRIBUTE = 0x13,
        PCX_AVT_ACTION_UNSET_ROOM_ATTRIBUTE = 0x14,
        PCX_AVT_ACTION_SET_MONSTER_ATTRIBUTE = 0x15,
        PCX_AVT_ACTION_UNSET_MONSTER_ATTRIBUTE = 0x16,
        PCX_AVT_ACTION_SET_PLAYER_ATTRIBUTE = 0x17,
        PCX_AVT_ACTION_UNSET_PLAYER_ATTRIBUTE = 0x18,
        PCX_AVT_ACTION_CHANGE_OBJECT_ADJECTIVE = 0x1b,
        PCX_AVT_ACTION_CHANGE_MONSTER_ADJECTIVE = 0x1c,
        PCX_AVT_ACTION_CHANGE_OBJECT_NAME = 0x1d,
        PCX_AVT_ACTION_CHANGE_MONSTER_NAME = 0x1e,
        PCX_AVT_ACTION_COPY_OBJECT = 0x1f,
        PCX_AVT_ACTION_COPY_MONSTER = 0x20,

        /* Additonally run the actions of the given rule */
        PCX_AVT_ACTION_RUN_RULE = 0x80,
};

struct pcx_avt_movable {
        /* Owned by this. These will have the ending removed. */
        char *name, *adjective;
        /* Owned by the parent pcx_avt. Can be NULL. */
        const char *description;

        enum pcx_avt_pronoun pronoun;

        enum pcx_avt_location_type location_type;
        uint8_t location;

        uint32_t attributes;
};

struct pcx_avt_object {
        struct pcx_avt_movable base;

        /* Text that gets shown when the object is read. Owned by the
         * parent pcx_avt. Can be NULL.
         */
        const char *read_text;

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
};

struct pcx_avt_monster {
        struct pcx_avt_movable base;

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

        uint8_t points;

        uint32_t attributes;
};

enum pcx_avt_rule_subject {
        PCX_AVT_RULE_SUBJECT_ROOM,
        PCX_AVT_RULE_SUBJECT_OBJECT,
        PCX_AVT_RULE_SUBJECT_TOOL,
        PCX_AVT_RULE_SUBJECT_MONSTER,
};

struct pcx_avt_condition_data {
        enum pcx_avt_rule_subject subject;
        enum pcx_avt_condition condition;
        uint8_t data;
};

struct pcx_avt_action_data {
        enum pcx_avt_rule_subject subject;
        enum pcx_avt_action action;
        uint8_t data;
};

struct pcx_avt_verb {
        char *name;

        int n_rules;
        uint16_t *rules;
};

struct pcx_avt_rule {
        /* Owned by pcx_avt->strings. Can be NULL. */
        const char *text;

        uint8_t points;

        unsigned n_conditions;
        struct pcx_avt_condition_data *conditions;
        unsigned n_actions;
        struct pcx_avt_action_data *actions;
};

enum pcx_avt_alias_type {
        PCX_AVT_ALIAS_TYPE_OBJECT = 1,
        PCX_AVT_ALIAS_TYPE_MONSTER = 3,
};

struct pcx_avt_alias {
        enum pcx_avt_alias_type type;
        bool plural;
        int index;
        char *name;
};

struct pcx_avt {
        char *name;
        char *author;
        char *year;

        size_t n_strings;
        char **strings;

        size_t n_verbs;
        struct pcx_avt_verb *verbs;

        size_t n_rules;
        struct pcx_avt_rule *rules;

        size_t n_aliases;
        struct pcx_avt_alias *aliases;

        size_t n_rooms;
        struct pcx_avt_room *rooms;
        size_t n_objects;
        struct pcx_avt_object *objects;
        size_t n_monsters;
        struct pcx_avt_monster *monsters;
        uint64_t game_attributes;

        uint16_t start_thirst;
        uint16_t start_hunger;

        /* Text to be displayed at the start of the game. Can be NULL */
        char *introduction;
};

void
pcx_avt_free(struct pcx_avt *avt);

#endif /* PCX_AVT_H */
