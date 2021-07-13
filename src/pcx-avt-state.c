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

struct pcx_avt_state {
        const struct pcx_avt *avt;

        int current_room;

        /* Queue of messages to report with
         * pcx_avt_state_get_next_message. Each message is a
         * zero-terminated string.
         */
        struct pcx_buffer message_buf;
        size_t message_buf_pos;
};

static PCX_PRINTF_FORMAT(2, 3) void
send_message(struct pcx_avt_state *state,
             const char *message,
             ...)
{
        va_list ap;

        va_start(ap, message);

        pcx_buffer_append_vprintf(&state->message_buf, message, ap);

        va_end(ap);

        /* Make the zero-termination be included in the length */
        state->message_buf.length++;
}

static void
send_room_description(struct pcx_avt_state *state)
{
        send_message(state,
                     "%s",
                     state->avt->rooms[state->current_room].description);
}

struct pcx_avt_state *
pcx_avt_state_new(const struct pcx_avt *avt)
{
        struct pcx_avt_state *state = pcx_calloc(sizeof *state);

        pcx_buffer_init(&state->message_buf);

        state->avt = avt;
        state->current_room = 0;

        send_room_description(state);

        return state;
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
        if (!command->has_direction ||
            command->has_object ||
            command->has_tool ||
            command->has_in)
                return false;

        if (command->has_subject &&
            (!command->subject.is_pronoun ||
             command->subject.pronoun.person != 1 ||
             command->subject.pronoun.plural))
                return false;

        if (command->has_verb &&
            !pcx_avt_command_word_equal(&command->verb, "ir"))
                return false;

        static const struct {
                const char *word;
                int direction;
        } direction_map[] = {
                { "nord", 0 },
                { "orient", 1 },
                { "sud", 2 },
                { "okcident", 3 },
                { "supr", 4 },
                { "malsupr", 5 },
                { "sub", 5 },
                { "el", 6 },
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

        return false;
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

        struct pcx_buffer *buf = &state->message_buf;

        if (command.has_subject)
                add_noun(buf, &command.subject);
        if (command.has_verb) {
                if (buf->length >= 1)
                        pcx_buffer_append_c(buf, ' ');
                pcx_buffer_append(buf,
                                  command.verb.start,
                                  command.verb.length);
                pcx_buffer_append_string(buf, "as");
        }
        if (command.has_object) {
                if (buf->length >= 1)
                        pcx_buffer_append_c(buf, ' ');
                add_noun(buf, &command.object);
        }
        if (command.has_direction) {
                if (buf->length >= 1)
                        pcx_buffer_append_c(buf, ' ');
                pcx_buffer_append(buf,
                                  command.direction.start,
                                  command.direction.length);
                pcx_buffer_append_string(buf, "en");
        }
        if (command.has_tool) {
                if (buf->length >= 1)
                        pcx_buffer_append_c(buf, ' ');
                pcx_buffer_append_string(buf, "per ");
                add_noun(buf, &command.tool);
        }
        if (command.has_in) {
                if (buf->length >= 1)
                        pcx_buffer_append_c(buf, ' ');
                pcx_buffer_append_string(buf, "en ");
                add_noun(buf, &command.in);
        }

        if (buf->length == 0)
                pcx_buffer_append_c(buf, '?');

        pcx_buffer_append_c(buf, '\0');
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

void
pcx_avt_state_free(struct pcx_avt_state *state)
{
        pcx_buffer_destroy(&state->message_buf);

        pcx_free(state);
}
