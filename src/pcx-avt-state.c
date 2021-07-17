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

#include "pcx-avt-state.h"

#include <string.h>

#include "pcx-util.h"
#include "pcx-avt-command.h"
#include "pcx-buffer.h"
#include "pcx-list.h"
#include "pcx-avt-hat.h"
#include "pcx-utf8.h"

#define PCX_AVT_STATE_MAX_CARRYING_WEIGHT 100
#define PCX_AVT_STATE_MAX_CARRYING_SIZE 100

enum pcx_avt_state_movable_type {
        PCX_AVT_STATE_MOVABLE_TYPE_OBJECT,
        PCX_AVT_STATE_MOVABLE_TYPE_MONSTER,
};

/* This is a copy of either a monster or an object from pcx_avt. They
 * are all allocated individually and will have their stats updated as
 * the game progresses.
 */
struct pcx_avt_state_movable {
        /* Node avt->all_movables */
        struct pcx_list all_node;

        /* Node in the list of the containing object. All movables
         * will always be in some list. If they aren’t in the game any
         * more then that list will be avt->nowhere.
         */
        struct pcx_list location_node;

        /* Parent movable if this is contained in another movable, or
         * NULL if it is somewhile else. The base.location_type enum
         * will also be kept up-to-date to determine where the node is
         * when this is NULL.
         */
        struct pcx_avt_state_movable *container;

        struct pcx_list contents;

        enum pcx_avt_state_movable_type type;

        union {
                struct pcx_avt_movable base;
                struct pcx_avt_object object;
                struct pcx_avt_monster monster;
        };
};

struct pcx_avt_state_room {
        /* Things that are in this room */
        struct pcx_list contents;

        uint32_t attributes;
};

struct pcx_avt_state {
        const struct pcx_avt *avt;

        int current_room;

        struct pcx_avt_state_room *rooms;

        /* Things that the player is carrying */
        struct pcx_list carrying;

        /* Things that are nowhere */
        struct pcx_list nowhere;

        /* All things */
        struct pcx_list all_movables;

        /* The objects and monsters that are originally created from
         * the pcx_avt will be directly referenced by number and so
         * have an index for them here.
         */
        struct pcx_avt_state_movable **object_index;
        struct pcx_avt_state_movable **monster_index;

        /* Queue of messages to report with
         * pcx_avt_state_get_next_message. Each message is a
         * zero-terminated string.
         */
        struct pcx_buffer message_buf;
        size_t message_buf_pos;

        uint64_t game_attributes;

        /* Temporary stack for searching for items */
        struct pcx_buffer stack;
};

static void
end_message(struct pcx_avt_state *state)
{
        pcx_buffer_append_c(&state->message_buf, '\0');
}

static PCX_PRINTF_FORMAT(2, 3) void
send_message(struct pcx_avt_state *state,
             const char *message,
             ...)
{
        va_list ap;

        va_start(ap, message);

        pcx_buffer_append_vprintf(&state->message_buf, message, ap);

        va_end(ap);

        end_message(state);
}

static void
add_movable_to_message(struct pcx_avt_state *state,
                       const struct pcx_avt_movable *movable,
                       const char *suffix)
{
        pcx_buffer_append_string(&state->message_buf, movable->adjective);
        pcx_buffer_append_c(&state->message_buf, 'a');
        if (movable->pronoun == PCX_AVT_PRONOUN_PLURAL)
                pcx_buffer_append_c(&state->message_buf, 'j');
        if (suffix)
                pcx_buffer_append_string(&state->message_buf, suffix);

        pcx_buffer_append_c(&state->message_buf, ' ');

        pcx_buffer_append_string(&state->message_buf, movable->name);
        pcx_buffer_append_c(&state->message_buf, 'o');
        if (movable->pronoun == PCX_AVT_PRONOUN_PLURAL)
                pcx_buffer_append_c(&state->message_buf, 'j');
        if (suffix)
                pcx_buffer_append_string(&state->message_buf, suffix);
}

static void
add_movables_to_message(struct pcx_avt_state *state,
                        struct pcx_list *list)
{
        struct pcx_avt_state_movable *object;

        pcx_list_for_each(object, list, location_node) {
                if (object->location_node.prev != list) {
                        const char *sep =
                                object->location_node.next == list ?
                                " kaj " :
                                ", ";
                        pcx_buffer_append_string(&state->message_buf, sep);
                }

                add_movable_to_message(state, &object->base, "n");
        }
}

static void
add_word_to_message(struct pcx_avt_state *state,
                    const struct pcx_avt_command_word *word)
{
        struct pcx_avt_hat_iter iter;

        pcx_avt_hat_iter_init(&iter, word->start, word->length);

        while (!pcx_avt_hat_iter_finished(&iter)) {
                uint32_t ch_upper = pcx_avt_hat_iter_next(&iter);
                uint32_t ch = pcx_avt_hat_to_lower(ch_upper);

                pcx_buffer_ensure_size(&state->message_buf,
                                       state->message_buf.length +
                                       PCX_UTF8_MAX_CHAR_LENGTH);

                int ch_length = pcx_utf8_encode(ch,
                                                (char *)
                                                state->message_buf.data +
                                                state->message_buf.length);
                state->message_buf.length += ch_length;
        }
}

static void
add_noun_to_message(struct pcx_avt_state *state,
                    const struct pcx_avt_command_noun *noun,
                    const char *suffix)
{
        if (noun->adjective.start) {
                add_word_to_message(state, &noun->adjective);
                pcx_buffer_append_c(&state->message_buf, 'a');
                if (noun->plural)
                        pcx_buffer_append_c(&state->message_buf, 'j');
                if (suffix)
                        pcx_buffer_append_string(&state->message_buf, suffix);
                pcx_buffer_append_c(&state->message_buf, ' ');
        }

        add_word_to_message(state, &noun->name);
        pcx_buffer_append_c(&state->message_buf, 'o');
        if (noun->plural)
                pcx_buffer_append_c(&state->message_buf, 'j');
        if (suffix)
                pcx_buffer_append_string(&state->message_buf, suffix);
}

static void
send_room_description(struct pcx_avt_state *state)
{
        const char *desc = state->avt->rooms[state->current_room].description;

        pcx_buffer_append_string(&state->message_buf, desc);

        struct pcx_avt_state_room *room = state->rooms + state->current_room;

        if (!pcx_list_empty(&room->contents)) {
                pcx_buffer_append_string(&state->message_buf, " Vi vidas ");
                add_movables_to_message(state, &room->contents);
                pcx_buffer_append_c(&state->message_buf, '.');
        }

        end_message(state);
}

static void
init_movable(struct pcx_avt_state *state,
             struct pcx_avt_state_movable *movable)
{
        movable->base.name = pcx_strdup(movable->base.name);
        movable->base.adjective = pcx_strdup(movable->base.adjective);

        pcx_list_init(&movable->contents);

        pcx_list_insert(state->all_movables.prev, &movable->all_node);
        pcx_list_insert(state->nowhere.prev, &movable->location_node);
}

static void
create_objects(struct pcx_avt_state *state)
{
        state->object_index =
                pcx_alloc(state->avt->n_objects *
                          sizeof (struct pcx_avt_state_movable *));

        for (size_t i = 0; i < state->avt->n_objects; i++) {
                struct pcx_avt_state_movable *movable =
                        pcx_alloc(sizeof *movable);
                state->object_index[i] = movable;
                movable->type = PCX_AVT_STATE_MOVABLE_TYPE_OBJECT;
                movable->object = state->avt->objects[i];
                init_movable(state, movable);
        }
}

static void
create_monsters(struct pcx_avt_state *state)
{
        state->monster_index =
                pcx_alloc(state->avt->n_monsters *
                          sizeof (struct pcx_avt_state_movable *));

        for (size_t i = 0; i < state->avt->n_monsters; i++) {
                struct pcx_avt_state_movable *movable =
                        pcx_alloc(sizeof *movable);
                state->monster_index[i] = movable;
                movable->type = PCX_AVT_STATE_MOVABLE_TYPE_MONSTER;
                movable->monster = state->avt->monsters[i];
                init_movable(state, movable);
        }
}

static void
put_movable_in_room(struct pcx_avt_state *state,
                    struct pcx_avt_state_room *room,
                    struct pcx_avt_state_movable *movable)
{
        pcx_list_remove(&movable->location_node);
        pcx_list_insert(room->contents.prev, &movable->location_node);
        movable->container = NULL;
        movable->base.location_type = PCX_AVT_LOCATION_TYPE_IN_ROOM;
}

static void
carry_movable(struct pcx_avt_state *state,
              struct pcx_avt_state_movable *movable)
{
        pcx_list_remove(&movable->location_node);
        pcx_list_insert(state->carrying.prev, &movable->location_node);
        movable->container = NULL;
        movable->base.location_type = PCX_AVT_LOCATION_TYPE_CARRYING;
}

static void
reparent_movable(struct pcx_avt_state *state,
                 struct pcx_avt_state_movable *parent,
                 struct pcx_avt_state_movable *movable)
{
        pcx_list_remove(&movable->location_node);
        pcx_list_insert(parent->contents.prev, &movable->location_node);
        movable->container = parent;

        switch (parent->type) {
        case PCX_AVT_STATE_MOVABLE_TYPE_OBJECT:
                movable->base.location_type =
                        PCX_AVT_LOCATION_TYPE_IN_OBJECT;
                break;
        case PCX_AVT_STATE_MOVABLE_TYPE_MONSTER:
                movable->base.location_type =
                        PCX_AVT_LOCATION_TYPE_WITH_MONSTER;
                break;
        }
}

static void
position_movables(struct pcx_avt_state *state)
{
        struct pcx_avt_state_movable *movable;

        pcx_list_for_each(movable, &state->all_movables, all_node) {
                uint8_t loc = movable->base.location;

                switch (movable->base.location_type) {
                case PCX_AVT_LOCATION_TYPE_IN_ROOM:
                        put_movable_in_room(state,
                                            state->rooms + loc,
                                            movable);
                        break;
                case PCX_AVT_LOCATION_TYPE_CARRYING:
                        carry_movable(state, movable);
                        break;
                case PCX_AVT_LOCATION_TYPE_NOWHERE:
                        /* This is the default */
                        break;
                case PCX_AVT_LOCATION_TYPE_WITH_MONSTER:
                        reparent_movable(state,
                                         state->monster_index[loc],
                                         movable);
                        break;
                case PCX_AVT_LOCATION_TYPE_IN_OBJECT:
                        reparent_movable(state,
                                         state->object_index[loc],
                                         movable);
                        break;
                }
        }
}

struct pcx_avt_state *
pcx_avt_state_new(const struct pcx_avt *avt)
{
        struct pcx_avt_state *state = pcx_calloc(sizeof *state);

        pcx_buffer_init(&state->message_buf);
        pcx_buffer_init(&state->stack);

        state->avt = avt;
        state->current_room = 0;

        pcx_list_init(&state->carrying);
        pcx_list_init(&state->nowhere);
        pcx_list_init(&state->all_movables);

        state->rooms = pcx_alloc(avt->n_rooms * sizeof *state->rooms);

        for (int i = 0; i < avt->n_rooms; i++) {
                pcx_list_init(&state->rooms[i].contents);
                state->rooms[i].attributes = avt->rooms[i].attributes;
        }

        create_objects(state);
        create_monsters(state);

        position_movables(state);

        if (state->avt->introduction)
                send_message(state, "%s", state->avt->introduction);

        send_room_description(state);

        state->game_attributes = avt->game_attributes;

        return state;
}

static const char *
get_pronoun_name(enum pcx_avt_pronoun pronoun)
{
        switch (pronoun) {
        case PCX_AVT_PRONOUN_MAN:
                return "li";
        case PCX_AVT_PRONOUN_WOMAN:
                return "ŝi";
        case PCX_AVT_PRONOUN_PLURAL:
                return "ili";
        case PCX_AVT_PRONOUN_ANIMAL:
                break;
        }

        return "ĝi";
}

static bool
movable_matches_noun(const struct pcx_avt_movable *movable,
                     const struct pcx_avt_command_noun *noun)
{
        if (noun->plural != (movable->pronoun == PCX_AVT_PRONOUN_PLURAL))
                return false;

        if (noun->adjective.start &&
            !pcx_avt_command_word_equal(&noun->adjective, movable->adjective))
                return false;

        return pcx_avt_command_word_equal(&noun->name, movable->name);
}

struct stack_entry {
        struct pcx_list *parent;
        struct pcx_list *child;
};

static struct stack_entry *
get_stack_top(struct pcx_avt_state *state)
{
        return (struct stack_entry *) ((state->stack.data +
                                        state->stack.length -
                                        sizeof (struct stack_entry)));
}

static void
push_list_onto_stack(struct pcx_avt_state *state,
                     struct pcx_list *list)
{
        pcx_buffer_set_length(&state->stack,
                              state->stack.length +
                              sizeof (struct stack_entry));

        struct stack_entry *entry = get_stack_top(state);

        entry->parent = list;
        entry->child = list->next;
}

static struct pcx_avt_state_movable *
find_movable_in_list(struct pcx_avt_state *state,
                     struct pcx_list *list,
                     const struct pcx_avt_command_noun *noun)
{
        pcx_buffer_set_length(&state->stack, 0);

        struct pcx_avt_state_movable *ret = NULL;

        push_list_onto_stack(state, list);

        while (state->stack.length > 0) {
                struct stack_entry *entry = get_stack_top(state);

                /* Have we reached the end of the list? */
                if (entry->child == entry->parent) {
                        /* Pop the stack */
                        state->stack.length -= sizeof (struct stack_entry);
                        continue;
                }

                struct pcx_avt_state_movable *child =
                        pcx_container_of(entry->child,
                                         struct pcx_avt_state_movable,
                                         location_node);

                if (movable_matches_noun(&child->base, noun)) {
                        ret = child;
                        break;
                }

                entry->child = entry->child->next;

                if (child->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT ||
                    (child->base.attributes &
                     PCX_AVT_OBJECT_ATTRIBUTE_CLOSED) == 0)
                        push_list_onto_stack(state, &child->contents);
        }

        return ret;
}

static struct pcx_avt_state_movable *
find_movable(struct pcx_avt_state *state,
             const struct pcx_avt_command_noun *noun)
{
        struct pcx_avt_state_movable *found;

        found = find_movable_in_list(state, &state->carrying, noun);
        if (found)
                return found;

        return find_movable_in_list(state,
                                    &state->rooms[state->current_room].contents,
                                    noun);
}

static struct pcx_avt_state_movable *
find_movable_or_message(struct pcx_avt_state *state,
                        const struct pcx_avt_command_noun *noun)
{
        struct pcx_avt_state_movable *movable = find_movable(state, noun);

        if (movable == NULL) {
                pcx_buffer_append_string(&state->message_buf, "Vi ne vidas ");
                add_noun_to_message(state, noun, "n");
                pcx_buffer_append_c(&state->message_buf, '.');
                end_message(state);
        }

        return movable;
}

static int
get_weight_of_list(struct pcx_avt_state *state,
                   struct pcx_list *list)
{
        int weight = 0;

        pcx_buffer_set_length(&state->stack, 0);

        push_list_onto_stack(state, list);

        while (state->stack.length > 0) {
                struct stack_entry *entry = get_stack_top(state);

                /* Have we reached the end of the list? */
                if (entry->child == entry->parent) {
                        /* Pop the stack */
                        state->stack.length -= sizeof (struct stack_entry);
                        continue;
                }

                struct pcx_avt_state_movable *child =
                        pcx_container_of(entry->child,
                                         struct pcx_avt_state_movable,
                                         location_node);

                if (child->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT)
                        weight += child->object.weight;

                entry->child = entry->child->next;

                push_list_onto_stack(state, &child->contents);
        }

        return weight;
}

static int
get_weight_of_movable(struct pcx_avt_state *state,
                      struct pcx_avt_state_movable *movable)
{
        int weight = 0;

        if (movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT)
                weight += movable->object.weight;

        return weight + get_weight_of_list(state, &movable->contents);
}

static int
get_size_in_list(struct pcx_list *list)
{
        struct pcx_avt_state_movable *movable;
        int size = 0;

        pcx_list_for_each(movable, list, location_node) {
                if (movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT)
                        size += movable->object.size;
        }

        return size;
}

static int
get_carrying_size(struct pcx_avt_state *state)
{
        return get_size_in_list(&state->carrying);
}

static int
get_movable_contents_size(struct pcx_avt_state_movable *movable)
{
        return get_size_in_list(&movable->contents);
}

static bool
is_carrying_movable(struct pcx_avt_state *state,
                    struct pcx_avt_state_movable *movable)
{
        while (true) {
                switch (movable->base.location_type) {
                case PCX_AVT_LOCATION_TYPE_NOWHERE:
                case PCX_AVT_LOCATION_TYPE_IN_ROOM:
                        return false;
                case PCX_AVT_LOCATION_TYPE_CARRYING:
                        return true;
                case PCX_AVT_LOCATION_TYPE_WITH_MONSTER:
                case PCX_AVT_LOCATION_TYPE_IN_OBJECT:
                        movable = movable->container;
                        continue;
                }

                return false;
        }
}

static void
add_noun(struct pcx_buffer *buf,
         const struct pcx_avt_command_noun *noun)
{
        if (noun->article)
                pcx_buffer_append_string(buf, "la ");
        if (noun->adjective.start) {
                pcx_buffer_append(buf,
                                  noun->adjective.start,
                                  noun->adjective.length);
                pcx_buffer_append_c(buf, 'a');
                if (noun->plural)
                        pcx_buffer_append_c(buf, 'j');
                if (noun->accusative)
                        pcx_buffer_append_c(buf, 'n');
                pcx_buffer_append_c(buf, ' ');
        }
        pcx_buffer_append(buf, noun->name.start, noun->name.length);

        if (!noun->is_pronoun) {
                pcx_buffer_append_c(buf, 'o');
                if (noun->plural)
                        pcx_buffer_append_c(buf, 'j');
        }

        if (noun->accusative)
                pcx_buffer_append_c(buf, 'n');
}

static bool
handle_direction(struct pcx_avt_state *state,
                 const struct pcx_avt_command *command)
{
        /* Must have direction. Subject and verb optional. */
        if ((command->has & ~(PCX_AVT_COMMAND_HAS_SUBJECT |
                              PCX_AVT_COMMAND_HAS_VERB)) !=
            PCX_AVT_COMMAND_HAS_DIRECTION)
                return false;

        if ((command->has & PCX_AVT_COMMAND_HAS_SUBJECT) &&
            (!command->subject.is_pronoun ||
             command->subject.pronoun.person != 1 ||
             command->subject.pronoun.plural))
                return false;

        if ((command->has & PCX_AVT_COMMAND_HAS_VERB) &&
            !pcx_avt_command_word_equal(&command->verb, "ir"))
                return false;

        static const struct {
                const char *word;
                int direction;
        } direction_map[] = {
                { "nord", PCX_AVT_DIRECTION_NORTH },
                { "orient", PCX_AVT_DIRECTION_EAST },
                { "sud", PCX_AVT_DIRECTION_SOUTH },
                { "okcident", PCX_AVT_DIRECTION_WEST },
                { "supr", PCX_AVT_DIRECTION_UP },
                { "malsupr", PCX_AVT_DIRECTION_DOWN },
                { "sub", PCX_AVT_DIRECTION_DOWN },
                { "el", PCX_AVT_DIRECTION_EXIT },
        };

        const struct pcx_avt_room *room =
                state->avt->rooms + state->current_room;

        for (int i = 0; i < PCX_N_ELEMENTS(direction_map); i++) {
                if (pcx_avt_command_word_equal(&command->direction,
                                               direction_map[i].word)) {
                        int dir = direction_map[i].direction;
                        int new_room = room->movements[dir];

                        if (new_room == PCX_AVT_DIRECTION_BLOCKED) {
                                send_message(state,
                                             "Vi ne povas iri %sen de "
                                             "ĉi tie.",
                                             direction_map[i].word);
                        } else {
                                state->current_room = new_room;
                                send_room_description(state);
                        }

                        return true;
                }
        }

        for (int i = 0; i < room->n_directions; i++) {
                if (pcx_avt_command_word_equal(&command->direction,
                                               room->directions[i].name)) {
                        state->current_room = room->directions[i].target;
                        send_room_description(state);
                        return true;
                }
        }

        return false;
}

static bool
handle_inventory(struct pcx_avt_state *state,
                 const struct pcx_avt_command *command)
{
        /* Must have object. Verb and subject optional. */
        if ((command->has & ~(PCX_AVT_COMMAND_HAS_SUBJECT |
                              PCX_AVT_COMMAND_HAS_VERB)) !=
            PCX_AVT_COMMAND_HAS_OBJECT)
                return false;

        if ((command->has & PCX_AVT_COMMAND_HAS_SUBJECT) &&
            (!command->subject.is_pronoun ||
             command->subject.pronoun.person != 1 ||
             command->subject.pronoun.plural))
                return false;

        if (command->object.is_pronoun ||
            command->object.article ||
            command->object.adjective.start ||
            command->object.plural ||
            !pcx_avt_command_word_equal(&command->object.name, "ki"))
                return false;

        if ((command->has & PCX_AVT_COMMAND_HAS_VERB) &&
            !pcx_avt_command_word_equal(&command->verb, "hav") &&
            !pcx_avt_command_word_equal(&command->verb, "kunport"))
                return false;

        pcx_buffer_append_string(&state->message_buf,
                                 "Vi kunportas ");

        if (pcx_list_empty(&state->carrying))
                pcx_buffer_append_string(&state->message_buf, "nenion");
        else
                add_movables_to_message(state, &state->carrying);

        pcx_buffer_append_c(&state->message_buf, '.');

        end_message(state);

        return true;
}

static bool
handle_look_direction(struct pcx_avt_state *state,
                      const struct pcx_avt_command_noun *noun)
{
        if (noun->adjective.start)
                return false;

        const struct pcx_avt_room *room =
                state->avt->rooms + state->current_room;

        for (size_t i = 0; i < room->n_directions; i++) {
                const char *description = room->directions[i].description;

                if (description == NULL)
                        continue;

                if (!pcx_avt_command_word_equal(&noun->name,
                                                room->directions[i].name))
                        continue;

                pcx_buffer_append_string(&state->message_buf, description);
                end_message(state);

                return true;
        }

        return false;
}

static bool
handle_look(struct pcx_avt_state *state,
            const struct pcx_avt_command *command)
{
        /* Must have verb. Object and subject optional. */
        if ((command->has & ~(PCX_AVT_COMMAND_HAS_SUBJECT |
                              PCX_AVT_COMMAND_HAS_OBJECT)) !=
            PCX_AVT_COMMAND_HAS_VERB)
                return false;

        if ((command->has & PCX_AVT_COMMAND_HAS_SUBJECT) &&
            (!command->subject.is_pronoun ||
             command->subject.pronoun.person != 1 ||
             command->subject.pronoun.plural))
                return false;

        if (!pcx_avt_command_word_equal(&command->verb, "rigard"))
                return false;

        if ((command->has & PCX_AVT_COMMAND_HAS_OBJECT) == 0) {
                send_room_description(state);
                return true;
        }

        if (handle_look_direction(state, &command->object))
                return true;

        struct pcx_avt_state_movable *movable =
                find_movable_or_message(state, &command->object);

        if (movable == NULL)
                return true;

        if (movable->base.description) {
                pcx_buffer_append_string(&state->message_buf,
                                         movable->base.description);
        } else {
                pcx_buffer_append_string(&state->message_buf,
                                         "Vi vidas nenion specialan pri la ");
                add_movable_to_message(state,
                                       &movable->base,
                                       NULL /* suffix */);
                pcx_buffer_append_c(&state->message_buf, '.');
        }

        if (movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT &&
            (movable->base.attributes & PCX_AVT_OBJECT_ATTRIBUTE_CLOSED) == 0 &&
            !pcx_list_empty(&movable->contents)) {
                const char *pronoun = get_pronoun_name(movable->base.pronoun);
                pcx_buffer_append_printf(&state->message_buf,
                                         " En %s vi vidas ",
                                         pronoun);
                add_movables_to_message(state, &movable->contents);
                pcx_buffer_append_c(&state->message_buf, '.');
        }

        end_message(state);

        return true;
}

static bool
is_verb_command_and_has(const struct pcx_avt_command *command,
                        enum pcx_avt_command_has has)
{
        /* Must have verb and extra flags. Subject optional. */
        if ((command->has & ~PCX_AVT_COMMAND_HAS_SUBJECT) !=
            (PCX_AVT_COMMAND_HAS_VERB | has))
                return false;

        if ((command->has & PCX_AVT_COMMAND_HAS_SUBJECT) &&
            (!command->subject.is_pronoun ||
             command->subject.pronoun.person != 1 ||
             command->subject.pronoun.plural))
                return false;

        return true;
}

static bool
is_verb_object_command(const struct pcx_avt_command *command)
{
        return is_verb_command_and_has(command, PCX_AVT_COMMAND_HAS_OBJECT);
}

static bool
validate_take(struct pcx_avt_state *state,
              struct pcx_avt_state_movable *movable)
{
        if (movable->base.location_type == PCX_AVT_LOCATION_TYPE_CARRYING) {
                pcx_buffer_append_string(&state->message_buf,
                                         "Vi jam portas la ");
                add_movable_to_message(state, &movable->base, "n");
                pcx_buffer_append_c(&state->message_buf, '.');
                end_message(state);
                return false;
        }

        if (movable->type == PCX_AVT_STATE_MOVABLE_TYPE_MONSTER ||
            (movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT &&
             (movable->base.attributes &
              PCX_AVT_OBJECT_ATTRIBUTE_PORTABLE) == 0)) {
                pcx_buffer_append_string(&state->message_buf,
                                         "Vi ne povas porti la ");
                add_movable_to_message(state, &movable->base, "n");
                pcx_buffer_append_c(&state->message_buf, '.');
                end_message(state);
                return false;
        }

        if (!is_carrying_movable(state, movable) &&
            get_weight_of_movable(state, movable) +
            get_weight_of_list(state, &state->carrying) >
            PCX_AVT_STATE_MAX_CARRYING_WEIGHT) {
                pcx_buffer_append_string(&state->message_buf,
                                         "Kune kun tio kion vi jam portas la ");
                add_movable_to_message(state, &movable->base, NULL);
                pcx_buffer_append_string(&state->message_buf,
                                         " estas tro peza");
                if (movable->base.pronoun == PCX_AVT_PRONOUN_PLURAL)
                        pcx_buffer_append_c(&state->message_buf, 'j');
                pcx_buffer_append_string(&state->message_buf, " por porti.");
                end_message(state);
                return false;
        }

        if (movable->object.size + get_carrying_size(state) >
            PCX_AVT_STATE_MAX_CARRYING_SIZE) {
                pcx_buffer_append_string(&state->message_buf,
                                         "Kune kun tio kion vi jam portas la ");
                add_movable_to_message(state, &movable->base, NULL);
                pcx_buffer_append_string(&state->message_buf,
                                         " estas tro granda");
                if (movable->base.pronoun == PCX_AVT_PRONOUN_PLURAL)
                        pcx_buffer_append_c(&state->message_buf, 'j');
                pcx_buffer_append_string(&state->message_buf, " por teni.");
                end_message(state);
                return false;
        }

        return true;
}

static bool
handle_take(struct pcx_avt_state *state,
            const struct pcx_avt_command *command)
{
        if (!is_verb_object_command(command))
                return false;

        if (!pcx_avt_command_word_equal(&command->verb, "pren"))
                return false;

        struct pcx_avt_state_movable *movable =
                find_movable_or_message(state, &command->object);

        if (movable == NULL)
                return true;

        if (!validate_take(state, movable))
                return true;

        carry_movable(state, movable);

        pcx_buffer_append_string(&state->message_buf, "Vi prenis la ");
        add_movable_to_message(state, &movable->base, "n");
        pcx_buffer_append_c(&state->message_buf, '.');
        end_message(state);

        return true;
}

static struct pcx_avt_state_movable *
find_carrying_movable_or_message(struct pcx_avt_state *state,
                                 const struct pcx_avt_command_noun *noun)
{
        struct pcx_avt_state_movable *movable =
                find_movable_or_message(state, noun);

        if (movable == NULL)
                return NULL;

        if (movable->base.location_type != PCX_AVT_LOCATION_TYPE_CARRYING) {
                if (!validate_take(state, movable))
                        return NULL;

                carry_movable(state, movable);

                pcx_buffer_append_string(&state->message_buf, "Vi prenis la ");
                add_movable_to_message(state, &movable->base, "n");
                pcx_buffer_append_c(&state->message_buf, '.');
                end_message(state);
        }

        return movable;
}

static bool
handle_drop(struct pcx_avt_state *state,
            const struct pcx_avt_command *command)
{
        if (!is_verb_object_command(command))
                return false;

        if (!pcx_avt_command_word_equal(&command->verb, "ĵet") &&
            !pcx_avt_command_word_equal(&command->verb, "forĵet") &&
            !pcx_avt_command_word_equal(&command->verb, "falig") &&
            !pcx_avt_command_word_equal(&command->verb, "las"))
                return false;

        struct pcx_avt_state_movable *movable =
                find_movable_or_message(state, &command->object);

        if (movable == NULL)
                return true;

        if (movable->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT ||
            movable->base.location_type != PCX_AVT_LOCATION_TYPE_CARRYING) {
                pcx_buffer_append_string(&state->message_buf,
                                         "Vi ne portas la ");
                add_movable_to_message(state, &movable->base, "n");
                pcx_buffer_append_c(&state->message_buf, '.');
                end_message(state);
                return true;
        }

        put_movable_in_room(state, state->rooms + state->current_room, movable);

        pcx_buffer_append_string(&state->message_buf, "Vi ĵetis la ");
        add_movable_to_message(state, &movable->base, "n");
        pcx_buffer_append_c(&state->message_buf, '.');
        end_message(state);

        return true;
}

static bool
container_would_create_cycle(struct pcx_avt_state_movable *container,
                             struct pcx_avt_state_movable *containee)
{
        while (container) {
                if (containee == container)
                        return true;

                container = container->container;
        }

        return false;
}

static bool
handle_put(struct pcx_avt_state *state,
           const struct pcx_avt_command *command)
{
        if (!is_verb_command_and_has(command,
                                     PCX_AVT_COMMAND_HAS_OBJECT |
                                     PCX_AVT_COMMAND_HAS_IN))
                return false;

        if (!pcx_avt_command_word_equal(&command->verb, "met") &&
            !pcx_avt_command_word_equal(&command->verb, "enmet"))
                return false;

        struct pcx_avt_state_movable *container =
                find_movable_or_message(state, &command->in);
        if (container == NULL)
                return true;

        if (container->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT ||
            container->object.container_size <= 0) {
                pcx_buffer_append_string(&state->message_buf,
                                         "Vi ne povas meti ion en la ");
                add_movable_to_message(state, &container->base, "n");
                pcx_buffer_append_c(&state->message_buf, '.');
                end_message(state);
                return true;
        }

        if ((container->base.attributes & PCX_AVT_OBJECT_ATTRIBUTE_CLOSED)) {
                pcx_buffer_append_string(&state->message_buf,
                                         "Vi ne povas meti ion en la ");
                add_movable_to_message(state, &container->base, "n");
                const char *pronoun = get_pronoun_name(container->base.pronoun);
                pcx_buffer_append_printf(&state->message_buf,
                                         " ĉar %s estas fermita",
                                         pronoun);
                if (container->base.pronoun == PCX_AVT_PRONOUN_PLURAL)
                        pcx_buffer_append_c(&state->message_buf, 'j');
                pcx_buffer_append_c(&state->message_buf, '.');
                end_message(state);
                return true;
        }

        struct pcx_avt_state_movable *containee =
                find_carrying_movable_or_message(state, &command->object);
        if (containee == NULL)
                return true;

        if (container_would_create_cycle(container, containee)) {
                send_message(state, "Vi ne povas meti ion en sin mem.");
                return true;
        }

        if (get_movable_contents_size(container) +
            containee->object.size >
            container->object.container_size) {
                pcx_buffer_append_string(&state->message_buf, "La ");
                add_movable_to_message(state, &containee->base, NULL);
                pcx_buffer_append_string(&state->message_buf,
                                         " estas tro granda");
                if (containee->base.pronoun == PCX_AVT_PRONOUN_PLURAL)
                        pcx_buffer_append_c(&state->message_buf, 'j');
                pcx_buffer_append_string(&state->message_buf,
                                         " por eniri la ");
                add_movable_to_message(state, &container->base, "n");
                pcx_buffer_append_c(&state->message_buf, '.');
                end_message(state);
                return true;
        }

        reparent_movable(state, container, containee);

        pcx_buffer_append_string(&state->message_buf, "Vi metis la ");
        add_movable_to_message(state, &containee->base, "n");
        pcx_buffer_append_string(&state->message_buf,
                                 " en la ");
        add_movable_to_message(state, &container->base, "n");
        pcx_buffer_append_c(&state->message_buf, '.');
        end_message(state);

        return true;
}

static bool
handle_enter(struct pcx_avt_state *state,
             const struct pcx_avt_command *command)
{
        const struct pcx_avt_command_noun *enter_noun;

        if (is_verb_object_command(command) &&
            pcx_avt_command_word_equal(&command->verb, "enir"))
                enter_noun = &command->object;
        else if (is_verb_command_and_has(command,
                                         PCX_AVT_COMMAND_HAS_IN) &&
                 (pcx_avt_command_word_equal(&command->verb, "enir") ||
                  pcx_avt_command_word_equal(&command->verb, "ir")))
                enter_noun = &command->in;
        else
                return false;

        struct pcx_avt_state_movable *movable =
                find_movable_or_message(state, enter_noun);

        if (movable == NULL)
                return true;

        if (movable->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT ||
            movable->object.enter_room == PCX_AVT_DIRECTION_BLOCKED) {
                pcx_buffer_append_string(&state->message_buf,
                                         "Vi ne povas eniri la ");
                add_movable_to_message(state, &movable->base, "n");
                pcx_buffer_append_c(&state->message_buf, '.');
                end_message(state);
                return true;
        }

        state->current_room = movable->object.enter_room;
        send_room_description(state);

        return true;
}

static bool
handle_exit(struct pcx_avt_state *state,
            const struct pcx_avt_command *command)
{
        if (!is_verb_command_and_has(command, 0) ||
            !pcx_avt_command_word_equal(&command->verb, "elir"))
                return false;

        const struct pcx_avt_room *room =
                state->avt->rooms + state->current_room;
        int new_room = room->movements[PCX_AVT_DIRECTION_EXIT];

        if (new_room == PCX_AVT_DIRECTION_BLOCKED) {
                send_message(state, "Vi ne povas eliri de ĉi tie.");
        } else {
                state->current_room = new_room;
                send_room_description(state);
        }

        return true;
}

static bool
handle_open_close(struct pcx_avt_state *state,
                  const struct pcx_avt_command *command)
{
        if (!is_verb_object_command(command))
                return false;

        uint32_t new_state;

        if (pcx_avt_command_word_equal(&command->verb, "ferm"))
                new_state = PCX_AVT_OBJECT_ATTRIBUTE_CLOSED;
        else if (pcx_avt_command_word_equal(&command->verb, "malferm"))
                new_state = 0;
        else
                return false;

        struct pcx_avt_state_movable *movable =
                find_movable_or_message(state, &command->object);

        if (movable == NULL)
                return true;

        if (movable->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT ||
            (movable->base.attributes &
             PCX_AVT_OBJECT_ATTRIBUTE_CLOSABLE) == 0) {
                pcx_buffer_append_string(&state->message_buf,
                                         "Vi ne povas ");
                add_word_to_message(state, &command->verb);
                pcx_buffer_append_string(&state->message_buf, "i la ");
                add_movable_to_message(state, &movable->base, "n");
                pcx_buffer_append_c(&state->message_buf, '.');
                end_message(state);
                return true;
        }

        if ((movable->base.attributes &
             PCX_AVT_OBJECT_ATTRIBUTE_CLOSED) == new_state) {
                pcx_buffer_append_string(&state->message_buf, "La ");
                add_movable_to_message(state, &movable->base, NULL);
                pcx_buffer_append_string(&state->message_buf, " jam estas ");
                if (new_state == 0)
                        pcx_buffer_append_string(&state->message_buf, "mal");
                pcx_buffer_append_string(&state->message_buf, "fermita");
                if (movable->base.pronoun == PCX_AVT_PRONOUN_PLURAL)
                        pcx_buffer_append_c(&state->message_buf, 'j');
                pcx_buffer_append_c(&state->message_buf, '.');
                end_message(state);
                return true;
        }

        movable->base.attributes = ((movable->base.attributes &
                                     ~PCX_AVT_OBJECT_ATTRIBUTE_CLOSED) |
                                    new_state);

        pcx_buffer_append_string(&state->message_buf, "Vi ");
        add_word_to_message(state, &command->verb);
        pcx_buffer_append_string(&state->message_buf, "is la ");
        add_movable_to_message(state, &movable->base, "n");
        pcx_buffer_append_c(&state->message_buf, '.');
        end_message(state);

        return true;
}

void
pcx_avt_state_run_command(struct pcx_avt_state *state,
                          const char *command_str)
{
        struct pcx_avt_command command;

        /* Free up any messages that we’ve already reported to the
         * caller
         */
        memmove(state->message_buf.data,
                state->message_buf.data + state->message_buf_pos,
                state->message_buf.length - state->message_buf_pos);
        state->message_buf.length -= state->message_buf_pos;
        state->message_buf_pos = state->message_buf.length;

        if (!pcx_avt_command_parse(command_str, &command)) {
                send_message(state, "Mi ne komprenas vin.");
                return;
        }

        if (handle_direction(state, &command))
                return;

        if (handle_look(state, &command))
                return;

        if (handle_take(state, &command))
                return;

        if (handle_drop(state, &command))
                return;

        if (handle_put(state, &command))
                return;

        if (handle_inventory(state, &command))
                return;

        if (handle_enter(state, &command))
                return;

        if (handle_exit(state, &command))
                return;

        if (handle_open_close(state, &command))
                return;

        struct pcx_buffer *buf = &state->message_buf;

        if ((command.has & PCX_AVT_COMMAND_HAS_SUBJECT))
                add_noun(buf, &command.subject);
        if ((command.has & PCX_AVT_COMMAND_HAS_VERB)) {
                if (buf->length >= 1)
                        pcx_buffer_append_c(buf, ' ');
                pcx_buffer_append(buf,
                                  command.verb.start,
                                  command.verb.length);
                pcx_buffer_append_string(buf, "as");
        }
        if ((command.has & PCX_AVT_COMMAND_HAS_OBJECT)) {
                if (buf->length >= 1)
                        pcx_buffer_append_c(buf, ' ');
                add_noun(buf, &command.object);
        }
        if ((command.has & PCX_AVT_COMMAND_HAS_DIRECTION)) {
                if (buf->length >= 1)
                        pcx_buffer_append_c(buf, ' ');
                pcx_buffer_append(buf,
                                  command.direction.start,
                                  command.direction.length);
                pcx_buffer_append_string(buf, "en");
        }
        if ((command.has & PCX_AVT_COMMAND_HAS_TOOL)) {
                if (buf->length >= 1)
                        pcx_buffer_append_c(buf, ' ');
                pcx_buffer_append_string(buf, "per ");
                add_noun(buf, &command.tool);
        }
        if ((command.has & PCX_AVT_COMMAND_HAS_IN)) {
                if (buf->length >= 1)
                        pcx_buffer_append_c(buf, ' ');
                pcx_buffer_append_string(buf, "en ");
                add_noun(buf, &command.in);
        }

        if (buf->length == 0)
                pcx_buffer_append_c(buf, '?');

        end_message(state);
}

const char *
pcx_avt_state_get_next_message(struct pcx_avt_state *state)
{
        if (state->message_buf_pos >= state->message_buf.length)
                return NULL;

        const char *ret = (const char *) (state->message_buf.data +
                                          state->message_buf_pos);

        state->message_buf_pos += strlen(ret) + 1;

        return ret;
}

static void
free_movables(struct pcx_avt_state *state)
{
        struct pcx_avt_state_movable *movable, *tmp;

        pcx_list_for_each_safe(movable, tmp, &state->all_movables, all_node) {
                pcx_free(movable->base.name);
                pcx_free(movable->base.adjective);
                pcx_free(movable);
        }
}

void
pcx_avt_state_free(struct pcx_avt_state *state)
{
        pcx_buffer_destroy(&state->message_buf);

        free_movables(state);

        pcx_free(state->object_index);
        pcx_free(state->monster_index);
        pcx_free(state->rooms);

        pcx_buffer_destroy(&state->stack);

        pcx_free(state);
}
