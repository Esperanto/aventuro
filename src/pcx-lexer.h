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

#ifndef PCX_LEXER
#define PCX_LEXER

#include "pcx-error.h"
#include "pcx-source.h"

extern struct pcx_error_domain
pcx_lexer_error;

enum pcx_lexer_error {
        PCX_LEXER_ERROR_INVALID_STRING,
        PCX_LEXER_ERROR_INVALID_SYMBOL,
        PCX_LEXER_ERROR_INVALID_NUMBER,
        PCX_LEXER_ERROR_UNEXPECTED_CHAR,
};

enum pcx_lexer_token_type {
        PCX_LEXER_TOKEN_TYPE_OPEN_BRACKET,
        PCX_LEXER_TOKEN_TYPE_CLOSE_BRACKET,
        PCX_LEXER_TOKEN_TYPE_SYMBOL,
        PCX_LEXER_TOKEN_TYPE_STRING,
        PCX_LEXER_TOKEN_TYPE_NUMBER,
        PCX_LEXER_TOKEN_TYPE_ROOM,
        PCX_LEXER_TOKEN_TYPE_TEXT,
        PCX_LEXER_TOKEN_TYPE_NAME,
        PCX_LEXER_TOKEN_TYPE_AUTHOR,
        PCX_LEXER_TOKEN_TYPE_YEAR,
        PCX_LEXER_TOKEN_TYPE_INTRODUCTION,
        PCX_LEXER_TOKEN_TYPE_DESCRIPTION,
        PCX_LEXER_TOKEN_TYPE_NORTH,
        PCX_LEXER_TOKEN_TYPE_EAST,
        PCX_LEXER_TOKEN_TYPE_SOUTH,
        PCX_LEXER_TOKEN_TYPE_WEST,
        PCX_LEXER_TOKEN_TYPE_UP,
        PCX_LEXER_TOKEN_TYPE_DOWN,
        PCX_LEXER_TOKEN_TYPE_EXIT,
        PCX_LEXER_TOKEN_TYPE_LIT,
        PCX_LEXER_TOKEN_TYPE_UNLIGHTABLE,
        PCX_LEXER_TOKEN_TYPE_GAME_OVER,
        PCX_LEXER_TOKEN_TYPE_POINTS,
        PCX_LEXER_TOKEN_TYPE_ATTRIBUTE,
        PCX_LEXER_TOKEN_TYPE_EOF,
        PCX_LEXER_TOKEN_TYPE_NENIO,
        PCX_LEXER_TOKEN_TYPE_DIRECTION,
        PCX_LEXER_TOKEN_TYPE_OBJECT,
        PCX_LEXER_TOKEN_TYPE_WEIGHT,
        PCX_LEXER_TOKEN_TYPE_SIZE,
        PCX_LEXER_TOKEN_TYPE_CONTAINER_SIZE,
        PCX_LEXER_TOKEN_TYPE_SHOT_DAMAGE,
        PCX_LEXER_TOKEN_TYPE_SHOTS,
        PCX_LEXER_TOKEN_TYPE_HIT_DAMAGE,
        PCX_LEXER_TOKEN_TYPE_STAB_DAMAGE,
        PCX_LEXER_TOKEN_TYPE_FOOD_POINTS,
        PCX_LEXER_TOKEN_TYPE_TRINK_POINTS,
        PCX_LEXER_TOKEN_TYPE_BURN_TIME,
        PCX_LEXER_TOKEN_TYPE_END,
        PCX_LEXER_TOKEN_TYPE_LEGIBLE,
        PCX_LEXER_TOKEN_TYPE_INTO,
        PCX_LEXER_TOKEN_TYPE_LOCATION,
        PCX_LEXER_TOKEN_TYPE_MAN,
        PCX_LEXER_TOKEN_TYPE_WOMAN,
        PCX_LEXER_TOKEN_TYPE_ANIMAL,
        PCX_LEXER_TOKEN_TYPE_PLURAL,
        PCX_LEXER_TOKEN_TYPE_PORTABLE,
        PCX_LEXER_TOKEN_TYPE_CLOSABLE,
        PCX_LEXER_TOKEN_TYPE_CLOSED,
        PCX_LEXER_TOKEN_TYPE_LIGHTABLE,
        PCX_LEXER_TOKEN_TYPE_OBJECT_LIT,
        PCX_LEXER_TOKEN_TYPE_FLAMMABLE,
        PCX_LEXER_TOKEN_TYPE_LIGHTER,
        PCX_LEXER_TOKEN_TYPE_BURNING,
        PCX_LEXER_TOKEN_TYPE_BURNT_OUT,
        PCX_LEXER_TOKEN_TYPE_EDIBLE,
        PCX_LEXER_TOKEN_TYPE_DRINKABLE,
        PCX_LEXER_TOKEN_TYPE_POISONOUS,
        PCX_LEXER_TOKEN_TYPE_CARRYING,
        PCX_LEXER_TOKEN_TYPE_ALIAS,
};

struct pcx_lexer_token {
        enum pcx_lexer_token_type type;

        union {
                long number_value;
                unsigned symbol_value;
                const char *string_value;
        };
};

struct pcx_lexer *
pcx_lexer_new(struct pcx_source *source);

const struct pcx_lexer_token *
pcx_lexer_get_token(struct pcx_lexer *lexer,
                    struct pcx_error **error);

void
pcx_lexer_put_token(struct pcx_lexer *lexer);

int
pcx_lexer_get_line_num(struct pcx_lexer *lexer);

const char *
pcx_lexer_get_symbol_name(struct pcx_lexer *lexer,
                          int symbol_num);

void
pcx_lexer_free(struct pcx_lexer *lexer);

#endif /* PCX_LEXER */
