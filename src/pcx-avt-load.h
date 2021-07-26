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

#ifndef PCX_AVT_LOAD_H
#define PCX_AVT_LOAD_H

#include <stdbool.h>
#include <stdlib.h>

#include "pcx-avt.h"
#include "pcx-error.h"

extern struct pcx_error_domain
pcx_avt_load_error;

struct pcx_avt_load_source {
        bool (* seek_source)(struct pcx_avt_load_source *source,
                             long pos,
                             struct pcx_error **error);
        size_t (* read_source)(struct pcx_avt_load_source *source,
                               void *ptr,
                               size_t length);
};

enum pcx_avt_load_error {
        PCX_AVT_LOAD_ERROR_INVALID_STRING,
        PCX_AVT_LOAD_ERROR_INVALID_STRING_NUM,
        PCX_AVT_LOAD_ERROR_INVALID_ITEM_LIST,
        PCX_AVT_LOAD_ERROR_INVALID_ROOM,
        PCX_AVT_LOAD_ERROR_INVALID_DIRECTION,
        PCX_AVT_LOAD_ERROR_INVALID_PRONOUN,
        PCX_AVT_LOAD_ERROR_INVALID_LOCATION_TYPE,
        PCX_AVT_LOAD_ERROR_INVALID_INSIDENESS,
        PCX_AVT_LOAD_ERROR_INVALID_OBJECT,
        PCX_AVT_LOAD_ERROR_INVALID_MONSTER,
        PCX_AVT_LOAD_ERROR_INVALID_ATTRIBUTES,
        PCX_AVT_LOAD_ERROR_INVALID_VERB,
        PCX_AVT_LOAD_ERROR_INVALID_CONDITION,
        PCX_AVT_LOAD_ERROR_INVALID_ACTION,
        PCX_AVT_LOAD_ERROR_INVALID_ALIAS,
        PCX_AVT_LOAD_ERROR_INVALID_INFORMATION,
};

struct pcx_avt *
pcx_avt_load(struct pcx_avt_load_source *source,
             struct pcx_error **error);

#endif /* PCX_AVT_LOAD_H */
