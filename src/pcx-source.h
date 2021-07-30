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

#ifndef PCX_SOURCE_H
#define PCX_SOURCE_H

#include <stdbool.h>
#include <stdlib.h>

#include "pcx-error.h"

struct pcx_source {
        bool (* seek_source)(struct pcx_source *source,
                             long pos,
                             struct pcx_error **error);
        size_t (* read_source)(struct pcx_source *source,
                               void *ptr,
                               size_t length);
};

#endif /* PCX_SOURCE_H */
