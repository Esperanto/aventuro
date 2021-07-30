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

#ifndef PCX_PARSER
#define PCX_PARSER

#include "pcx-error.h"
#include "pcx-avt.h"
#include "pcx-source.h"

extern struct pcx_error_domain
pcx_parser_error;

enum pcx_parser_error {
        PCX_PARSER_ERROR_INVALID,
};

struct pcx_avt *
pcx_parser_parse(struct pcx_source *source,
                 struct pcx_error **error);

#endif /* PCX_PARSER */
