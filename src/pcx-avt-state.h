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

#ifndef PCX_AVT_STATE_H
#define PCX_AVT_STATE_H

#include "pcx-avt.h"

struct pcx_avt_state;

enum pcx_avt_state_message_type {
        PCX_AVT_STATE_MESSAGE_TYPE_NORMAL,
        /* The message should be displayed as part of the previous
         * message after a short delay.
         */
        PCX_AVT_STATE_MESSAGE_TYPE_DELAY,
};

struct pcx_avt_state_message {
        enum pcx_avt_state_message_type type;
        /* Null-terminated string containing the text */
        char text[];
};

struct pcx_avt_state *
pcx_avt_state_new(const struct pcx_avt *avt);

void
pcx_avt_state_run_command(struct pcx_avt_state *state,
                          const char *command);

/* Returns the next message in the queue or NULL if there are no more
 * messages. The returned data is owned by the pcx_avt_state and is
 * valid until the next call to pcx_avt_state_run_command.
 */
const struct pcx_avt_state_message *
pcx_avt_state_get_next_message(struct pcx_avt_state *state);

bool
pcx_avt_state_game_is_over(struct pcx_avt_state *state);

void
pcx_avt_state_free(struct pcx_avt_state *state);

#endif /* PCX_AVT_STATE_H */
