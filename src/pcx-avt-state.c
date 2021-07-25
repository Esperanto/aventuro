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
#include <stddef.h>
#include <stdalign.h>

#include "pcx-util.h"
#include "pcx-avt-command.h"
#include "pcx-buffer.h"
#include "pcx-list.h"
#include "pcx-avt-hat.h"
#include "pcx-utf8.h"

#define PCX_AVT_STATE_MAX_CARRYING_WEIGHT 100
#define PCX_AVT_STATE_MAX_CARRYING_SIZE 100

/* If a rule action triggers another rule action, we’ll limit the
 * recursion to this to prevent stack overflow.
 */
#define PCX_AVT_STATE_MAX_RECURSION_DEPTH 10

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

        /* Have we visited this room before? */
        bool visited;

        uint32_t attributes;
};

struct pcx_avt_state_references {
        struct pcx_avt_state_movable *object;
        struct pcx_avt_state_movable *tool;
        struct pcx_avt_state_movable *in;
        struct pcx_avt_state_movable *direction;
};

struct pcx_avt_state {
        const struct pcx_avt *avt;

        int current_room;
        bool game_over;
        int points;

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
         * zero-terminated string prefixed with pcx_avt_state_message.
         */
        struct pcx_buffer message_buf;
        size_t message_buf_pos;
        bool message_in_progress;

        uint64_t game_attributes;

        /* Temporary stack for searching for items */
        struct pcx_buffer stack;

        /* Used to prevent infinite recursion when executing rules */
        int rule_recursion_depth;

        /* The references that were used in the last command. This
         * will be used to resolve pronouns.
         */
        struct pcx_avt_state_references previous_references;

        int (* random_cb)(void *);
        void *random_cb_data;
};

static int
get_random(struct pcx_avt_state *state)
{
        return state->random_cb(state->random_cb_data);
}

static bool
run_special_rules(struct pcx_avt_state *state,
                  const char *verb_str,
                  struct pcx_avt_state_movable *object,
                  struct pcx_avt_state_movable *tool,
                  int room);

static void
end_message(struct pcx_avt_state *state);

static size_t
align_message_start(size_t pos)
{
        const size_t alignment = alignof(struct pcx_avt_state_message);
        return (pos + alignment - 1) & ~(alignment - 1);
}

static void
start_message_type(struct pcx_avt_state *state,
                   enum pcx_avt_state_message_type type)
{
        if (state->message_in_progress)
                end_message(state);

        size_t message_start_offset = state->message_buf.length;

        pcx_buffer_set_length(&state->message_buf,
                              message_start_offset +
                              offsetof(struct pcx_avt_state_message, text));

        struct pcx_avt_state_message *message =
                (struct pcx_avt_state_message *)
                (state->message_buf.data + message_start_offset);

        message->type = type;

        state->message_in_progress = true;
}

static void
ensure_message_in_progress(struct pcx_avt_state *state)
{
        if (state->message_in_progress)
                return;

        start_message_type(state, PCX_AVT_STATE_MESSAGE_TYPE_NORMAL);
}

static void
add_message_data(struct pcx_avt_state *state,
                 const void *data,
                 size_t length)
{
        ensure_message_in_progress(state);
        pcx_buffer_append(&state->message_buf, data, length);
}

static void
add_message_string(struct pcx_avt_state *state,
                   const char *string)
{
        ensure_message_in_progress(state);
        pcx_buffer_append_string(&state->message_buf, string);
}

static void
add_message_c(struct pcx_avt_state *state,
              char ch)
{
        ensure_message_in_progress(state);
        pcx_buffer_append_c(&state->message_buf, ch);
}

static void
add_message_unichar(struct pcx_avt_state *state,
                    uint32_t ch)
{
        ensure_message_in_progress(state);

        pcx_buffer_ensure_size(&state->message_buf,
                               state->message_buf.length +
                               PCX_UTF8_MAX_CHAR_LENGTH);

        int ch_length = pcx_utf8_encode(ch,
                                        (char *)
                                        state->message_buf.data +
                                        state->message_buf.length);
        state->message_buf.length += ch_length;
}

static void
add_message_vprintf(struct pcx_avt_state *state,
                    const char *format,
                    va_list ap)
{
        ensure_message_in_progress(state);
        pcx_buffer_append_vprintf(&state->message_buf, format, ap);
}

PCX_PRINTF_FORMAT(2, 3) static void
add_message_printf(struct pcx_avt_state *state,
                   const char *format,
                   ...)
{
        va_list ap;

        va_start(ap, format);
        add_message_vprintf(state, format, ap);
        va_end(ap);
}

static void
end_message(struct pcx_avt_state *state)
{
        ensure_message_in_progress(state);
        pcx_buffer_append_c(&state->message_buf, '\0');
        pcx_buffer_set_length(&state->message_buf,
                              align_message_start(state->message_buf.length));
        state->message_in_progress = false;
}

static PCX_PRINTF_FORMAT(2, 3) void
send_message(struct pcx_avt_state *state,
             const char *message,
             ...)
{
        va_list ap;

        va_start(ap, message);

        add_message_vprintf(state, message, ap);

        va_end(ap);

        end_message(state);
}

static void
add_movable_to_message(struct pcx_avt_state *state,
                       const struct pcx_avt_movable *movable,
                       const char *suffix)
{
        add_message_string(state, movable->adjective);
        add_message_c(state, 'a');
        if (movable->pronoun == PCX_AVT_PRONOUN_PLURAL)
                add_message_c(state, 'j');
        if (suffix)
                add_message_string(state, suffix);

        add_message_c(state, ' ');

        add_message_string(state, movable->name);
        add_message_c(state, 'o');
        if (movable->pronoun == PCX_AVT_PRONOUN_PLURAL)
                add_message_c(state, 'j');
        if (suffix)
                add_message_string(state, suffix);
}

static void
add_movables_to_message(struct pcx_avt_state *state,
                        struct pcx_list *list,
                        const char *suffix)
{
        struct pcx_avt_state_movable *object;

        pcx_list_for_each(object, list, location_node) {
                if (object->location_node.prev != list) {
                        add_message_string(state,
                                           object->location_node.next == list ?
                                           " kaj " :
                                           ", ");
                }

                add_movable_to_message(state, &object->base, suffix);
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

                add_message_unichar(state, ch);
        }
}

static void
add_noun_to_message(struct pcx_avt_state *state,
                    const struct pcx_avt_command_noun *noun,
                    const char *suffix)
{
        if (noun->adjective.start) {
                add_word_to_message(state, &noun->adjective);
                add_message_c(state, 'a');
                if (noun->plural)
                        add_message_c(state, 'j');
                if (suffix)
                        add_message_string(state, suffix);
                add_message_c(state, ' ');
        }

        add_word_to_message(state, &noun->name);
        add_message_c(state, 'o');
        if (noun->plural)
                add_message_c(state, 'j');
        if (suffix)
                add_message_string(state, suffix);
}

struct stack_entry {
        struct pcx_list *parent;
        struct pcx_list *child;
};

typedef bool
(* iterate_movables_cb)(struct pcx_avt_state_movable *movable,
                        void *user_data);

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
iterate_movables_in_list(struct pcx_avt_state *state,
                         struct pcx_list *list,
                         bool descend_closed,
                         iterate_movables_cb cb,
                         void *user_data)
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

                if (cb(child, user_data)) {
                        ret = child;
                        break;
                }

                entry->child = entry->child->next;

                if (descend_closed ||
                    child->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT ||
                    (child->base.attributes &
                     PCX_AVT_OBJECT_ATTRIBUTE_CLOSED) == 0)
                        push_list_onto_stack(state, &child->contents);
        }

        return ret;
}

static bool
check_light_cb(struct pcx_avt_state_movable *movable,
               void *user_data)
{
        return (movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT &&
                (movable->base.attributes &
                 (PCX_AVT_OBJECT_ATTRIBUTE_LIT |
                  PCX_AVT_OBJECT_ATTRIBUTE_BURNING)) != 0);
}

static bool
check_light(struct pcx_avt_state *state)
{
       struct pcx_avt_state_room *room = state->rooms + state->current_room;

       if ((room->attributes & PCX_AVT_ROOM_ATTRIBUTE_LIT))
               return true;

       if ((room->attributes & PCX_AVT_ROOM_ATTRIBUTE_UNLIGHTABLE))
               return false;

       struct pcx_avt_state_movable *lit_movable;

       lit_movable = iterate_movables_in_list(state,
                                              &state->carrying,
                                              false, /* descend_closed */
                                              check_light_cb,
                                              NULL /* user_data */);

       if (lit_movable)
               return true;

       lit_movable = iterate_movables_in_list(state,
                                              &room->contents,
                                              false, /* descend_closed */
                                              check_light_cb,
                                              NULL /* user_data */);

       if (lit_movable)
               return true;

       return false;
}

static bool
is_movable_present(struct pcx_avt_state *state,
                   const struct pcx_avt_state_movable *base_movable)
{
        const struct pcx_avt_state_movable *movable = base_movable;

        while (true) {
                switch (movable->base.location_type) {
                case PCX_AVT_LOCATION_TYPE_IN_ROOM:
                        return (movable->base.location == state->current_room &&
                                check_light(state));
                case PCX_AVT_LOCATION_TYPE_CARRYING:
                        return true;
                case PCX_AVT_LOCATION_TYPE_NOWHERE:
                        return false;
                case PCX_AVT_LOCATION_TYPE_WITH_MONSTER:
                case PCX_AVT_LOCATION_TYPE_IN_OBJECT:
                        movable = movable->container;
                        if (movable == base_movable)
                                return false;
                        if (movable->type ==
                            PCX_AVT_STATE_MOVABLE_TYPE_OBJECT &&
                            (movable->base.attributes &
                             PCX_AVT_OBJECT_ATTRIBUTE_CLOSED))
                                return false;
                        continue;
                }

                return false;
        }
}

static bool
movable_should_be_listed(const struct pcx_avt_state_movable *movable)
{
        return (movable->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT ||
                (movable->base.attributes &
                 PCX_AVT_OBJECT_ATTRIBUTE_PORTABLE) != 0);
}

static void
add_room_contents_to_message(struct pcx_avt_state *state,
                             const struct pcx_avt_state_room *room)
{
        /* Count the number of movable objects, skipping out the ones
         * that shouldn’t be listed.
         */
        int n_items = 0;
        const struct pcx_avt_state_movable *movable;

        pcx_list_for_each(movable, &room->contents, location_node) {
                if (movable_should_be_listed(movable))
                        n_items++;
        }

        if (n_items == 0)
                return;

        add_message_string(state, " Vi vidas ");

        int item_num = 0;

        pcx_list_for_each(movable, &room->contents, location_node) {
                if (!movable_should_be_listed(movable))
                        continue;

                if (item_num > 0) {
                        add_message_string(state,
                                           item_num + 1 < n_items ?
                                           ", " :
                                           " kaj ");
                }

                add_movable_to_message(state, &movable->base, "n");

                item_num++;
        }

        add_message_c(state, '.');
}

static void
add_points(struct pcx_avt_state *state,
           int points)
{
        state->points += points;
}

static void
send_room_description(struct pcx_avt_state *state)
{
        struct pcx_avt_state_room *room = state->rooms + state->current_room;

        if (check_light(state)) {
                const char *desc =
                        state->avt->rooms[state->current_room].description;

                add_message_string(state, desc);


                add_room_contents_to_message(state, room);

                end_message(state);

                run_special_rules(state,
                                  "priskrib",
                                  NULL, /* object */
                                  NULL, /* tool */
                                  state->current_room);
        } else {
                send_message(state, "Estas mallume. Vi vidas nenion.");
        }

        if (!room->visited) {
                add_points(state,
                           state->avt->rooms[state->current_room].points);
                room->visited = true;
        }
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
disappear_movable(struct pcx_avt_state *state,
                  struct pcx_avt_state_movable *movable)
{
        pcx_list_remove(&movable->location_node);
        pcx_list_insert(state->nowhere.prev, &movable->location_node);
        movable->container = NULL;
        movable->base.location_type = PCX_AVT_LOCATION_TYPE_NOWHERE;
}

static void
put_movable_in_room(struct pcx_avt_state *state,
                    int room_number,
                    struct pcx_avt_state_movable *movable)
{
        struct pcx_avt_state_room *room = state->rooms + room_number;

        pcx_list_remove(&movable->location_node);
        pcx_list_insert(room->contents.prev, &movable->location_node);
        movable->container = NULL;
        movable->base.location_type = PCX_AVT_LOCATION_TYPE_IN_ROOM;
        movable->base.location = room_number;
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
copy_movable(struct pcx_avt_state_movable *dst,
             const struct pcx_avt_state_movable *src)
{
        if (dst == src)
                return;

        pcx_free(dst->base.adjective);
        pcx_free(dst->base.name);
        memcpy(&dst->base,
               &src->base,
               sizeof (*dst) - offsetof(struct pcx_avt_state_movable, base));
        dst->type = src->type;
        dst->base.adjective = pcx_strdup(dst->base.adjective);
        dst->base.name = pcx_strdup(dst->base.name);
}

static bool
check_condition(struct pcx_avt_state *state,
                const struct pcx_avt_condition_data *condition,
                struct pcx_avt_state_movable *movable,
                int room)
{
        switch (condition->condition) {
        case PCX_AVT_CONDITION_IN_ROOM:
                return room == condition->data;
        case PCX_AVT_CONDITION_OBJECT_IS:
                return movable == state->object_index[condition->data];
        case PCX_AVT_CONDITION_ANOTHER_OBJECT_PRESENT:
                return is_movable_present(state,
                                          state->object_index[condition->data]);
        case PCX_AVT_CONDITION_MONSTER_IS:
                return movable == state->monster_index[condition->data];
        case PCX_AVT_CONDITION_ANOTHER_MONSTER_PRESENT:
                movable = state->monster_index[condition->data];
                return is_movable_present(state, movable);
        case PCX_AVT_CONDITION_SHOTS:
                return (movable &&
                        movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT &&
                        movable->object.shots >= condition->data);
        case PCX_AVT_CONDITION_WEIGHT:
                return (movable &&
                        movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT &&
                        movable->object.weight >= condition->data);
        case PCX_AVT_CONDITION_SIZE:
                return (movable &&
                        movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT &&
                        movable->object.size >= condition->data);
        case PCX_AVT_CONDITION_CONTAINER_SIZE:
                return (movable &&
                        movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT &&
                        movable->object.container_size >= condition->data);
        case PCX_AVT_CONDITION_BURN_TIME:
                return (movable &&
                        movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT &&
                        movable->object.burn_time >= condition->data);
        case PCX_AVT_CONDITION_SOMETHING:
                return movable != NULL;
        case PCX_AVT_CONDITION_NOTHING:
                return movable == NULL;
        case PCX_AVT_CONDITION_NONE:
                return true;
        case PCX_AVT_CONDITION_OBJECT_ATTRIBUTE:
                return (movable &&
                        movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT &&
                        (movable->base.attributes & (1 << condition->data)));
        case PCX_AVT_CONDITION_NOT_OBJECT_ATTRIBUTE:
                return (movable &&
                        movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT &&
                        (movable->base.attributes &
                         (1 << condition->data)) == 0);
        case PCX_AVT_CONDITION_ROOM_ATTRIBUTE:
                return (state->rooms[room].attributes &
                        (1 << condition->data));
        case PCX_AVT_CONDITION_NOT_ROOM_ATTRIBUTE:
                return (state->rooms[room].attributes &
                        (1 << condition->data)) == 0;
        case PCX_AVT_CONDITION_MONSTER_ATTRIBUTE:
                return (movable &&
                        movable->type == PCX_AVT_STATE_MOVABLE_TYPE_MONSTER &&
                        (movable->base.attributes & (1 << condition->data)));
        case PCX_AVT_CONDITION_NOT_MONSTER_ATTRIBUTE:
                return (movable &&
                        movable->type == PCX_AVT_STATE_MOVABLE_TYPE_MONSTER &&
                        (movable->base.attributes &
                         (1 << condition->data)) == 0);
        case PCX_AVT_CONDITION_PLAYER_ATTRIBUTE:
                return (state->game_attributes & (1 << condition->data));
        case PCX_AVT_CONDITION_NOT_PLAYER_ATTRIBUTE:
                return (state->game_attributes & (1 << condition->data)) == 0;
        case PCX_AVT_CONDITION_CHANCE:
                return get_random(state) < condition->data;
        case PCX_AVT_CONDITION_OBJECT_SAME_ADJECTIVE:
                return (movable &&
                        !strcmp(movable->base.adjective,
                                state->object_index[condition->data]->
                                base.adjective));
        case PCX_AVT_CONDITION_OBJECT_SAME_NAME:
                return (movable &&
                        !strcmp(movable->base.name,
                                state->object_index[condition->data]->
                                base.name));
        case PCX_AVT_CONDITION_OBJECT_SAME_NOUN:
                return (movable &&
                        !strcmp(movable->base.adjective,
                                state->object_index[condition->data]->
                                base.adjective) &&
                        !strcmp(movable->base.name,
                                state->object_index[condition->data]->
                                base.name));
        case PCX_AVT_CONDITION_MONSTER_SAME_ADJECTIVE:
                return (movable &&
                        !strcmp(movable->base.adjective,
                                state->monster_index[condition->data]->
                                base.adjective));
        case PCX_AVT_CONDITION_MONSTER_SAME_NAME:
                return (movable &&
                        !strcmp(movable->base.name,
                                state->monster_index[condition->data]->
                                base.name));
        case PCX_AVT_CONDITION_MONSTER_SAME_NOUN:
                return (movable &&
                        !strcmp(movable->base.adjective,
                                state->monster_index[condition->data]->
                                base.adjective) &&
                        !strcmp(movable->base.name,
                                state->monster_index[condition->data]->
                                base.name));
        }

        return false;
}

static void
execute_action(struct pcx_avt_state *state,
               const struct pcx_avt_action_data *action,
               bool is_room,
               struct pcx_avt_state_movable *movable,
               int room)
{
        switch (action->action) {
        case PCX_AVT_ACTION_MOVE_TO:
                /* If the action is the room action then the player
                 * moves, otherwise the object moves.
                 */
                if (is_room) {
                        if (state->current_room != action->data) {
                                state->current_room = action->data;
                                send_room_description(state);
                        }
                } else {
                        put_movable_in_room(state,
                                            action->data,
                                            movable);
                }
                break;

        case PCX_AVT_ACTION_REPLACE_OBJECT:
        case PCX_AVT_ACTION_CREATE_OBJECT:
                /* Despite the documentation, in testing with the
                 * original interpreter these actions seem to do
                 * exactly the same thing. If there is an object then
                 * it disappears. The referenced object appears in the
                 * room, even if the original object was being held by
                 * the player.
                 */
                if (movable)
                        disappear_movable(state, movable);

                put_movable_in_room(state,
                                    room,
                                    state->object_index[action->data]);

                break;

        case PCX_AVT_ACTION_REPLACE_MONSTER:
        case PCX_AVT_ACTION_CREATE_MONSTER:
                if (movable)
                        disappear_movable(state, movable);

                put_movable_in_room(state,
                                    room,
                                    state->monster_index[action->data]);

                break;

        case PCX_AVT_ACTION_CHANGE_END:
                if (movable &&
                    movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT) {
                        movable->object.end = action->data;
                }
                break;
        case PCX_AVT_ACTION_CHANGE_SHOTS:
                if (movable &&
                    movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT) {
                        movable->object.shots = action->data;
                }
                break;
        case PCX_AVT_ACTION_CHANGE_WEIGHT:
                if (movable &&
                    movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT) {
                        movable->object.weight = action->data;
                }
                break;
        case PCX_AVT_ACTION_CHANGE_SIZE:
                if (movable &&
                    movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT) {
                        movable->object.size = action->data;
                }
                break;
        case PCX_AVT_ACTION_CHANGE_CONTAINER_SIZE:
                if (movable &&
                    movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT) {
                        movable->object.container_size = action->data;
                }
                break;
        case PCX_AVT_ACTION_CHANGE_BURN_TIME:
                if (movable &&
                    movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT) {
                        movable->object.burn_time = action->data;
                }
                break;
        case PCX_AVT_ACTION_NOTHING:
        case PCX_AVT_ACTION_NOTHING_ROOM:
                break;
        case PCX_AVT_ACTION_CARRY:
                /* The original interpreter seems to make the object
                 * disappear if you use this. Is that a bug? That
                 * doesn’t seem very useful so let’s just make it do
                 * what it seems like it should do.
                 */
                if (movable &&
                    movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT)
                        carry_movable(state, movable);
                break;
        case PCX_AVT_ACTION_SET_OBJECT_ATTRIBUTE:
                if (movable &&
                    movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT)
                        movable->base.attributes |= 1 << action->data;
                break;
        case PCX_AVT_ACTION_UNSET_OBJECT_ATTRIBUTE:
                if (movable &&
                    movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT)
                        movable->base.attributes &= ~(1 << action->data);
                break;
        case PCX_AVT_ACTION_SET_ROOM_ATTRIBUTE:
                state->rooms[room].attributes |=
                        1 << action->data;
                break;
        case PCX_AVT_ACTION_UNSET_ROOM_ATTRIBUTE:
                state->rooms[room].attributes &=
                        ~(1 << action->data);
                break;
        case PCX_AVT_ACTION_SET_MONSTER_ATTRIBUTE:
                if (movable &&
                    movable->type == PCX_AVT_STATE_MOVABLE_TYPE_MONSTER)
                        movable->base.attributes |= 1 << action->data;
                break;
        case PCX_AVT_ACTION_UNSET_MONSTER_ATTRIBUTE:
                if (movable &&
                    movable->type == PCX_AVT_STATE_MOVABLE_TYPE_MONSTER)
                        movable->base.attributes &= ~(1 << action->data);
                break;
        case PCX_AVT_ACTION_SET_PLAYER_ATTRIBUTE:
                state->game_attributes |= 1 << action->data;
                break;
        case PCX_AVT_ACTION_UNSET_PLAYER_ATTRIBUTE:
                state->game_attributes &= ~(1 << action->data);
                break;
        case PCX_AVT_ACTION_CHANGE_OBJECT_ADJECTIVE:
                if (movable) {
                        pcx_free(movable->base.adjective);
                        const struct pcx_avt_state_movable *other =
                                state->object_index[action->data];
                        movable->base.adjective =
                                pcx_strdup(other->base.adjective);
                }
                break;
        case PCX_AVT_ACTION_CHANGE_MONSTER_ADJECTIVE:
                if (movable) {
                        pcx_free(movable->base.adjective);
                        const struct pcx_avt_state_movable *other =
                                state->monster_index[action->data];
                        movable->base.adjective =
                                pcx_strdup(other->base.adjective);
                }
                break;
        case PCX_AVT_ACTION_CHANGE_OBJECT_NAME:
                if (movable) {
                        pcx_free(movable->base.name);
                        const struct pcx_avt_state_movable *other =
                                state->object_index[action->data];
                        movable->base.name =
                                pcx_strdup(other->base.name);
                }
                break;
        case PCX_AVT_ACTION_CHANGE_MONSTER_NAME:
                if (movable) {
                        pcx_free(movable->base.name);
                        const struct pcx_avt_state_movable *other =
                                state->monster_index[action->data];
                        movable->base.name =
                                pcx_strdup(other->base.name);
                }
                break;
        case PCX_AVT_ACTION_COPY_OBJECT:
                if (movable) {
                        copy_movable(movable,
                                     state->object_index[action->data]);
                }
                break;
        case PCX_AVT_ACTION_COPY_MONSTER:
                if (movable) {
                        copy_movable(movable,
                                     state->monster_index[action->data]);
                }
                break;
        }
}

static void
send_rule_message(struct pcx_avt_state *state,
                  const char *text,
                  struct pcx_avt_state_movable *object,
                  struct pcx_avt_state_movable *monster,
                  struct pcx_avt_state_movable *tool,
                  bool present)
{
        while (true) {
                const char *dollar = strchr(text, '$');

                if (dollar == NULL)
                        break;

                if (present)
                        add_message_data(state, text, dollar - text);

                switch (dollar[1]) {
                case '\0':
                        goto done;
                case 'A':
                        if (object) {
                                const char *suffix = NULL;

                                if (dollar[2] == 'n') {
                                        suffix = "n";
                                        dollar++;
                                }

                                if (present) {
                                        add_movable_to_message(state,
                                                               &object->base,
                                                               suffix);
                                }
                        }
                        break;
                case 'P':
                        if (tool) {
                                const char *suffix = NULL;

                                if (dollar[2] == 'n') {
                                        suffix = "n";
                                        dollar++;
                                }

                                if (present) {
                                        add_movable_to_message(state,
                                                               &tool->base,
                                                               suffix);
                                }
                        }
                        break;
                case 'M':
                        if (monster) {
                                const char *suffix = NULL;

                                if (dollar[2] == 'n') {
                                        suffix = "n";
                                        dollar++;
                                }

                                if (present) {
                                        add_movable_to_message(state,
                                                               &monster->base,
                                                               suffix);
                                }
                        }
                        break;
                case 'D':
                        if (present) {
                                enum pcx_avt_state_message_type type =
                                        PCX_AVT_STATE_MESSAGE_TYPE_DELAY;
                                start_message_type(state, type);
                        }
                        break;
                case 'F':
                        /* The monsters flee. FIXME */
                        break;
                case 'S':
                        /* Show the following even if the player isn’t
                         * present.
                         */
                        present = true;
                        break;
                case '$':
                        /* This isn’t mentioned in the docs but it
                         * seems like a good idea.
                         */
                        add_message_c(state, '$');
                        break;
                }

                text = dollar + 2;
        }

done:
        if (present)
                add_message_string(state, text);

        end_message(state);
}

static bool
run_rules(struct pcx_avt_state *state,
          const struct pcx_avt_command_word *verb,
          struct pcx_avt_state_movable *command_object,
          struct pcx_avt_state_movable *tool,
          int room)
{
        /* Prevent infinite recursion when rule actions trigger other rules.
         */
        if (state->rule_recursion_depth >= PCX_AVT_STATE_MAX_RECURSION_DEPTH)
                return false;

        bool executed_rule = false;

        struct pcx_avt_state_movable *object =
                (command_object &&
                 command_object->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT) ?
                command_object :
                NULL;
        struct pcx_avt_state_movable *monster =
                (command_object &&
                 command_object->type == PCX_AVT_STATE_MOVABLE_TYPE_MONSTER) ?
                command_object :
                NULL;

        for (size_t i = 0; i < state->avt->n_rules; i++) {
                const struct pcx_avt_rule *rule = state->avt->rules + i;

                if (!pcx_avt_command_word_equal(verb, rule->verb))
                        continue;

                if (check_condition(state,
                                    &rule->room_condition,
                                    command_object,
                                    room) &&
                    check_condition(state,
                                    &rule->object_condition,
                                    object,
                                    room) &&
                    check_condition(state,
                                    &rule->tool_condition,
                                    tool,
                                    room) &&
                    check_condition(state,
                                    &rule->monster_condition,
                                    monster,
                                    room)) {
                        state->rule_recursion_depth++;

                        if (rule->text) {
                                send_rule_message(state,
                                                  rule->text,
                                                  object,
                                                  monster,
                                                  tool,
                                                  state->current_room == room);
                        }

                        execute_action(state,
                                       &rule->room_action,
                                       true, /* is_room */
                                       command_object,
                                       room);
                        execute_action(state,
                                       &rule->object_action,
                                       false, /* is_room */
                                       object,
                                       room);
                        execute_action(state,
                                       &rule->tool_action,
                                       false, /* is_room */
                                       tool,
                                       room);
                        execute_action(state,
                                       &rule->monster_action,
                                       false, /* is_room */
                                       monster,
                                       room);

                        add_points(state, rule->points);

                        executed_rule = true;

                        state->rule_recursion_depth--;
                }
        }

        return executed_rule;
}

static bool
run_special_rules(struct pcx_avt_state *state,
                  const char *verb_str,
                  struct pcx_avt_state_movable *object,
                  struct pcx_avt_state_movable *tool,
                  int room)
{
        struct pcx_avt_command_word verb = {
                .start = verb_str,
                .length = strlen(verb_str),
        };

        return run_rules(state, &verb, object, tool, room);
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
                                            loc,
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

static void
send_end_game_messages(struct pcx_avt_state *state)
{
        struct pcx_avt_state_movable *movable;

        pcx_list_for_each(movable, &state->carrying, location_node) {
                if (movable->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT)
                        continue;

                add_points(state, movable->object.points);
        }

        add_message_string(state, "Vi kunportis ");

        if (pcx_list_empty(&state->carrying))
                add_message_string(state, "nenion");
        else
                add_movables_to_message(state, &state->carrying, "n");

        add_message_string(state,
                           ". "
                           "Vi havis ");

        if (state->points == 0)
                add_message_string(state, "neniun poenton");
        else if (state->points == 1)
                add_message_string(state, "unu poenton");
        else
                add_message_printf(state, "%i poentojn", state->points);

        add_message_c(state, '.');

        end_message(state);

        send_message(state, "Fino.");
}

static bool
check_game_over(struct pcx_avt_state *state)
{
        const struct pcx_avt_room *room =
                state->avt->rooms + state->current_room;

        if ((room->attributes & PCX_AVT_ROOM_ATTRIBUTE_GAME_OVER))
                state->game_over = true;

        return state->game_over;
}

static int
get_movable_room(struct pcx_avt_state *state,
                 const struct pcx_avt_state_movable *movable)
{
        while (true) {
                switch (movable->base.location_type) {
                case PCX_AVT_LOCATION_TYPE_IN_ROOM:
                        return movable->base.location;
                case PCX_AVT_LOCATION_TYPE_CARRYING:
                        return state->current_room;
                case PCX_AVT_LOCATION_TYPE_NOWHERE:
                        return -1;
                case PCX_AVT_LOCATION_TYPE_WITH_MONSTER:
                case PCX_AVT_LOCATION_TYPE_IN_OBJECT:
                        movable = movable->container;
                        continue;
                }

                return -1;
        }
}

static void
burn_out_object(struct pcx_avt_state *state,
                struct pcx_avt_state_movable *movable)
{
        int room = get_movable_room(state, movable);
        bool is_present = is_movable_present(state, movable);

        movable->base.attributes |= PCX_AVT_OBJECT_ATTRIBUTE_BURNT_OUT;

        disappear_movable(state, movable);

        if (is_present) {
                add_message_string(state, "La ");
                add_movable_to_message(state, &movable->base, NULL);
                add_message_string(state, " elbrulis.");
                end_message(state);
        }

        run_special_rules(state,
                          "fajrig",
                          movable,
                          NULL, /* tool */
                          room);
}

static void
object_after_command(struct pcx_avt_state *state,
                     struct pcx_avt_state_movable *movable)
{
        if ((movable->base.attributes & PCX_AVT_OBJECT_ATTRIBUTE_BURNING)) {
                if (movable->object.burn_time > 0)
                        movable->object.burn_time--;

                if (movable->object.burn_time <= 0) {
                        movable->base.attributes &=
                                ~PCX_AVT_OBJECT_ATTRIBUTE_BURNING;

                        if ((movable->base.attributes &
                             PCX_AVT_OBJECT_ATTRIBUTE_BURNT_OUT) == 0)
                                burn_out_object(state, movable);
                }
        }

        if (movable->object.end > 0 &&
            movable->base.location_type != PCX_AVT_LOCATION_TYPE_NOWHERE) {
                movable->object.end--;

                if (movable->object.end <= 0) {
                        run_special_rules(state,
                                          "fin",
                                          movable,
                                          NULL, /* tool */
                                          get_movable_room(state, movable));
                }
        }
}

static void
movables_after_command(struct pcx_avt_state *state)
{
        struct pcx_avt_state_movable *movable;

        pcx_list_for_each(movable, &state->all_movables, all_node) {
                switch (movable->type) {
                case PCX_AVT_STATE_MOVABLE_TYPE_OBJECT:
                        object_after_command(state, movable);
                        break;
                case PCX_AVT_STATE_MOVABLE_TYPE_MONSTER:
                        break;
                }

                if (check_game_over(state))
                        break;
        }
}

static void
after_command(struct pcx_avt_state *state)
{
        run_special_rules(state,
                          "est",
                          NULL, /* object */
                          NULL, /* tool */
                          state->current_room);

        if (!check_game_over(state))
                movables_after_command(state);

        if (check_game_over(state))
                send_end_game_messages(state);
}

static int
default_random_cb(void *user_data)
{
        return rand() % 100;
}

struct pcx_avt_state *
pcx_avt_state_new(const struct pcx_avt *avt)
{
        struct pcx_avt_state *state = pcx_calloc(sizeof *state);

        pcx_buffer_init(&state->message_buf);
        pcx_buffer_init(&state->stack);

        state->random_cb = default_random_cb;

        state->avt = avt;
        state->current_room = 0;

        pcx_list_init(&state->carrying);
        pcx_list_init(&state->nowhere);
        pcx_list_init(&state->all_movables);

        state->rooms = pcx_alloc(avt->n_rooms * sizeof *state->rooms);

        for (int i = 0; i < avt->n_rooms; i++) {
                pcx_list_init(&state->rooms[i].contents);
                state->rooms[i].attributes = avt->rooms[i].attributes;
                state->rooms[i].visited = false;
        }

        create_objects(state);
        create_monsters(state);

        position_movables(state);

        if (state->avt->introduction)
                send_message(state, "%s", state->avt->introduction);

        state->game_attributes = avt->game_attributes;

        send_room_description(state);

        after_command(state);

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

static const char *
get_capital_pronoun_name(enum pcx_avt_pronoun pronoun)
{
        switch (pronoun) {
        case PCX_AVT_PRONOUN_MAN:
                return "Li";
        case PCX_AVT_PRONOUN_WOMAN:
                return "Ŝi";
        case PCX_AVT_PRONOUN_PLURAL:
                return "Ili";
        case PCX_AVT_PRONOUN_ANIMAL:
                break;
        }

        return "Ĝi";
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

static struct pcx_avt_state_movable *
find_movable_via_alias(struct pcx_avt_state *state,
                       const struct pcx_avt_command_noun *noun)
{
        for (unsigned i = 0; i < state->avt->n_aliases; i++) {
                const struct pcx_avt_alias *alias = state->avt->aliases + i;

                if (noun->plural != alias->plural)
                        continue;

                if (!pcx_avt_command_word_equal(&noun->name, alias->name))
                        continue;

                struct pcx_avt_state_movable *movable = NULL;

                switch (alias->type) {
                case PCX_AVT_ALIAS_TYPE_OBJECT:
                        movable = state->object_index[alias->index];
                        break;
                case PCX_AVT_ALIAS_TYPE_MONSTER:
                        movable = state->monster_index[alias->index];
                        break;
                }

                if (noun->adjective.start &&
                    !pcx_avt_command_word_equal(&noun->adjective,
                                                movable->base.adjective))
                        continue;

                if (!is_movable_present(state, movable))
                        continue;

                return movable;
        }

        return NULL;
}

static bool
pronoun_can_reference(struct pcx_avt_state *state,
                      const struct pcx_avt_command_pronoun *pronoun,
                      const struct pcx_avt_state_movable *movable)
{
        switch (movable->base.pronoun) {
        case PCX_AVT_PRONOUN_MAN:
                if ((pronoun->genders & PCX_AVT_COMMAND_GENDER_MAN) == 0 ||
                    pronoun->plural)
                        return false;
                break;
        case PCX_AVT_PRONOUN_WOMAN:
                if ((pronoun->genders & PCX_AVT_COMMAND_GENDER_WOMAN) == 0 ||
                    pronoun->plural)
                        return false;
                break;
        case PCX_AVT_PRONOUN_ANIMAL:
                if ((pronoun->genders & PCX_AVT_COMMAND_GENDER_THING) == 0 ||
                    pronoun->plural)
                        return false;
                break;
        case PCX_AVT_PRONOUN_PLURAL:
                if (!pronoun->plural)
                        return false;
                break;
        }

        if (!is_movable_present(state, movable))
                return false;

        return true;
}

static struct pcx_avt_state_movable *
find_movable_by_pronoun(struct pcx_avt_state *state,
                        const struct pcx_avt_command_pronoun *pronoun)
{
        const struct pcx_avt_state_references *prev_refs =
                &state->previous_references;

        if (pronoun->person != 3)
                return NULL;

        if (prev_refs->object &&
            pronoun_can_reference(state, pronoun, prev_refs->object))
                return prev_refs->object;

        if (prev_refs->tool &&
            pronoun_can_reference(state, pronoun, prev_refs->tool))
                return prev_refs->tool;

        if (prev_refs->in &&
            pronoun_can_reference(state, pronoun, prev_refs->in))
                return prev_refs->in;

        if (prev_refs->direction &&
            pronoun_can_reference(state, pronoun, prev_refs->direction))
                return prev_refs->direction;

        return NULL;
}

static bool
find_movable_cb(struct pcx_avt_state_movable *movable,
                void *user_data)
{
        const struct pcx_avt_command_noun *noun = user_data;

        return movable_matches_noun(&movable->base, noun);
}

static struct pcx_avt_state_movable *
find_movable(struct pcx_avt_state *state,
             const struct pcx_avt_command_noun *noun)
{
        struct pcx_avt_state_movable *found;

        if (noun->is_pronoun)
                return find_movable_by_pronoun(state, &noun->pronoun);

        found = iterate_movables_in_list(state,
                                         &state->carrying,
                                         false, /* descend_closed */
                                         find_movable_cb,
                                         (void *) noun /* user_data */);
        if (found)
                return found;

        if (check_light(state)) {
                struct pcx_avt_state_room *room =
                        state->rooms + state->current_room;

                found = iterate_movables_in_list(state,
                                                 &room->contents,
                                                 false, /* descend_closed */
                                                 find_movable_cb,
                                                 (void *) noun /* user_data */);
                if (found)
                        return found;
        }

        found = find_movable_via_alias(state, noun);
        if (found)
                return found;

        return NULL;
}

static void
send_missing_reference_message(struct pcx_avt_state *state,
                               const struct pcx_avt_command_noun *noun)
{
        if (noun->is_pronoun) {
                add_message_string(state, "Mi ne scias kion “");
                add_word_to_message(state, &noun->name);
                add_message_string(state, "” indikas.");
        } else {
                add_message_string(state, "Vi ne vidas ");
                add_noun_to_message(state, noun, "n");

                if (!check_light(state)) {
                        add_message_string(state,
                                           " kaj estas tro mallume "
                                           "por serĉi");
                }

                add_message_c(state, '.');
        }

        end_message(state);
}

static struct pcx_avt_state_movable *
get_object_or_message(struct pcx_avt_state *state,
                      const struct pcx_avt_command *command,
                      const struct pcx_avt_state_references *references)
{
        if (references->object == NULL) {
                send_missing_reference_message(state, &command->object);
                return NULL;
        }

        return references->object;
}

static struct pcx_avt_state_movable *
get_tool_or_message(struct pcx_avt_state *state,
                    const struct pcx_avt_command *command,
                    const struct pcx_avt_state_references *references)
{
        if (references->tool == NULL) {
                send_missing_reference_message(state, &command->tool);
                return NULL;
        }

        return references->tool;
}

static struct pcx_avt_state_movable *
get_in_or_message(struct pcx_avt_state *state,
                  const struct pcx_avt_command *command,
                  const struct pcx_avt_state_references *references)
{
        if (references->in == NULL) {
                send_missing_reference_message(state, &command->in);
                return NULL;
        }

        return references->in;
}

static struct pcx_avt_state_movable *
get_direction_or_message(struct pcx_avt_state *state,
                    const struct pcx_avt_command *command,
                    const struct pcx_avt_state_references *references)
{
        if (references->direction == NULL) {
                send_missing_reference_message(state, &command->direction);
                return NULL;
        }

        return references->direction;
}

struct get_weight_closure {
        int weight;
};

static bool
get_weight_cb(struct pcx_avt_state_movable *movable,
              void *user_data)
{
        struct get_weight_closure *data = user_data;

        if (movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT)
                data->weight += movable->object.weight;

        return false;
}

static int
get_weight_of_list(struct pcx_avt_state *state,
                   struct pcx_list *list)
{
        struct get_weight_closure closure = { .weight = 0 };

        iterate_movables_in_list(state,
                                 list,
                                 true, /* descend_closed */
                                 get_weight_cb,
                                 &closure);

        return closure.weight;
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

static bool
handle_compass_direction(struct pcx_avt_state *state,
                         const struct pcx_avt_command_noun *noun)
{
        if (noun->plural || noun->is_pronoun)
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
                if (pcx_avt_command_word_equal(&noun->name,
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

        return false;
}

static bool
handle_named_direction(struct pcx_avt_state *state,
                       const struct pcx_avt_command_noun *noun)
{
        if (noun->adjective.start)
                return false;

        const struct pcx_avt_room *room =
                state->avt->rooms + state->current_room;

        for (int i = 0; i < room->n_directions; i++) {
                if (pcx_avt_command_word_equal(&noun->name,
                                               room->directions[i].name)) {
                        state->current_room = room->directions[i].target;
                        send_room_description(state);
                        return true;
                }
        }

        return false;
}

static bool
handle_direction(struct pcx_avt_state *state,
                 const struct pcx_avt_command *command,
                 const struct pcx_avt_state_references *references)
{
        /* Must have direction. Subject and verb optional. */
        if ((command->has & ~(PCX_AVT_COMMAND_HAS_SUBJECT |
                              PCX_AVT_COMMAND_HAS_VERB)) !=
            PCX_AVT_COMMAND_HAS_DIRECTION)
                return false;

        if (command->direction.adjective.start)
                return false;

        if ((command->has & PCX_AVT_COMMAND_HAS_SUBJECT) &&
            (!command->subject.is_pronoun ||
             command->subject.pronoun.person != 1 ||
             command->subject.pronoun.plural))
                return false;

        if ((command->has & PCX_AVT_COMMAND_HAS_VERB) &&
            !pcx_avt_command_word_equal(&command->verb, "ir"))
                return false;

        if (handle_compass_direction(state, &command->direction))
                return true;

        if (handle_named_direction(state, &command->direction))
                return true;

        return false;
}

static bool
handle_inventory(struct pcx_avt_state *state,
                 const struct pcx_avt_command *command,
                 const struct pcx_avt_state_references *references)
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

        add_message_string(state, "Vi kunportas ");

        if (pcx_list_empty(&state->carrying))
                add_message_string(state, "nenion");
        else
                add_movables_to_message(state, &state->carrying, "n");

        add_message_c(state, '.');

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

                add_message_string(state, description);
                end_message(state);

                return true;
        }

        return false;
}

static void
show_monster_contents(struct pcx_avt_state *state,
                      struct pcx_avt_state_movable *movable)
{
        if (movable->monster.aggression > 0) {
                const char *pronoun =
                        get_capital_pronoun_name(movable->base.pronoun);
                add_message_printf(state, " %s gardas ", pronoun);
                add_movables_to_message(state, &movable->contents, "n");
        } else {
                const char *pronoun =
                        get_pronoun_name(movable->base.pronoun);
                add_message_printf(state, " Apud %s estas ", pronoun);
                add_movables_to_message(state, &movable->contents, NULL);
        }

        add_message_c(state, '.');
}

static void
show_movable_with_monster(struct pcx_avt_state *state,
                          struct pcx_avt_state_movable *movable)
{
        if (movable->base.location_type != PCX_AVT_LOCATION_TYPE_WITH_MONSTER)
                return;

        const char *pronoun =
                get_capital_pronoun_name(movable->base.pronoun);
        add_message_printf(state, " %s estas apud ", pronoun);
        add_movable_to_message(state, &movable->container->base, NULL);
        add_message_c(state, '.');
}

static bool
handle_look(struct pcx_avt_state *state,
            const struct pcx_avt_command *command,
            const struct pcx_avt_state_references *references)
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
                get_object_or_message(state, command, references);

        if (movable == NULL)
                return true;

        if (movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT &&
            (movable->base.attributes & PCX_AVT_OBJECT_ATTRIBUTE_BURNING)) {
                const char *pronoun =
                        get_capital_pronoun_name(movable->base.pronoun);
                add_message_printf(state,
                                   "%s nun fajras kaj forbrulas.",
                                   pronoun);
        } else if (movable->base.description) {
                add_message_string(state, movable->base.description);
        } else {
                add_message_string(state, "Vi vidas nenion specialan pri la ");
                add_movable_to_message(state,
                                       &movable->base,
                                       NULL /* suffix */);
                add_message_c(state, '.');
        }

        show_movable_with_monster(state, movable);

        switch (movable->type) {
        case PCX_AVT_STATE_MOVABLE_TYPE_OBJECT:
                if ((movable->base.attributes &
                     PCX_AVT_OBJECT_ATTRIBUTE_CLOSED)) {
                        if ((movable->base.attributes &
                             PCX_AVT_OBJECT_ATTRIBUTE_CLOSABLE)) {
                                enum pcx_avt_pronoun pronoun =
                                        movable->base.pronoun;
                                const char *pronoun_name =
                                        get_capital_pronoun_name(pronoun);
                                add_message_printf(state,
                                                   " %s estas fermita",
                                                   pronoun_name);
                                if (movable->base.pronoun ==
                                    PCX_AVT_PRONOUN_PLURAL) {
                                        add_message_c(state, 'j');
                                }
                                add_message_c(state, '.');
                        }
                }
                else if (!pcx_list_empty(&movable->contents)) {
                        const char *pronoun =
                                get_pronoun_name(movable->base.pronoun);
                        add_message_printf(state, " En %s vi vidas ", pronoun);
                        add_movables_to_message(state, &movable->contents, "n");
                        add_message_c(state, '.');
                }
                break;
        case PCX_AVT_STATE_MOVABLE_TYPE_MONSTER:
                if (!pcx_list_empty(&movable->contents))
                        show_monster_contents(state, movable);
                break;
        }

        end_message(state);

        run_special_rules(state,
                          "rigard",
                          movable,
                          NULL, /* tool */
                          state->current_room);

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

static struct pcx_avt_state_movable *
find_aggressive_holder(struct pcx_avt_state_movable *movable)
{
        while (true) {
                movable = movable->container;

                if (movable == NULL)
                        return NULL;

                if (movable->type == PCX_AVT_STATE_MOVABLE_TYPE_MONSTER &&
                    movable->monster.aggression > 0)
                        return movable;
        }
}

static bool
validate_take(struct pcx_avt_state *state,
              struct pcx_avt_state_movable *movable)
{
        if (movable->base.location_type == PCX_AVT_LOCATION_TYPE_CARRYING) {
                add_message_string(state, "Vi jam portas la ");
                add_movable_to_message(state, &movable->base, "n");
                add_message_c(state, '.');
                end_message(state);
                return false;
        }

        if (movable->type == PCX_AVT_STATE_MOVABLE_TYPE_MONSTER ||
            (movable->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT &&
             (movable->base.attributes &
              PCX_AVT_OBJECT_ATTRIBUTE_PORTABLE) == 0)) {
                add_message_string(state, "Vi ne povas porti la ");
                add_movable_to_message(state, &movable->base, "n");
                add_message_c(state, '.');
                end_message(state);
                return false;
        }

        struct pcx_avt_state_movable *aggressive_holder =
                find_aggressive_holder(movable);
        if (aggressive_holder) {
                add_message_string(state, "La ");
                add_movable_to_message(state, &aggressive_holder->base, NULL);
                add_message_string(state, " malebligas al vi preni la ");
                add_movable_to_message(state, &movable->base, "n");
                add_message_c(state, '.');
                end_message(state);
                return false;
        }

        if (!is_carrying_movable(state, movable) &&
            get_weight_of_movable(state, movable) +
            get_weight_of_list(state, &state->carrying) >
            PCX_AVT_STATE_MAX_CARRYING_WEIGHT) {
                add_message_string(state,
                                   "Kune kun tio kion vi jam portas la ");
                add_movable_to_message(state, &movable->base, NULL);
                add_message_string(state, " estas tro peza");
                if (movable->base.pronoun == PCX_AVT_PRONOUN_PLURAL)
                        add_message_c(state, 'j');
                add_message_string(state, " por porti.");
                end_message(state);
                return false;
        }

        if (movable->object.size + get_carrying_size(state) >
            PCX_AVT_STATE_MAX_CARRYING_SIZE) {
                add_message_string(state,
                                   "Kune kun tio kion vi jam portas la ");
                add_movable_to_message(state, &movable->base, NULL);
                add_message_string(state, " estas tro granda");
                if (movable->base.pronoun == PCX_AVT_PRONOUN_PLURAL)
                        add_message_c(state, 'j');
                add_message_string(state, " por teni.");
                end_message(state);
                return false;
        }

        return true;
}

static bool
handle_take(struct pcx_avt_state *state,
            const struct pcx_avt_command *command,
            const struct pcx_avt_state_references *references)
{
        if (!is_verb_object_command(command))
                return false;

        if (!pcx_avt_command_word_equal(&command->verb, "pren"))
                return false;

        struct pcx_avt_state_movable *movable =
                get_object_or_message(state, command, references);

        if (movable == NULL)
                return true;

        if (!validate_take(state, movable))
                return true;

        if (!run_special_rules(state,
                               "pren",
                               movable,
                               NULL, /* tool */
                               state->current_room)) {
                carry_movable(state, movable);

                add_message_string(state, "Vi prenis la ");
                add_movable_to_message(state, &movable->base, "n");
                add_message_c(state, '.');
                end_message(state);
        }

        return true;
}

static bool
try_take(struct pcx_avt_state *state,
         struct pcx_avt_state_movable *movable)
{
        if (!validate_take(state, movable))
                return false;

        if (run_special_rules(state,
                              "pren",
                              movable,
                              NULL, /* tool */
                              state->current_room)) {
                /* The rule replaces the default action, but it might
                 * end up causing the object to be carried anyway.
                 */
                return (movable->base.location_type ==
                        PCX_AVT_LOCATION_TYPE_CARRYING);
        }

        carry_movable(state, movable);

        add_message_string(state, "Vi prenis la ");
        add_movable_to_message(state, &movable->base, "n");
        add_message_c(state, '.');
        end_message(state);

        return true;
}

static bool
ensure_carrying(struct pcx_avt_state *state,
                struct pcx_avt_state_movable *movable)
{
        if (movable->base.location_type == PCX_AVT_LOCATION_TYPE_CARRYING)
                return true;

        return try_take(state, movable);
}

static bool
handle_drop(struct pcx_avt_state *state,
            const struct pcx_avt_command *command,
            const struct pcx_avt_state_references *references)
{
        if (!is_verb_object_command(command))
                return false;

        if (!pcx_avt_command_word_equal(&command->verb, "ĵet") &&
            !pcx_avt_command_word_equal(&command->verb, "forĵet") &&
            !pcx_avt_command_word_equal(&command->verb, "falig") &&
            !pcx_avt_command_word_equal(&command->verb, "las"))
                return false;

        struct pcx_avt_state_movable *movable =
                get_object_or_message(state, command, references);

        if (movable == NULL)
                return true;

        if (movable->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT ||
            movable->base.location_type != PCX_AVT_LOCATION_TYPE_CARRYING) {
                add_message_string(state, "Vi ne portas la ");
                add_movable_to_message(state, &movable->base, "n");
                add_message_c(state, '.');
                end_message(state);
                return true;
        }

        if (!run_special_rules(state,
                               "ĵet",
                               movable,
                               NULL, /* tool */
                               state->current_room)) {
                put_movable_in_room(state,
                                    state->current_room,
                                    movable);

                add_message_string(state, "Vi ĵetis la ");
                add_movable_to_message(state, &movable->base, "n");
                add_message_c(state, '.');
                end_message(state);
        }

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
           const struct pcx_avt_command *command,
           const struct pcx_avt_state_references *references)
{
        if (!is_verb_command_and_has(command,
                                     PCX_AVT_COMMAND_HAS_OBJECT |
                                     PCX_AVT_COMMAND_HAS_IN))
                return false;

        if (!pcx_avt_command_word_equal(&command->verb, "met") &&
            !pcx_avt_command_word_equal(&command->verb, "enmet"))
                return false;

        struct pcx_avt_state_movable *container =
                get_in_or_message(state, command, references);
        if (container == NULL)
                return true;

        if (container->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT ||
            container->object.container_size <= 0) {
                add_message_string(state, "Vi ne povas meti ion en la ");
                add_movable_to_message(state, &container->base, "n");
                add_message_c(state, '.');
                end_message(state);
                return true;
        }

        if ((container->base.attributes & PCX_AVT_OBJECT_ATTRIBUTE_CLOSED)) {
                add_message_string(state, "Vi ne povas meti ion en la ");
                add_movable_to_message(state, &container->base, "n");
                const char *pronoun = get_pronoun_name(container->base.pronoun);
                add_message_printf(state, " ĉar %s estas fermita", pronoun);
                if (container->base.pronoun == PCX_AVT_PRONOUN_PLURAL)
                        add_message_c(state, 'j');
                add_message_c(state, '.');
                end_message(state);
                return true;
        }

        struct pcx_avt_state_movable *containee =
                get_object_or_message(state, command, references);
        if (containee == NULL || !ensure_carrying(state, containee))
                return true;

        if (container_would_create_cycle(container, containee)) {
                send_message(state, "Vi ne povas meti ion en sin mem.");
                return true;
        }

        if (get_movable_contents_size(container) +
            containee->object.size >
            container->object.container_size) {
                add_message_string(state, "La ");
                add_movable_to_message(state, &containee->base, NULL);
                add_message_string(state, " estas tro granda");
                if (containee->base.pronoun == PCX_AVT_PRONOUN_PLURAL)
                        add_message_c(state, 'j');
                add_message_string(state, " por eniri la ");
                add_movable_to_message(state, &container->base, "n");
                add_message_c(state, '.');
                end_message(state);
                return true;
        }

        reparent_movable(state, container, containee);

        add_message_string(state, "Vi metis la ");
        add_movable_to_message(state, &containee->base, "n");
        add_message_string(state, " en la ");
        add_movable_to_message(state, &container->base, "n");
        add_message_c(state, '.');
        end_message(state);

        return true;
}

static bool
handle_enter(struct pcx_avt_state *state,
             const struct pcx_avt_command *command,
             const struct pcx_avt_state_references *references)
{
        struct pcx_avt_state_movable *movable;

        if (is_verb_object_command(command) &&
            pcx_avt_command_word_equal(&command->verb, "enir")) {
                if (handle_named_direction(state, &command->object))
                        return true;
                movable = get_object_or_message(state, command, references);
        } else if (is_verb_command_and_has(command,
                                           PCX_AVT_COMMAND_HAS_IN) &&
                   (pcx_avt_command_word_equal(&command->verb, "enir") ||
                    pcx_avt_command_word_equal(&command->verb, "ir"))) {
                if (handle_named_direction(state, &command->in))
                        return true;
                movable = get_in_or_message(state, command, references);
        } else {
                return false;
        }

        if (movable == NULL)
                return true;

        if (run_special_rules(state,
                              "enir",
                              movable,
                              NULL, /* tool */
                              state->current_room))
                return true;

        if (movable->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT ||
            movable->object.enter_room == PCX_AVT_DIRECTION_BLOCKED) {
                add_message_string(state, "Vi ne povas eniri la ");
                add_movable_to_message(state, &movable->base, "n");
                add_message_c(state, '.');
                end_message(state);
                return true;
        }

        state->current_room = movable->object.enter_room;
        send_room_description(state);

        return true;
}

static bool
handle_exit(struct pcx_avt_state *state,
            const struct pcx_avt_command *command,
            const struct pcx_avt_state_references *references)
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
                  const struct pcx_avt_command *command,
                  const struct pcx_avt_state_references *references)
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
                get_object_or_message(state, command, references);

        if (movable == NULL)
                return true;

        if (run_rules(state,
                      &command->verb,
                      movable,
                      NULL, /* tool */
                      state->current_room))
                return true;

        if (movable->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT ||
            (movable->base.attributes &
             PCX_AVT_OBJECT_ATTRIBUTE_CLOSABLE) == 0) {
                add_message_string(state, "Vi ne povas ");
                add_word_to_message(state, &command->verb);
                add_message_string(state, "i la ");
                add_movable_to_message(state, &movable->base, "n");
                add_message_c(state, '.');
                end_message(state);
                return true;
        }

        if ((movable->base.attributes &
             PCX_AVT_OBJECT_ATTRIBUTE_CLOSED) == new_state) {
                add_message_string(state, "La ");
                add_movable_to_message(state, &movable->base, NULL);
                add_message_string(state, " jam estas ");
                if (new_state == 0)
                        add_message_string(state, "mal");
                add_message_string(state, "fermita");
                if (movable->base.pronoun == PCX_AVT_PRONOUN_PLURAL)
                        add_message_c(state, 'j');
                add_message_c(state, '.');
                end_message(state);
                return true;
        }

        movable->base.attributes = ((movable->base.attributes &
                                     ~PCX_AVT_OBJECT_ATTRIBUTE_CLOSED) |
                                    new_state);

        add_message_string(state, "Vi ");
        add_word_to_message(state, &command->verb);
        add_message_string(state, "is la ");
        add_movable_to_message(state, &movable->base, "n");
        add_message_c(state, '.');
        end_message(state);

        return true;
}

static bool
handle_read(struct pcx_avt_state *state,
            const struct pcx_avt_command *command,
            const struct pcx_avt_state_references *references)
{
        if (!is_verb_object_command(command))
                return false;

        if (!pcx_avt_command_word_equal(&command->verb, "leg"))
                return false;

        struct pcx_avt_state_movable *movable =
                get_object_or_message(state, command, references);

        if (movable == NULL)
                return true;

        if (run_special_rules(state,
                              "leg",
                              movable,
                              NULL, /* tool */
                              state->current_room))
                return true;

        if (movable->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT ||
            movable->object.read_text == NULL) {
                add_message_string(state, "Vi ne povas legi la ");
                add_movable_to_message(state, &movable->base, "n");
                add_message_c(state, '.');
                end_message(state);
                return true;
        }

        send_message(state, "%s", movable->object.read_text);

        return true;
}

static bool
handle_set_alight(struct pcx_avt_state *state,
                  const struct pcx_avt_command *command,
                  const struct pcx_avt_state_references *references)
{
        /* Must have verb and object. Subject and tool optional. */
        if ((command->has & ~(PCX_AVT_COMMAND_HAS_SUBJECT |
                              PCX_AVT_COMMAND_HAS_TOOL)) !=
            (PCX_AVT_COMMAND_HAS_VERB | PCX_AVT_COMMAND_HAS_OBJECT))
                return false;

        if ((command->has & PCX_AVT_COMMAND_HAS_SUBJECT) &&
            (!command->subject.is_pronoun ||
             command->subject.pronoun.person != 1 ||
             command->subject.pronoun.plural))
                return false;

        if (!pcx_avt_command_word_equal(&command->verb, "fajrig") &&
            !pcx_avt_command_word_equal(&command->verb, "brulig"))
                return false;

        struct pcx_avt_state_movable *object =
                get_object_or_message(state, command, references);

        if (object == NULL)
                return true;

        if ((command->has & PCX_AVT_COMMAND_HAS_TOOL) == 0) {
                add_message_string(state, "Per kio vi volas bruligi la ");
                add_movable_to_message(state, &object->base, "n");
                add_message_c(state, '?');
                end_message(state);
                return true;
        }

        struct pcx_avt_state_movable *tool =
                get_tool_or_message(state, command, references);

        if (tool == NULL || !ensure_carrying(state, tool))
                return true;

        if ((object->type == PCX_AVT_STATE_MOVABLE_TYPE_OBJECT) &&
            (object->base.attributes & PCX_AVT_OBJECT_ATTRIBUTE_BURNING)) {
                add_message_string(state, "La ");
                add_movable_to_message(state, &object->base, NULL);
                add_message_string(state, " jam brulas.");
                end_message(state);
                return true;
        }

        if ((object->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT) ||
            (object->base.attributes & (PCX_AVT_OBJECT_ATTRIBUTE_BURNING |
                                        PCX_AVT_OBJECT_ATTRIBUTE_BURNT_OUT |
                                        PCX_AVT_OBJECT_ATTRIBUTE_FLAMMABLE)) !=
            PCX_AVT_OBJECT_ATTRIBUTE_FLAMMABLE ||
            (tool->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT) ||
            (tool->base.attributes & PCX_AVT_OBJECT_ATTRIBUTE_LIGHTER) == 0) {
                add_message_string(state, "Vi ne povas bruligi la ");
                add_movable_to_message(state, &object->base, "n");
                add_message_string(state, " per la ");
                add_movable_to_message(state, &tool->base, NULL);
                add_message_c(state, '.');
                end_message(state);
                return true;
        }

        bool had_light = check_light(state);

        object->base.attributes |= PCX_AVT_OBJECT_ATTRIBUTE_BURNING;

        add_message_string(state, "La ");
        add_movable_to_message(state, &object->base, NULL);
        add_message_string(state, " ekbrulas.");
        end_message(state);

        if (!had_light)
                send_room_description(state);

        /* The verbs in the original interpreter seem to be the other
         * way around from what is described in the document.
         */
        run_special_rules(state,
                          "brulig",
                          object,
                          NULL, /* tool */
                          state->current_room);

        return true;
}

static bool
handle_turn_on_off(struct pcx_avt_state *state,
                   const struct pcx_avt_command *command,
                   const struct pcx_avt_state_references *references)
{
        if (!is_verb_object_command(command))
                return false;

        uint32_t new_state;

        if (pcx_avt_command_word_equal(&command->verb, "ŝalt") ||
            pcx_avt_command_word_equal(&command->verb, "lumig"))
                new_state = PCX_AVT_OBJECT_ATTRIBUTE_LIT;
        else if (pcx_avt_command_word_equal(&command->verb, "malŝalt") ||
                 pcx_avt_command_word_equal(&command->verb, "mallumig"))
                new_state = 0;
        else
                return false;

        struct pcx_avt_state_movable *movable =
                get_object_or_message(state, command, references);

        if (movable == NULL)
                return true;

        if (run_rules(state,
                      &command->verb,
                      movable,
                      NULL, /* tool */
                      state->current_room))
                return true;

        if (movable->type != PCX_AVT_STATE_MOVABLE_TYPE_OBJECT ||
            (movable->base.attributes &
             PCX_AVT_OBJECT_ATTRIBUTE_LIGHTABLE) == 0) {
                add_message_string(state, "Vi ne povas ");
                add_word_to_message(state, &command->verb);
                add_message_string(state, "i la ");
                add_movable_to_message(state, &movable->base, "n");
                add_message_c(state, '.');
                end_message(state);
                return true;
        }

        if ((movable->base.attributes &
             PCX_AVT_OBJECT_ATTRIBUTE_LIT) == new_state) {
                add_message_string(state, "La ");
                add_movable_to_message(state, &movable->base, NULL);
                add_message_string(state, " jam estas ");
                add_word_to_message(state, &command->verb);
                add_message_string(state, "ita");
                if (movable->base.pronoun == PCX_AVT_PRONOUN_PLURAL)
                        add_message_c(state, 'j');
                add_message_c(state, '.');
                end_message(state);
                return true;
        }

        bool had_light = check_light(state);

        movable->base.attributes = ((movable->base.attributes &
                                     ~PCX_AVT_OBJECT_ATTRIBUTE_LIT) |
                                    new_state);

        add_message_string(state, "Vi ");
        add_word_to_message(state, &command->verb);
        add_message_string(state, "is la ");
        add_movable_to_message(state, &movable->base, "n");
        add_message_c(state, '.');
        end_message(state);

        if (!had_light && new_state)
                send_room_description(state);

        return true;
}

static bool
handle_throw_to(struct pcx_avt_state *state,
                const struct pcx_avt_command *command,
                const struct pcx_avt_state_references *references)
{
        /* The original interpreter seems to have a special case for
         * “ĵeti X al Y”. If there is a rule then inexplicably X is
         * the tool and Y is the object. Otherwise it seems to just
         * drop the object. It also seems to do something weird if
         * there is a rule and in that case the objects just
         * disappear. We need to handle this in order to make a puzzle
         * work in the Texel sample game.
         */

        if (!is_verb_command_and_has(command,
                                     PCX_AVT_COMMAND_HAS_OBJECT |
                                     PCX_AVT_COMMAND_HAS_DIRECTION))
                return false;

        if (!pcx_avt_command_word_equal(&command->verb, "ĵet"))
                return false;

        struct pcx_avt_state_movable *target =
                get_direction_or_message(state, command, references);
        if (target == NULL)
                return true;

        struct pcx_avt_state_movable *projectile =
                get_object_or_message(state, command, references);
        if (projectile == NULL || !ensure_carrying(state, projectile))
                return true;

        if (run_special_rules(state,
                              "ĵet",
                              /* target is the object for some reason */
                              target,
                              /* projectile is the tool for some reason */
                              projectile,
                              state->current_room))
                return true;

        add_message_string(state, "Vi ne povas ĵeti la ");
        add_movable_to_message(state, &projectile->base, "n");
        add_message_string(state, " al la ");
        add_movable_to_message(state, &target->base, NULL);
        add_message_c(state, '.');
        end_message(state);

        return true;
}

static bool
handle_custom_command(struct pcx_avt_state *state,
                      const struct pcx_avt_command *command,
                      const struct pcx_avt_state_references *references)
{
        /* The custom rules only know how to handle the object and the
         * tool.
         */
        if ((command->has & ~(PCX_AVT_COMMAND_HAS_SUBJECT |
                              PCX_AVT_COMMAND_HAS_OBJECT |
                              PCX_AVT_COMMAND_HAS_TOOL)) !=
            PCX_AVT_COMMAND_HAS_VERB)
                return false;

        if ((command->has & PCX_AVT_COMMAND_HAS_SUBJECT) &&
            (!command->subject.is_pronoun ||
             command->subject.pronoun.person != 1 ||
             command->subject.pronoun.plural))
                return false;

        struct pcx_avt_state_movable *object = NULL;

        if ((command->has & PCX_AVT_COMMAND_HAS_OBJECT)) {
                object = get_object_or_message(state, command, references);
                if (object == NULL)
                        return true;
        }

        struct pcx_avt_state_movable *tool = NULL;

        if ((command->has & PCX_AVT_COMMAND_HAS_TOOL)) {
                tool = get_tool_or_message(state, command, references);
                if (tool == NULL)
                        return true;
        }

        return run_rules(state,
                         &command->verb,
                         object,
                         tool,
                         state->current_room);
}

static void
handle_command(struct pcx_avt_state *state,
               const struct pcx_avt_command *command,
               const struct pcx_avt_state_references *references)
{
        if (handle_direction(state, command, references))
                return;

        if (handle_look(state, command, references))
                return;

        if (handle_take(state, command, references))
                return;

        if (handle_drop(state, command, references))
                return;

        if (handle_put(state, command, references))
                return;

        if (handle_inventory(state, command, references))
                return;

        if (handle_enter(state, command, references))
                return;

        if (handle_exit(state, command, references))
                return;

        if (handle_open_close(state, command, references))
                return;

        if (handle_read(state, command, references))
                return;

        if (handle_set_alight(state, command, references))
                return;

        if (handle_turn_on_off(state, command, references))
                return;

        if (handle_throw_to(state, command, references))
                return;

        if (handle_custom_command(state, command, references))
                return;

        send_message(state, "Mi ne komprenas vin.");
}

static void
get_references(struct pcx_avt_state *state,
               const struct pcx_avt_command *command,
               struct pcx_avt_state_references *references)
{
        memset(references, 0, sizeof *references);

        if ((command->has & PCX_AVT_COMMAND_HAS_OBJECT))
                references->object = find_movable(state, &command->object);
        if ((command->has & PCX_AVT_COMMAND_HAS_TOOL))
                references->tool = find_movable(state, &command->tool);
        if ((command->has & PCX_AVT_COMMAND_HAS_IN))
                references->in = find_movable(state, &command->in);
        if ((command->has & PCX_AVT_COMMAND_HAS_DIRECTION)) {
                references->direction =
                        find_movable(state, &command->direction);
        }
}

void
pcx_avt_state_run_command(struct pcx_avt_state *state,
                          const char *command_str)
{
        struct pcx_avt_command command;
        struct pcx_avt_state_references references;

        /* Free up any messages that we’ve already reported to the
         * caller
         */
        memmove(state->message_buf.data,
                state->message_buf.data + state->message_buf_pos,
                state->message_buf.length - state->message_buf_pos);
        state->message_buf.length -= state->message_buf_pos;
        state->message_buf_pos = state->message_buf.length;

        state->rule_recursion_depth = 0;

        if (state->game_over) {
                send_message(state, "La ludo jam finiĝis.");
                return;
        }

        if (!pcx_avt_command_parse(command_str, &command)) {
                send_message(state, "Mi ne komprenas vin.");
                return;
        }

        get_references(state, &command, &references);

        handle_command(state, &command, &references);

        after_command(state);

        state->previous_references = references;
}

const struct pcx_avt_state_message *
pcx_avt_state_get_next_message(struct pcx_avt_state *state)
{
        if (state->message_buf_pos >= state->message_buf.length)
                return NULL;

        struct pcx_avt_state_message *message =
                (struct pcx_avt_state_message *)
                (state->message_buf.data +
                 state->message_buf_pos);

        state->message_buf_pos +=
                align_message_start(offsetof(struct pcx_avt_state_message,
                                             text) +
                                    strlen(message->text) + 1);

        return message;
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

bool
pcx_avt_state_game_is_over(struct pcx_avt_state *state)
{
        return state->game_over;
}

void
pcx_avt_state_set_random_cb(struct pcx_avt_state *state,
                            int (* cb)(void *),
                            void *user_data)
{
        state->random_cb = cb;
        state->random_cb_data = user_data;
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
