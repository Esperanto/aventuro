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

#include "pcx-avt-load.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
        bool quit;
        int retval;
};

static void
print_message(const char *message)
{
        int col = 0;

        while (true) {
                while (*message == ' ')
                        message++;

                if (*message == '\0')
                        break;

                const char *word_end = strchr(message, ' ');

                if (word_end == NULL)
                        word_end = message + strlen(message);

                const char *p = message;
                size_t word_length = 0;

                while (p < word_end) {
                        word_length++;
                        p = pcx_utf8_next(p);
                }

                if (col > 0 && col + word_length + 1 > WRAP_WIDTH) {
                        fputc('\n', stdout);
                        col = 0;
                }

                if (col > 0) {
                        fputc(' ', stdout);
                        col++;
                }

                fwrite(message, 1, word_end - message, stdout);

                col += word_length;

                message = word_end;
        }

        fputs("\n\n", stdout);
}

static void
print_messages(struct data *data)
{
        const char *message;

        while ((message = pcx_avt_state_get_next_message(data->state)))
                print_message(message);
}

static void
read_chunk(struct data *data)
{
        ssize_t got;

        pcx_buffer_ensure_size(&data->command_buffer,
                               data->command_buffer.length + 64);

        got = read(STDIN_FILENO,
                   data->command_buffer.data + data->command_buffer.length,
                   data->command_buffer.size - data->command_buffer.length);

        if (got == -1) {
                if (errno == EAGAIN || errno == EINTR)
                        return;
                fprintf(stderr,
                        "error reading from stdin: %s\n",
                        strerror(errno));
                data->quit = true;
                data->retval = EXIT_FAILURE;
                return;
        }

        if (got == 0) {
                data->quit = true;
                return;
        }

        data->command_buffer.length += got;

        size_t offset = 0;
        uint8_t *end;

        while ((end = memchr(data->command_buffer.data + offset,
                             '\n',
                             data->command_buffer.length - offset))) {
                size_t end_offset = end - data->command_buffer.data;

                *end = '\0';

                if (end_offset > offset && end[-1] == '\r')
                        end[-1] = '\0';

                const char *command =
                        (const char *) data->command_buffer.data + offset;

                fputc('\n', stdout);

                if (pcx_utf8_is_valid_string(command)) {
                        pcx_avt_state_run_command(data->state, command);

                        print_messages(data);
                }

                offset = end_offset + 1;
        }

        memmove(data->command_buffer.data,
                data->command_buffer.data + offset,
                data->command_buffer.length - offset);
        data->command_buffer.length -= offset;
}

static void
run_game(struct data *data)
{
        do
                read_chunk(data);
        while (!data->quit);
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
                .quit = false,
                .retval = EXIT_SUCCESS,
        };
        const char *avt_filename = argv[1];
        struct pcx_error *error = NULL;

        data.avt = pcx_avt_load(avt_filename, &error);

        if (data.avt == NULL) {
                fprintf(stderr,
                        "%s: %s\n",
                        avt_filename,
                        error->message);
                pcx_error_free(error);
                data.retval = EXIT_FAILURE;
        } else {
                data.state = pcx_avt_state_new(data.avt);

                print_messages(&data);

                run_game(&data);

                pcx_avt_state_free(data.state);
                pcx_avt_free(data.avt);
        }

        pcx_buffer_destroy(&data.command_buffer);

        return data.retval;
}
