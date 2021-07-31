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

enum pcx_lexer_keyword {
        PCX_LEXER_KEYWORD_ROOM = 1,
        PCX_LEXER_KEYWORD_TEXT,
        PCX_LEXER_KEYWORD_NAME,
        PCX_LEXER_KEYWORD_AUTHOR,
        PCX_LEXER_KEYWORD_YEAR,
        PCX_LEXER_KEYWORD_INTRODUCTION,
        PCX_LEXER_KEYWORD_DESCRIPTION,
        PCX_LEXER_KEYWORD_NORTH,
        PCX_LEXER_KEYWORD_EAST,
        PCX_LEXER_KEYWORD_SOUTH,
        PCX_LEXER_KEYWORD_WEST,
        PCX_LEXER_KEYWORD_UP,
        PCX_LEXER_KEYWORD_DOWN,
        PCX_LEXER_KEYWORD_EXIT,
        PCX_LEXER_KEYWORD_LIT,
        PCX_LEXER_KEYWORD_UNLIGHTABLE,
        PCX_LEXER_KEYWORD_GAME_OVER,
        PCX_LEXER_KEYWORD_POINTS,
        PCX_LEXER_KEYWORD_ATTRIBUTE,
        PCX_LEXER_KEYWORD_NOTHING,
        PCX_LEXER_KEYWORD_DIRECTION,
        PCX_LEXER_KEYWORD_OBJECT,
        PCX_LEXER_KEYWORD_WEIGHT,
        PCX_LEXER_KEYWORD_SIZE,
        PCX_LEXER_KEYWORD_CONTAINER_SIZE,
        PCX_LEXER_KEYWORD_SHOT_DAMAGE,
        PCX_LEXER_KEYWORD_SHOTS,
        PCX_LEXER_KEYWORD_HIT_DAMAGE,
        PCX_LEXER_KEYWORD_STAB_DAMAGE,
        PCX_LEXER_KEYWORD_FOOD_POINTS,
        PCX_LEXER_KEYWORD_TRINK_POINTS,
        PCX_LEXER_KEYWORD_BURN_TIME,
        PCX_LEXER_KEYWORD_END,
        PCX_LEXER_KEYWORD_LEGIBLE,
        PCX_LEXER_KEYWORD_INTO,
        PCX_LEXER_KEYWORD_LOCATION,
        PCX_LEXER_KEYWORD_MAN,
        PCX_LEXER_KEYWORD_WOMAN,
        PCX_LEXER_KEYWORD_ANIMAL,
        PCX_LEXER_KEYWORD_PLURAL,
        PCX_LEXER_KEYWORD_UNPORTABLE,
        PCX_LEXER_KEYWORD_PORTABLE,
        PCX_LEXER_KEYWORD_CLOSABLE,
        PCX_LEXER_KEYWORD_CLOSED,
        PCX_LEXER_KEYWORD_LIGHTABLE,
        PCX_LEXER_KEYWORD_OBJECT_LIT,
        PCX_LEXER_KEYWORD_FLAMMABLE,
        PCX_LEXER_KEYWORD_LIGHTER,
        PCX_LEXER_KEYWORD_BURNING,
        PCX_LEXER_KEYWORD_BURNT_OUT,
        PCX_LEXER_KEYWORD_EDIBLE,
        PCX_LEXER_KEYWORD_DRINKABLE,
        PCX_LEXER_KEYWORD_POISONOUS,
        PCX_LEXER_KEYWORD_CARRYING,
        PCX_LEXER_KEYWORD_ALIAS,
        PCX_LEXER_KEYWORD_VERB,
        PCX_LEXER_KEYWORD_MESSAGE,
        PCX_LEXER_KEYWORD_RULE,
        PCX_LEXER_KEYWORD_TOOL,
        PCX_LEXER_KEYWORD_CHANCE,
        PCX_LEXER_KEYWORD_PRESENT,
        PCX_LEXER_KEYWORD_ADJECTIVE,
        PCX_LEXER_KEYWORD_COPY,
        PCX_LEXER_KEYWORD_SOMETHING,
        PCX_LEXER_KEYWORD_UNSET,
        PCX_LEXER_KEYWORD_NEW,
        PCX_LEXER_KEYWORD_ELSEWHERE,

        PCX_LEXER_N_KEYWORDS,
};

enum pcx_lexer_token_type {
        PCX_LEXER_TOKEN_TYPE_OPEN_BRACKET,
        PCX_LEXER_TOKEN_TYPE_CLOSE_BRACKET,
        PCX_LEXER_TOKEN_TYPE_SYMBOL,
        PCX_LEXER_TOKEN_TYPE_STRING,
        PCX_LEXER_TOKEN_TYPE_NUMBER,
        PCX_LEXER_TOKEN_TYPE_EOF,
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
