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

static const char * const
keywords[] = {
        [PCX_LEXER_KEYWORD_ROOM] = "ejo",
        [PCX_LEXER_KEYWORD_TEXT] = "teksto",
        [PCX_LEXER_KEYWORD_NAME] = "nomo",
        [PCX_LEXER_KEYWORD_AUTHOR] = "aŭtoro",
        [PCX_LEXER_KEYWORD_YEAR] = "jaro",
        [PCX_LEXER_KEYWORD_INTRODUCTION] = "enkonduko",
        [PCX_LEXER_KEYWORD_DESCRIPTION] = "priskribo",
        [PCX_LEXER_KEYWORD_NORTH] = "norden",
        [PCX_LEXER_KEYWORD_EAST] = "orienten",
        [PCX_LEXER_KEYWORD_SOUTH] = "suden",
        [PCX_LEXER_KEYWORD_WEST] = "okcidenten",
        [PCX_LEXER_KEYWORD_UP] = "supren",
        [PCX_LEXER_KEYWORD_DOWN] = "suben",
        [PCX_LEXER_KEYWORD_EXIT] = "elen",
        [PCX_LEXER_KEYWORD_LIT] = "luma",
        [PCX_LEXER_KEYWORD_UNLIGHTABLE] = "nelumigebla",
        [PCX_LEXER_KEYWORD_GAME_OVER] = "ludfino",
        [PCX_LEXER_KEYWORD_POINTS] = "poentoj",
        [PCX_LEXER_KEYWORD_ATTRIBUTE] = "eco",
        [PCX_LEXER_KEYWORD_NOTHING] = "nenio",
        [PCX_LEXER_KEYWORD_DIRECTION] = "direkto",
        [PCX_LEXER_KEYWORD_CONTAINER] = "ujo",
        [PCX_LEXER_KEYWORD_OBJECT] = "aĵo",
        [PCX_LEXER_KEYWORD_WEIGHT] = "pezo",
        [PCX_LEXER_KEYWORD_SIZE] = "grando",
        [PCX_LEXER_KEYWORD_CONTAINER_SIZE] = "enhavo",
        [PCX_LEXER_KEYWORD_SHOT_DAMAGE] = "perpafo",
        [PCX_LEXER_KEYWORD_SHOTS] = "ŝargo",
        [PCX_LEXER_KEYWORD_HIT_DAMAGE] = "perbato",
        [PCX_LEXER_KEYWORD_STAB_DAMAGE] = "perpiko",
        [PCX_LEXER_KEYWORD_FOOD_POINTS] = "manĝeblo",
        [PCX_LEXER_KEYWORD_DRINK_POINTS] = "trinkeblo",
        [PCX_LEXER_KEYWORD_BURN_TIME] = "fajrodaŭro",
        [PCX_LEXER_KEYWORD_END] = "fino",
        [PCX_LEXER_KEYWORD_LEGIBLE] = "legebla",
        [PCX_LEXER_KEYWORD_INTO] = "enen",
        [PCX_LEXER_KEYWORD_LOCATION] = "loko",
        [PCX_LEXER_KEYWORD_MAN] = "viro",
        [PCX_LEXER_KEYWORD_WOMAN] = "ino",
        [PCX_LEXER_KEYWORD_ANIMAL] = "besto",
        [PCX_LEXER_KEYWORD_PLURAL] = "pluralo",
        [PCX_LEXER_KEYWORD_UNPORTABLE] = "neportebla",
        [PCX_LEXER_KEYWORD_PORTABLE] = "portebla",
        [PCX_LEXER_KEYWORD_CLOSABLE] = "fermebla",
        [PCX_LEXER_KEYWORD_CLOSED] = "fermita",
        [PCX_LEXER_KEYWORD_LIGHTABLE] = "lumigebla",
        [PCX_LEXER_KEYWORD_OBJECT_LIT] = "lumigita",
        [PCX_LEXER_KEYWORD_FLAMMABLE] = "fajrebla",
        [PCX_LEXER_KEYWORD_LIGHTER] = "fajrilo",
        [PCX_LEXER_KEYWORD_BURNING] = "brulanta",
        [PCX_LEXER_KEYWORD_BURNT_OUT] = "bruligita",
        [PCX_LEXER_KEYWORD_EDIBLE] = "manĝebla",
        [PCX_LEXER_KEYWORD_DRINKABLE] = "trinkebla",
        [PCX_LEXER_KEYWORD_POISONOUS] = "venena",
        [PCX_LEXER_KEYWORD_CARRYING] = "kunportata",
        [PCX_LEXER_KEYWORD_ALIAS] = "alinomo",
        [PCX_LEXER_KEYWORD_VERB] = "verbo",
        [PCX_LEXER_KEYWORD_MESSAGE] = "mesaĝo",
        [PCX_LEXER_KEYWORD_RULE] = "fenomeno",
        [PCX_LEXER_KEYWORD_TOOL] = "pero",
        [PCX_LEXER_KEYWORD_CHANCE] = "ŝanco",
        [PCX_LEXER_KEYWORD_PRESENT] = "ĉeestas",
        [PCX_LEXER_KEYWORD_ADJECTIVE] = "adjektivo",
        [PCX_LEXER_KEYWORD_COPY] = "kopio",
        [PCX_LEXER_KEYWORD_SOMETHING] = "io",
        [PCX_LEXER_KEYWORD_UNSET] = "malvera",
        [PCX_LEXER_KEYWORD_NEW] = "nova",
        [PCX_LEXER_KEYWORD_ELSEWHERE] = "alien",
        [PCX_LEXER_KEYWORD_APPEAR] = "apero",
};

_Static_assert(PCX_N_ELEMENTS(keywords) == PCX_LEXER_N_KEYWORDS,
               "Keyword is a missing a name");

static void
set_verror(struct pcx_lexer *lexer,
           struct pcx_error **error,
           enum pcx_lexer_error code,
           int line_num,
           const char *format,
           va_list ap)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        pcx_buffer_append_printf(&buf,
                                 "linio %i: ",
                                 line_num);

        pcx_buffer_append_vprintf(&buf, format, ap);

        pcx_set_error(error,
                      &pcx_lexer_error,
                      code,
                      "%s",
                      (const char *) buf.data);

        pcx_buffer_destroy(&buf);
}

PCX_PRINTF_FORMAT(4, 5)
static void
set_error(struct pcx_lexer *lexer,
           struct pcx_error **error,
           enum pcx_lexer_error code,
           const char *format,
           ...)
{
        va_list ap;

        va_start(ap, format);
        set_verror(lexer, error, code, lexer->line_num, format, ap);
        va_end(ap);
}

PCX_PRINTF_FORMAT(5, 6)
static void
set_error_with_line(struct pcx_lexer *lexer,
                    struct pcx_error **error,
                    enum pcx_lexer_error code,
                    int line_num,
                    const char *format,
                    ...)
{
        va_list ap;

        va_start(ap, format);
        set_verror(lexer, error, code, line_num, format, ap);
        va_end(ap);
}

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

        enum {
                start,
                had_space,
                had_newline,
                had_other,
        } state = start;

        int newline_count = 0;

        while (*src) {
                switch (state) {
                case start:
                        if (!is_space(*src)) {
                                *(dst++) = *src;
                                state = had_other;
                        }
                        break;
                case had_space:
                        if (*src == '\n') {
                                state = had_newline;
                                newline_count = 1;
                        } else if (!is_space(*src)) {
                                *(dst++) = ' ';
                                *(dst++) = *src;
                                state = had_other;
                        }
                        break;
                case had_newline:
                        if (*src == '\n') {
                                newline_count++;
                        } else if (!is_space(*src)) {
                                if (newline_count == 1) {
                                        *(dst++) = ' ';
                                } else {
                                        for (int i = 0; i < newline_count; i++)
                                                *(dst++) = '\n';
                                }
                                *(dst++) = *src;
                                state = had_other;
                        }
                        break;
                case had_other:
                        if (*src == '\n') {
                                state = had_newline;
                                newline_count = 1;
                        } else if (is_space(*src)) {
                                state = had_space;
                        } else {
                                *(dst++) = *src;
                        }
                        break;
                }

                src++;
        }

        *dst = '\0';

        if (!pcx_utf8_is_valid_string(str)) {
                set_error(lexer,
                          error,
                          PCX_LEXER_ERROR_INVALID_STRING,
                          "Teksto enhavas nevalidan UTF-8");
                return false;
        }

        return true;
}

static bool
find_symbol(struct pcx_lexer *lexer, struct pcx_error **error)
{
        const char *str = (const char *) lexer->buffer.data;

        if (!pcx_utf8_is_valid_string(str)) {
                set_error(lexer,
                          error,
                          PCX_LEXER_ERROR_INVALID_SYMBOL,
                          "Trovis nevalidan UTF-8");
                return false;
        }

        for (size_t i = 1; i < PCX_LEXER_N_KEYWORDS; i++) {
                if (!strcmp(keywords[i], str)) {
                        lexer->token.type = PCX_LEXER_TOKEN_TYPE_SYMBOL;
                        lexer->token.symbol_value = i;
                        return true;
                }
        }

        char **symbols = (char **) lexer->symbols.data;
        size_t n_symbols = lexer->symbols.length / sizeof (char *);

        for (size_t i = 0; i < n_symbols; i++) {
                if (!strcmp(symbols[i], str)) {
                        lexer->token.type = PCX_LEXER_TOKEN_TYPE_SYMBOL;
                        lexer->token.symbol_value = i + PCX_LEXER_N_KEYWORDS;
                        return true;
                }
        }

        char *symbol = pcx_strdup((const char *) lexer->buffer.data);
        pcx_buffer_append(&lexer->symbols, &symbol, sizeof symbol);

        lexer->token.type = PCX_LEXER_TOKEN_TYPE_SYMBOL;
        lexer->token.symbol_value = n_symbols + PCX_LEXER_N_KEYWORDS;

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
                                set_error(lexer,
                                          error,
                                          PCX_LEXER_ERROR_UNEXPECTED_CHAR,
                                          "Neatendita signo ‘%c’",
                                          ch);
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
                                set_error(lexer,
                                          error,
                                          PCX_LEXER_ERROR_INVALID_NUMBER,
                                          "Nevalida numero “%s”",
                                          str);
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
                                set_error_with_line
                                        (lexer,
                                         error,
                                         PCX_LEXER_ERROR_INVALID_STRING,
                                         lexer->string_start_line,
                                         "Senfina teksto");
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
                        if (ch == '"' || ch == '\\') {
                                pcx_buffer_append_c(&lexer->buffer, ch);
                                lexer->state = PCX_LEXER_STATE_READING_STRING;
                                break;
                        }
                        set_error(lexer,
                                  error,
                                  PCX_LEXER_ERROR_INVALID_NUMBER,
                                  "Nevalida kodŝanĝa signo");
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
        if (symbol_num < PCX_LEXER_N_KEYWORDS) {
                return keywords[symbol_num];
        } else {
                char **symbols = (char **) lexer->symbols.data;

                return symbols[symbol_num - PCX_LEXER_N_KEYWORDS];
        }
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
