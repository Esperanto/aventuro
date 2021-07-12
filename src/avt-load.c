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

int
main(int argc, char **argv)
{
        int ret = EXIT_SUCCESS;

        for (int i = 1; i < argc; i++) {
                struct pcx_error *error = NULL;
                struct pcx_avt *avt = pcx_avt_load(argv[i], &error);

                if (avt == NULL) {
                        fprintf(stderr,
                                "%s: %s\n",
                                argv[i],
                                error->message);
                        pcx_error_free(error);
                        ret = EXIT_FAILURE;
                        break;
                }

                for (size_t i = 0; i < avt->n_rooms; i++) {
                        const struct pcx_avt_room *room = avt->rooms + i;

                        printf("%s: ", avt->rooms[i].name);

                        for (size_t j = 0; j < room->n_directions; j++) {
                                if (j > 0)
                                        printf(", ");
                                printf("%s->%s",
                                       room->directions[j].name,
                                       avt->rooms[room->directions[j].target].
                                       name);
                        }

                        fputc('\n', stdout);
                }

                pcx_avt_free(avt);
        }

        return ret;
}
