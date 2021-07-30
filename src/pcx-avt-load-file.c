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
#include <string.h>
#include <errno.h>

#include "pcx-avt-load.h"
#include "pcx-list.h"
#include "pcx-file-error.h"

struct load_file_data {
        struct pcx_source source;
        FILE *file;
};

static bool
seek_file_source(struct pcx_source *source,
                 long pos,
                 struct pcx_error **error)
{
        struct load_file_data *data = pcx_container_of(source,
                                                       struct load_file_data,
                                                       source);

        int r = fseek(data->file, pos, SEEK_SET);

        if (r == -1) {
                pcx_file_error_set(error,
                                   errno,
                                   "%s",
                                   strerror(errno));
                return false;
        }

        return true;

}

static size_t
read_file_source(struct pcx_source *source,
                 void *ptr,
                 size_t length)
{
        struct load_file_data *data = pcx_container_of(source,
                                                       struct load_file_data,
                                                       source);

        return fread(ptr, 1, length, data->file);
}

struct pcx_avt *
pcx_avt_load_file(const char *filename,
                  struct pcx_error **error)
{
        struct load_file_data data = {
                .source = {
                        .seek_source = seek_file_source,
                        .read_source = read_file_source,
                },
        };

        data.file = fopen(filename, "rb");
        if (data.file == NULL) {
                pcx_file_error_set(error,
                                   errno,
                                   "%s",
                                   strerror(errno));
                return NULL;
        }

        struct pcx_avt *avt = pcx_avt_load(&data.source, error);

        fclose(data.file);

        return avt;
}
