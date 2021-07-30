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

#include "pcx-load-or-parse.h"

#include <string.h>

#include "pcx-parser.h"
#include "pcx-avt-load.h"

struct pcx_avt *
pcx_load_or_parse(struct pcx_source *source,
                  struct pcx_error **error)
{
        char buf[16];

        if (source->read_source(source, buf, sizeof buf) == sizeof buf &&
            !memcmp(buf, "Aventur-programo", sizeof buf))
                return pcx_avt_load(source, error);

        if (!source->seek_source(source, 0, error))
                return NULL;

        return pcx_parser_parse(source, error);
}
