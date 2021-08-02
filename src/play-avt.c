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

#define WRAP_WIDTH 78

struct data {
        struct pcx_buffer command_buffer;
        struct pcx_avt *avt;
        struct pcx_avt_state *state;
        int retval;
        /* The column number we have currently printed to */
        int col;
};

static const char *
get_word_end(const char *word)
{
        while (*word && *word != ' ' && *word != '\n')
                word++;

        return word;
}

static void
print_message(struct data *data,
              const char *message)
{
        while (true) {
                while (*message == ' ')
                        message++;

                if (*message == '\0')
                        break;

                if (*message == '\n') {
                        fputc('\n', stdout);
                        data->col = 0;
                        message++;
                        continue;
                }

                const char *word_end = get_word_end(message);

                const char *p = message;
                size_t word_length = 0;

                while (p < word_end) {
                        word_length++;
                        p = pcx_utf8_next(p);
                }

                if (data->col > 0 && data->col + word_length + 1 > WRAP_WIDTH) {
                        fputc('\n', stdout);
                        data->col = 0;
                }

                if (data->col > 0) {
                        fputc(' ', stdout);
                        data->col++;
                }

                fwrite(message, 1, word_end - message, stdout);

                data->col += word_length;

                message = word_end;
        }
}

static void
print_messages(struct data *data)
{
        const struct pcx_avt_state_message *message;

        while ((message = pcx_avt_state_get_next_message(data->state))) {
                if (message->type == PCX_AVT_STATE_MESSAGE_TYPE_NORMAL &&
                    data->col > 0) {
                        fputs("\n\n", stdout);
                        data->col = 0;
                }

                print_message(data, message->text);
        }

        fputs("\n\n", stdout);
        data->col = 0;
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
                          stdin) == NULL)
                        return false;

                size_t got = strlen(read_start);

                data->command_buffer.length += got;

                if (got > 0 && read_start[got - 1] == '\n') {
                        read_start[got - 1] = '\0';
                        return true;
                }
        }
}

static void
run_game(struct data *data)
{
        while (!pcx_avt_state_game_is_over(data->state)) {
                pcx_buffer_set_length(&data->command_buffer, 0);

                if (!read_line(data))
                        break;

                const char *command = (const char *) data->command_buffer.data;

                if (pcx_utf8_is_valid_string(command)) {
                        fputc('\n', stdout);

                        pcx_avt_state_run_command(data->state, command);

                        print_messages(data);
                }
        }
}

int
main(int argc, char **argv)
{
        if (argc != 2) {
                fprintf(stderr, "usage: play-avt <avt-file>\n");
                return EXIT_FAILURE;
        }

        struct data data = {
                .command_buffer = PCX_BUFFER_STATIC_INIT,
                .retval = EXIT_SUCCESS,
        };
        const char *avt_filename = argv[1];
        struct pcx_error *error = NULL;

        data.avt = pcx_avt_load_file(avt_filename, &error);

        if (data.avt == NULL) {
                fprintf(stderr,
                        "%s: %s\n",
                        avt_filename,
                        error->message);
                pcx_error_free(error);
                data.retval = EXIT_FAILURE;
        } else {
                printf("%s\n"
                       "© %s %s\n"
                       "\n",
                       data.avt->name,
                       data.avt->year,
                       data.avt->author);

                data.state = pcx_avt_state_new(data.avt);

                print_messages(&data);

                run_game(&data);

                pcx_avt_state_free(data.state);
                pcx_avt_free(data.avt);
        }

        pcx_buffer_destroy(&data.command_buffer);

        return data.retval;
}
