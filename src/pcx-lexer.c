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

#include "pcx-lexer.h"

#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "pcx-buffer.h"
#include "pcx-utf8.h"

struct pcx_error_domain
pcx_lexer_error;

enum pcx_lexer_state {
        PCX_LEXER_STATE_SKIPPING_WHITESPACE,
        PCX_LEXER_STATE_SKIPPING_COMMENT,
        PCX_LEXER_STATE_READING_NUMBER,
        PCX_LEXER_STATE_READING_STRING,
        PCX_LEXER_STATE_READING_STRING_ESCAPE,
        PCX_LEXER_STATE_READING_SYMBOL,
};

struct pcx_lexer {
        struct pcx_buffer buffer;
        int line_num;
        struct pcx_source *source;

        enum pcx_lexer_state state;

        bool has_queued_token;
        struct pcx_lexer_token token;

        bool had_eof;

        uint8_t buf[128];
        int buf_pos;
        int buf_size;

        /* Array of char* */
        struct pcx_buffer symbols;

        int string_start_line;
};

struct pcx_lexer_builtin_symbol {
        const char *name;
        enum pcx_lexer_token_type type;
};

static const struct pcx_lexer_builtin_symbol
symbols[] = {
        { .name = "salono", .type = PCX_LEXER_TOKEN_TYPE_ROOM },
        { .name = "teksto", .type = PCX_LEXER_TOKEN_TYPE_TEXT, },
        { .name = "nomo", .type = PCX_LEXER_TOKEN_TYPE_NAME },
        { .name = "aŭtoro", .type = PCX_LEXER_TOKEN_TYPE_AUTHOR },
        { .name = "jaro", .type = PCX_LEXER_TOKEN_TYPE_YEAR },
        { .name = "enkonduko", .type = PCX_LEXER_TOKEN_TYPE_INTRODUCTION },
        { .name = "priskribo", .type = PCX_LEXER_TOKEN_TYPE_DESCRIPTION },
        { .name = "norden", .type = PCX_LEXER_TOKEN_TYPE_NORTH },
        { .name = "orienten", .type = PCX_LEXER_TOKEN_TYPE_EAST, },
        { .name = "suden", .type = PCX_LEXER_TOKEN_TYPE_SOUTH },
        { .name = "okcidenten", .type = PCX_LEXER_TOKEN_TYPE_WEST, },
        { .name = "supren", .type = PCX_LEXER_TOKEN_TYPE_UP,   },
        { .name = "suben", .type = PCX_LEXER_TOKEN_TYPE_DOWN, },
        { .name = "elen", .type = PCX_LEXER_TOKEN_TYPE_EXIT, },
        { .name = "luma", .type = PCX_LEXER_TOKEN_TYPE_LIT, },
        { .name = "nelumigebla", .type = PCX_LEXER_TOKEN_TYPE_UNLIGHTABLE, },
        { .name = "ludfino", .type = PCX_LEXER_TOKEN_TYPE_GAME_OVER, },
        { .name = "poentoj", .type = PCX_LEXER_TOKEN_TYPE_POINTS, },
        { .name = "eco", .type = PCX_LEXER_TOKEN_TYPE_ATTRIBUTE, },
        { .name = "nenio", .type = PCX_LEXER_TOKEN_TYPE_NENIO, },
        { .name = "direkto", .type = PCX_LEXER_TOKEN_TYPE_DIRECTION, },
};

static int
get_character(struct pcx_lexer *lexer)
{
        if (lexer->buf_pos >= lexer->buf_size) {
                if (lexer->had_eof)
                        return -1;

                lexer->buf_size = lexer->source->read_source(lexer->source,
                                                             lexer->buf,
                                                             sizeof lexer->buf);
                lexer->had_eof = lexer->buf_size < sizeof lexer->buf;
                lexer->buf_pos = 0;

                if (lexer->buf_size <= 0)
                        return -1;
        }

        return lexer->buf[lexer->buf_pos++];
}

static void
put_character(struct pcx_lexer *lexer, int ch)
{
        if (ch == -1) {
                assert(lexer->had_eof);
                assert(lexer->buf_pos >= lexer->buf_size);
                return;
        }

        assert(lexer->buf_pos > 0);

        if (ch == '\n')
                lexer->line_num--;

        lexer->buf[--lexer->buf_pos] = ch;
}

struct pcx_lexer *
pcx_lexer_new(struct pcx_source *source)
{
        struct pcx_lexer *lexer = pcx_alloc(sizeof *lexer);

        lexer->source = source;
        lexer->line_num = 1;
        lexer->had_eof = false;
        lexer->buf_pos = 0;
        lexer->buf_size = 0;
        lexer->has_queued_token = false;
        lexer->state = PCX_LEXER_STATE_SKIPPING_WHITESPACE;
        pcx_buffer_init(&lexer->symbols);
        pcx_buffer_init(&lexer->buffer);

        return lexer;
}

static bool
is_space(char ch)
{
        return ch && strchr(" \n\r\t", ch) != NULL;
}

static bool
normalize_string(struct pcx_lexer *lexer, struct pcx_error **error)
{
        char *str = (char *) lexer->buffer.data;
        char *src = str, *dst = str;

        /* Skip leading spaces */
        while (is_space(*src))
                src++;

        bool had_space = false;

        while (*src) {
                if (is_space(*src)) {
                        if (!had_space) {
                                *(dst++) = ' ';
                                had_space = true;
                        }
                } else {
                        had_space = false;
                        *(dst++) = *src;
                }

                src++;
        }

        if (dst > str && dst[-1] == ' ')
                dst--;

        *dst = '\0';

        if (!pcx_utf8_is_valid_string(str)) {
                pcx_set_error(error,
                              &pcx_lexer_error,
                              PCX_LEXER_ERROR_INVALID_STRING,
                              "String contains invalid UTF-8 at line %i",
                              lexer->line_num);
                return false;
        }

        return true;
}

static bool
find_symbol(struct pcx_lexer *lexer, struct pcx_error **error)
{
        const char *str = (const char *) lexer->buffer.data;

        if (!pcx_utf8_is_valid_string(str)) {
                pcx_set_error(error,
                              &pcx_lexer_error,
                              PCX_LEXER_ERROR_INVALID_SYMBOL,
                              "Invalid UTF-8 encountered at line %i",
                              lexer->line_num);
                return false;
        }

        for (size_t i = 0; i < PCX_N_ELEMENTS(symbols); i++) {
                if (!strcmp(symbols[i].name, str)) {
                        lexer->token.type = symbols[i].type;
                        return true;
                }
        }

        char **symbols = (char **) lexer->symbols.data;
        size_t n_symbols = lexer->symbols.length / sizeof (char *);

        for (size_t i = 0; i < n_symbols; i++) {
                if (!strcmp(symbols[i], str)) {
                        lexer->token.type = PCX_LEXER_TOKEN_TYPE_SYMBOL;
                        lexer->token.symbol_value = i + 1;
                        return true;
                }
        }

        char *symbol = pcx_strdup((const char *) lexer->buffer.data);
        pcx_buffer_append(&lexer->symbols, &symbol, sizeof symbol);

        lexer->token.type = PCX_LEXER_TOKEN_TYPE_SYMBOL;
        lexer->token.symbol_value = n_symbols + 1;

        return true;
}

void
pcx_lexer_put_token(struct pcx_lexer *lexer)
{
        lexer->has_queued_token = true;
}

const struct pcx_lexer_token *
pcx_lexer_get_token(struct pcx_lexer *lexer,
                    struct pcx_error **error)
{
        if (lexer->has_queued_token) {
                lexer->has_queued_token = false;
                return &lexer->token;
        }

        char *tail;
        const char *str;

        while (true) {
                int ch = get_character(lexer);

                if (ch == '\n')
                        lexer->line_num++;

                switch (lexer->state) {
                case PCX_LEXER_STATE_SKIPPING_WHITESPACE:
                        if (is_space(ch))
                                break;

                        pcx_buffer_set_length(&lexer->buffer, 0);

                        if ((ch >= '0' && ch <= '9') || ch == '-') {
                                put_character(lexer, ch);
                                lexer->state = PCX_LEXER_STATE_READING_NUMBER;
                        } else if ((ch >= 'a' && ch <= 'z') ||
                                   (ch >= 'A' && ch <= 'Z') ||
                                   ch >= 0x80 ||
                                   ch == '_') {
                                put_character(lexer, ch);
                                lexer->state = PCX_LEXER_STATE_READING_SYMBOL;
                        } else if (ch == '"') {
                                lexer->state = PCX_LEXER_STATE_READING_STRING;
                                lexer->string_start_line = lexer->line_num;
                        } else if (ch == '{') {
                                lexer->token.type =
                                        PCX_LEXER_TOKEN_TYPE_OPEN_BRACKET;
                                return &lexer->token;
                        } else if (ch == '}') {
                                lexer->token.type =
                                        PCX_LEXER_TOKEN_TYPE_CLOSE_BRACKET;
                                return &lexer->token;
                        } else if (ch == '#') {
                                lexer->state = PCX_LEXER_STATE_SKIPPING_COMMENT;
                        } else if (ch == -1) {
                                lexer->token.type =
                                        PCX_LEXER_TOKEN_TYPE_EOF;
                                return &lexer->token;
                        } else {
                                pcx_set_error(error,
                                              &pcx_lexer_error,
                                              PCX_LEXER_ERROR_UNEXPECTED_CHAR,
                                              "Unexpected character ‘%c’ on "
                                              "line %i",
                                              ch,
                                              lexer->line_num);
                                return NULL;
                        }
                        break;

                case PCX_LEXER_STATE_READING_NUMBER:
                        if ((ch >= '0' && ch <= '9') ||
                            (ch >= 'a' && ch <= 'z') ||
                            (ch >= 'A' && ch <= 'Z') ||
                            ch >= 0x80 ||
                            (lexer->buffer.length == 0 && ch == '-')) {
                                pcx_buffer_append_c(&lexer->buffer, ch);
                                break;
                        } else if (ch == '_') {
                                break;
                        }

                        put_character(lexer, ch);

                        pcx_buffer_append_c(&lexer->buffer, '\0');

                        str = (const char *) lexer->buffer.data;
                        errno = 0;
                        lexer->token.number_value = strtol(str, &tail, 10);

                        if (errno || *tail) {
                                pcx_set_error(error,
                                              &pcx_lexer_error,
                                              PCX_LEXER_ERROR_INVALID_NUMBER,
                                              "Invalid number “%s” on "
                                              "line %i",
                                              str,
                                              lexer->line_num);
                                return NULL;
                        }

                        lexer->token.type = PCX_LEXER_TOKEN_TYPE_NUMBER;
                        lexer->state = PCX_LEXER_STATE_SKIPPING_WHITESPACE;
                        return &lexer->token;

                case PCX_LEXER_STATE_READING_SYMBOL:
                        if ((ch >= '0' && ch <= '9') ||
                            (ch >= 'a' && ch <= 'z') ||
                            (ch >= 'A' && ch <= 'Z') ||
                            ch >= 0x80 || ch == '_') {
                                pcx_buffer_append_c(&lexer->buffer, ch);
                                break;
                        }

                        put_character(lexer, ch);
                        pcx_buffer_append_c(&lexer->buffer, '\0');
                        str = (const char *) lexer->buffer.data;

                        if (!find_symbol(lexer, error))
                                return NULL;

                        lexer->state = PCX_LEXER_STATE_SKIPPING_WHITESPACE;
                        return &lexer->token;

                case PCX_LEXER_STATE_READING_STRING:
                        if (ch == '\\') {
                                lexer->state =
                                        PCX_LEXER_STATE_READING_STRING_ESCAPE;
                                break;
                        } else if (ch == -1) {
                                pcx_set_error(error,
                                              &pcx_lexer_error,
                                              PCX_LEXER_ERROR_INVALID_STRING,
                                              "Unterminated string sequence "
                                              "at line %i",
                                              lexer->string_start_line);
                                return NULL;
                        } else if (ch != '"') {
                                pcx_buffer_append_c(&lexer->buffer, ch);
                                break;
                        }

                        pcx_buffer_append_c(&lexer->buffer, '\0');

                        if (!normalize_string(lexer, error))
                                return NULL;

                        lexer->state = PCX_LEXER_STATE_SKIPPING_WHITESPACE;
                        lexer->token.type = PCX_LEXER_TOKEN_TYPE_STRING;
                        lexer->token.string_value =
                                (const char *) lexer->buffer.data;
                        return &lexer->token;

                case PCX_LEXER_STATE_READING_STRING_ESCAPE:
                        if (ch == '"') {
                                pcx_buffer_append_c(&lexer->buffer, '"');
                                lexer->state = PCX_LEXER_STATE_READING_STRING;
                                break;
                        }
                        pcx_set_error(error,
                                      &pcx_lexer_error,
                                      PCX_LEXER_ERROR_INVALID_NUMBER,
                                      "Invalid escape sequence in string on "
                                      "line %i",
                                      lexer->line_num);
                        return NULL;

                case PCX_LEXER_STATE_SKIPPING_COMMENT:
                        if (ch == '\n') {
                                lexer->state =
                                        PCX_LEXER_STATE_SKIPPING_WHITESPACE;
                        }
                        break;
                }
        }
}

int
pcx_lexer_get_line_num(struct pcx_lexer *lexer)
{
        return lexer->line_num;
}

const char *
pcx_lexer_get_symbol_name(struct pcx_lexer *lexer,
                          int symbol_num)
{
        char **symbols = (char **) lexer->symbols.data;

        return symbols[symbol_num - 1];
}

void
pcx_lexer_free(struct pcx_lexer *lexer)
{
        char **symbols = (char **) lexer->symbols.data;
        size_t n_symbols = lexer->symbols.length / sizeof (char *);

        for (size_t i = 0; i < n_symbols; i++)
                pcx_free(symbols[i]);

        pcx_buffer_destroy(&lexer->symbols);

        pcx_buffer_destroy(&lexer->buffer);
        pcx_free(lexer);
}
