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

#include "pcx-avt-load-file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "pcx-avt-state.h"
#include "pcx-buffer.h"
#include "pcx-utf8.h"

struct data {
        struct pcx_buffer command_buffer;
        struct pcx_avt *avt;
        struct pcx_avt_state *state;
        FILE *input;
        int line_num;
        int random_number;
};

static int
random_cb(void *user_data)
{
        struct data *data = user_data;

        return data->random_number;
}

static void
create_avt_state(struct data *data)
{
        data->state = pcx_avt_state_new(data->avt);

        pcx_avt_state_set_random_cb(data->state,
                                    random_cb,
                                    data);
}

static bool
read_line(struct data *data)
{
        while (true) {
                pcx_buffer_ensure_size(&data->command_buffer,
                                       data->command_buffer.length + 64);

                char *read_start = (char *) (data->command_buffer.data +
                                             data->command_buffer.length);

                if (fgets(read_start,
                          data->command_buffer.size -
                          data->command_buffer.length -
                          1,
                          data->input) == NULL)
                        return false;

                size_t got = strlen(read_start);

                data->command_buffer.length += got;

                if (got > 0 && read_start[got - 1] == '\n') {
                        read_start[got - 1] = '\0';
                        return true;
                }
        }
}

static bool
ensure_empty_message_queue(struct data *data)
{
        const struct pcx_avt_state_message *msg =
                pcx_avt_state_get_next_message(data->state);

        if (msg == NULL)
                return true;

        fprintf(stderr,
                "Unexpected message received at line %i: %s\n",
                data->line_num,
                msg->text);

        return false;
}

static bool
check_room_name(struct data *data,
                const char *room_name)
{
        const char *state_room_name =
                pcx_avt_state_get_current_room_name(data->state);

        if (strcmp(room_name, state_room_name)) {
                fprintf(stderr,
                        "Wrong room name at line %i:\n"
                        " Expected: %s\n"
                        " Received: %s\n",
                        data->line_num,
                        room_name,
                        state_room_name);
                return false;
        }

        return true;
}

static bool
handle_test_command(struct data *data,
                    const char *command)
{
        if (!strcmp(command, "restart")) {
                if (!ensure_empty_message_queue(data))
                        return false;

                pcx_avt_state_free(data->state);
                create_avt_state(data);

                return true;
        } else if (!strcmp(command, "game_over")) {
                if (!pcx_avt_state_game_is_over(data->state)) {
                        fprintf(stderr,
                                "Game over expected but not reported at "
                                "line %i.\n",
                                data->line_num);
                        return false;
                }

                return true;
        } else if (!strcmp(command, "not_game_over")) {
                if (pcx_avt_state_game_is_over(data->state)) {
                        fprintf(stderr,
                                "Unexpected game over at line %i.\n",
                                data->line_num);
                        return false;
                }

                return true;
        } else if (!strncmp(command, "random ", 7)) {
                data->random_number = strtol(command + 7, NULL, 10);
                return true;
        } else if (!strncmp(command, "room ", 5)) {
                return check_room_name(data, command + 5);
        } else {
                fprintf(stderr,
                        "Unknown test command “%s” at line %i\n",
                        command,
                        data->line_num);
                return false;
        }
}

static bool
run_test(struct data *data)
{
        data->line_num = 0;

        while (true) {
                pcx_buffer_set_length(&data->command_buffer, 0);

                if (!read_line(data))
                        break;

                data->line_num++;

                char *command = (char *) data->command_buffer.data;
                size_t len = data->command_buffer.length;

                while (len > 0 && command[len - 1] == ' ')
                        len--;

                command[len] = '\0';

                while (*command == ' ')
                        command++;

                if (*command == '#' || *command == '\0')
                        continue;

                if (!pcx_utf8_is_valid_string(command)) {
                        fprintf(stderr,
                                "Invalid UTF-8 encountered in test script at "
                                "line %i",
                                data->line_num);
                        return false;
                }

                if (*command == '>') {
                        while (*(++command) == ' ');

                        if (!ensure_empty_message_queue(data))
                                return false;

                        pcx_avt_state_run_command(data->state, command);
                } else if (*command == '@') {
                        while (*(++command) == ' ');

                        if (!handle_test_command(data, command))
                                return false;
                } else {
                        const struct pcx_avt_state_message *msg =
                                pcx_avt_state_get_next_message(data->state);

                        if (msg == NULL) {
                                fprintf(stderr,
                                        "Expected message at line "
                                        "%i but none received\n",
                                        data->line_num);
                                return false;
                        } else if (strcmp(msg->text, command)) {
                                fprintf(stderr,
                                        "At line %i:\n"
                                        " Expected: %s\n"
                                        " Received: %s\n",
                                        data->line_num,
                                        command,
                                        msg->text);
                                return false;
                        }
                }
        }

        const struct pcx_avt_state_message *msg =
                pcx_avt_state_get_next_message(data->state);

        if (msg) {
                fprintf(stderr,
                        "Extra message received after test: %s\n",
                        msg->text);
                return false;
        }

        return true;
}

int
main(int argc, char **argv)
{
        if (argc != 2 && argc != 3) {
                fprintf(stderr, "usage: test-avt <avt-file> [test-script]\n");
                return EXIT_FAILURE;
        }

        struct data data = {
                .command_buffer = PCX_BUFFER_STATIC_INIT,
        };
        const char *avt_filename = argv[1];
        const char *test_script = argc > 2 ? argv[2] : NULL;
        struct pcx_error *error = NULL;
        int retval = EXIT_SUCCESS;

        data.avt = pcx_avt_load_file(avt_filename, &error);

        if (data.avt == NULL) {
                fprintf(stderr,
                        "%s: %s\n",
                        avt_filename,
                        error->message);
                pcx_error_free(error);
                retval = EXIT_FAILURE;
        } else {
                if (test_script) {
                        data.input = fopen(test_script, "rt");

                        if (data.input == NULL) {
                                fprintf(stderr,
                                        "%s: %s\n",
                                        test_script,
                                        strerror(errno));
                                retval = EXIT_FAILURE;
                        } else {
                                create_avt_state(&data);

                                if (!run_test(&data))
                                        retval = EXIT_FAILURE;

                                pcx_avt_state_free(data.state);

                                fclose(data.input);
                        }
                }

                pcx_avt_free(data.avt);
        }

        pcx_buffer_destroy(&data.command_buffer);

        return retval;
}
