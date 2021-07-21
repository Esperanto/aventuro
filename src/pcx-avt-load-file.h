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

#ifndef PCX_AVT_LOAD_FILE_H
#define PCX_AVT_LOAD_FILE_H

#include "pcx-avt-load.h"
#include "pcx-error.h"

struct pcx_avt *
pcx_avt_load_file(const char *filename,
                  struct pcx_error **error);

#endif /* PCX_AVT_LOAD_FILE_H */
