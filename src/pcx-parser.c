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

#include "pcx-parser.h"

#include <string.h>

#include "pcx-lexer.h"
#include "pcx-list.h"
#include "pcx-buffer.h"

struct pcx_error_domain
pcx_parser_error;

enum pcx_parser_return {
        PCX_PARSER_RETURN_OK,
        PCX_PARSER_RETURN_NOT_MATCHED,
        PCX_PARSER_RETURN_ERROR,
};

struct pcx_parser_attribute_set {
        int base_value;
        /* Array of unsigned ints representing the symbols */
        struct pcx_buffer symbols;
};

struct pcx_parser {
        struct pcx_lexer *lexer;
        struct pcx_buffer symbols;
        struct pcx_list rooms;
        struct pcx_list texts;
        struct pcx_parser_attribute_set room_attributes;
        struct pcx_buffer tmp_buf;

        char *game_name;
        char *game_author;
        char *game_year;
        char *game_intro;
};

struct pcx_parser_reference {
        unsigned symbol;
        int line_num;
};

enum pcx_parser_target_type {
        PCX_PARSER_TARGET_TYPE_ROOM,
        PCX_PARSER_TARGET_TYPE_TEXT,
};

struct pcx_parser_target {
        enum pcx_parser_target_type type;
        unsigned id;
        int line_num;
        int num;
        struct pcx_list link;
};

struct pcx_parser_text_reference {
        bool resolved;
        int line_num;
        union {
                unsigned id;
                const char *text;
        };
};

struct pcx_parser_text {
        struct pcx_parser_target base;
        char *text;
};

struct pcx_parser_room {
        struct pcx_parser_target base;
        char *name;
        struct pcx_parser_text_reference description;
        struct pcx_parser_reference movements[PCX_AVT_N_DIRECTIONS];
        uint32_t attributes;
        long points;
};

enum pcx_parser_value_type {
        PCX_PARSER_VALUE_TYPE_STRING,
        PCX_PARSER_VALUE_TYPE_TEXT,
        PCX_PARSER_VALUE_TYPE_REFERENCE,
        PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
        PCX_PARSER_VALUE_TYPE_CUSTOM_ATTRIBUTE,
        PCX_PARSER_VALUE_TYPE_INT,
};

struct pcx_parser_property {
        size_t offset;
        enum pcx_parser_value_type value_type;
        enum pcx_lexer_token_type prop_token;
        const char *duplicate_msg;
        union {
                uint32_t attribute_value;
                struct {
                        long min_value, max_value;
                };
                size_t attribute_set_offset;
        };
};

typedef enum pcx_parser_return
(* item_parse_func)(struct pcx_parser *parser,
                    struct pcx_error **error);

#define check_item_token(parser, token_type, error)                     \
        do {                                                            \
                token = pcx_lexer_get_token((parser)->lexer, (error));  \
                                                                        \
                if (token == NULL)                                      \
                        return PCX_PARSER_RETURN_ERROR;                 \
                                                                        \
                if (token->type != (token_type)) {                      \
                        pcx_lexer_put_token(parser->lexer);             \
                        return PCX_PARSER_RETURN_NOT_MATCHED;           \
                }                                                       \
        } while (0)

#define require_token(parser, token_type, msg, error)                   \
        do {                                                            \
                token = pcx_lexer_get_token((parser)->lexer, (error));  \
                                                                        \
                if (token == NULL)                                      \
                        return PCX_PARSER_RETURN_ERROR;                 \
                                                                        \
                if (token->type != (token_type)) {                      \
                        pcx_set_error(error,                            \
                                      &pcx_parser_error,                \
                                      PCX_PARSER_ERROR_INVALID,         \
                                      msg                               \
                                      " at line %i",                    \
                                      pcx_lexer_get_line_num(parser->lexer)); \
                        return PCX_PARSER_RETURN_ERROR;                 \
                }                                                       \
        } while (0)

static bool
text_reference_specified(const struct pcx_parser_text_reference *reference)
{
        return reference->resolved || reference->id != 0;
}

static bool
assign_symbol(struct pcx_parser *parser,
              enum pcx_parser_target_type type,
              struct pcx_parser_target *target,
              const char *msg,
              struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        token = pcx_lexer_get_token(parser->lexer, error);

        if (token == NULL)
                return false;

        if (token->type != PCX_LEXER_TOKEN_TYPE_SYMBOL) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "%s at line %i",
                              msg,
                              pcx_lexer_get_line_num(parser->lexer));
                return false;
        }

        size_t new_size = ((token->symbol_value + 1) *
                           sizeof (struct pcx_parser_target *));

        if (new_size > parser->symbols.length) {
                pcx_buffer_ensure_size(&parser->symbols, new_size);
                memset(parser->symbols.data + parser->symbols.length,
                       0,
                       new_size - parser->symbols.length);
                pcx_buffer_set_length(&parser->symbols, new_size);
        }

        struct pcx_parser_target **symbols =
                (struct pcx_parser_target **) parser->symbols.data;

        if (symbols[token->symbol_value]) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Same name used for multiple objects at line %i",
                              pcx_lexer_get_line_num(parser->lexer));
                return false;
        }

        symbols[token->symbol_value] = target;
        target->id = token->symbol_value;
        target->type = type;

        return true;
}

static enum pcx_parser_return
parse_string_property(struct pcx_parser *parser,
                      const struct pcx_parser_property *prop,
                      void *object,
                      struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_token(parser, prop->prop_token, error);
        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_STRING,
                      "String expected",
                      error);

        char **field = (char **) (((uint8_t *) object) + prop->offset);

        if (*field) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "%s at line %i",
                              prop->duplicate_msg,
                              pcx_lexer_get_line_num(parser->lexer));
                return PCX_PARSER_RETURN_ERROR;
        }

        *field = pcx_strdup(token->string_value);

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_int_property(struct pcx_parser *parser,
                   const struct pcx_parser_property *prop,
                   void *object,
                   struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_token(parser, prop->prop_token, error);
        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_NUMBER,
                      "Number expected",
                      error);

        unsigned *field = (unsigned *) (((uint8_t *) object) + prop->offset);

        if (token->number_value < prop->min_value ||
            token->number_value > prop->max_value) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Number out of range at line %i",
                              pcx_lexer_get_line_num(parser->lexer));
                return PCX_PARSER_RETURN_ERROR;
        }

        *field = token->number_value;

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_attribute_property(struct pcx_parser *parser,
                         const struct pcx_parser_property *prop,
                         void *object,
                         struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_token(parser, prop->prop_token, error);

        uint32_t *field = (uint32_t *) (((uint8_t *) object) + prop->offset);

        *field |= prop->attribute_value;

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_custom_attribute_property(struct pcx_parser *parser,
                                const struct pcx_parser_property *prop,
                                void *object,
                                struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_token(parser, PCX_LEXER_TOKEN_TYPE_ATTRIBUTE, error);
        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_SYMBOL,
                      "Attribute name expected",
                      error);

        struct pcx_parser_attribute_set *set =
                (struct pcx_parser_attribute_set *)
                (((uint8_t *) parser) + prop->attribute_set_offset);
        int pos;
        size_t n_symbols = set->symbols.length / sizeof (unsigned);
        const unsigned *symbols = (const unsigned *) set->symbols.data;

        for (pos = 0; pos < n_symbols; pos++) {
                if (symbols[pos] == token->symbol_value)
                        goto found;
        }

        if (n_symbols + set->base_value + 1 > 32) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Too many unique attributes at line %i",
                              pcx_lexer_get_line_num(parser->lexer));
                return PCX_PARSER_RETURN_ERROR;
        }

        pcx_buffer_append(&set->symbols,
                          &token->symbol_value,
                          sizeof token->symbol_value);

found: (void) 0;

        uint32_t *field = (uint32_t *) (((uint8_t *) object) + prop->offset);

        *field |= 1 << (set->base_value + pos);

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_reference_property(struct pcx_parser *parser,
                         const struct pcx_parser_property *prop,
                         void *object,
                         struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_token(parser, prop->prop_token, error);
        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_SYMBOL,
                      "Item name expected",
                      error);

        struct pcx_parser_reference *ref =
                (struct pcx_parser_reference *)
                (((uint8_t *) object) + prop->offset);

        if (ref->symbol) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "%s at line %i",
                              prop->duplicate_msg,
                              pcx_lexer_get_line_num(parser->lexer));
                return PCX_PARSER_RETURN_ERROR;
        }

        ref->symbol = token->symbol_value;
        ref->line_num = pcx_lexer_get_line_num(parser->lexer);

        return PCX_PARSER_RETURN_OK;
}

static void
add_target(struct pcx_parser *parser,
           struct pcx_list *list,
           struct pcx_parser_target *target)
{
        if (pcx_list_empty(list)) {
                target->num = 0;
        } else {
                struct pcx_parser_target *prev =
                        pcx_container_of(list->prev,
                                         struct pcx_parser_target,
                                         link);
                target->num = prev->num + 1;
        }

        pcx_list_insert(list->prev, &target->link);
        target->line_num = pcx_lexer_get_line_num(parser->lexer);
}

static const char *
add_text(struct pcx_parser *parser,
         const char *value)
{
        struct pcx_parser_text *text = pcx_calloc(sizeof *text);

        text->text = pcx_strdup(value);
        add_target(parser, &parser->texts, &text->base);

        return text->text;
}

static enum pcx_parser_return
parse_text_property(struct pcx_parser *parser,
                    const struct pcx_parser_property *prop,
                    void *object,
                    struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_token(parser, prop->prop_token, error);

        token = pcx_lexer_get_token(parser->lexer, error);
        if (token == NULL)
                return PCX_PARSER_RETURN_ERROR;

        struct pcx_parser_text_reference *field =
                (struct pcx_parser_text_reference *)
                (((uint8_t *) object) + prop->offset);

        if (text_reference_specified(field)) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "%s at line %i",
                              prop->duplicate_msg,
                              pcx_lexer_get_line_num(parser->lexer));
                return PCX_PARSER_RETURN_ERROR;
        }

        field->line_num = pcx_lexer_get_line_num(parser->lexer);

        switch (token->type) {
        case PCX_LEXER_TOKEN_TYPE_STRING:
                field->resolved = true;
                field->text = add_text(parser, token->string_value);
                break;
        case PCX_LEXER_TOKEN_TYPE_SYMBOL:
                field->resolved = false;
                field->id = token->symbol_value;
                break;
        default:
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Expected text item at line %i",
                              pcx_lexer_get_line_num(parser->lexer));
                return PCX_PARSER_RETURN_ERROR;
        }

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_properties(struct pcx_parser *parser,
                 const struct pcx_parser_property *props,
                 size_t n_props,
                 void *object,
                 struct pcx_error **error)
{
        for (unsigned i = 0; i < n_props; i++) {
                enum pcx_parser_return ret = PCX_PARSER_RETURN_NOT_MATCHED;

                switch (props[i].value_type) {
                case PCX_PARSER_VALUE_TYPE_STRING:
                        ret = parse_string_property(parser,
                                                    props + i,
                                                    object,
                                                    error);
                        break;
                case PCX_PARSER_VALUE_TYPE_REFERENCE:
                        ret = parse_reference_property(parser,
                                                       props + i,
                                                       object,
                                                       error);
                        break;
                case PCX_PARSER_VALUE_TYPE_ATTRIBUTE:
                        ret = parse_attribute_property(parser,
                                                       props + i,
                                                       object,
                                                       error);
                        break;
                case PCX_PARSER_VALUE_TYPE_CUSTOM_ATTRIBUTE:
                        ret = parse_custom_attribute_property(parser,
                                                              props + i,
                                                              object,
                                                              error);
                        break;
                case PCX_PARSER_VALUE_TYPE_TEXT:
                        ret = parse_text_property(parser,
                                                  props + i,
                                                  object,
                                                  error);
                        break;
                case PCX_PARSER_VALUE_TYPE_INT:
                        ret = parse_int_property(parser,
                                                 props + i,
                                                 object,
                                                 error);
                        break;
                }

                if (ret != PCX_PARSER_RETURN_NOT_MATCHED)
                        return ret;
        }

        return PCX_PARSER_RETURN_NOT_MATCHED;
}

static const struct pcx_parser_property
room_props[] = {
        {
                offsetof(struct pcx_parser_room, name),
                PCX_PARSER_VALUE_TYPE_STRING,
                PCX_LEXER_TOKEN_TYPE_NAME,
                "Room already has a name",
        },
        {
                offsetof(struct pcx_parser_room, description),
                PCX_PARSER_VALUE_TYPE_TEXT,
                PCX_LEXER_TOKEN_TYPE_DESCRIPTION,
                "Room already has a description",
        },
        {
                offsetof(struct pcx_parser_room, movements[0]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_TOKEN_TYPE_NORTH,
                "Room already has a north direction",
        },
        {
                offsetof(struct pcx_parser_room, movements[1]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_TOKEN_TYPE_EAST,
                "Room already has an east direction",
        },
        {
                offsetof(struct pcx_parser_room, movements[2]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_TOKEN_TYPE_SOUTH,
                "Room already has a south direction",
        },
        {
                offsetof(struct pcx_parser_room, movements[3]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_TOKEN_TYPE_WEST,
                "Room already has a west direction",
        },
        {
                offsetof(struct pcx_parser_room, movements[4]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_TOKEN_TYPE_UP,
                "Room already has an up direction",
        },
        {
                offsetof(struct pcx_parser_room, movements[5]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_TOKEN_TYPE_DOWN,
                "Room already has a down direction",
        },
        {
                offsetof(struct pcx_parser_room, movements[6]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_TOKEN_TYPE_EXIT,
                "Room already has an exit direction",
        },
        {
                offsetof(struct pcx_parser_room, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_TOKEN_TYPE_LIT,
                .attribute_value = PCX_AVT_ROOM_ATTRIBUTE_LIT,
        },
        {
                offsetof(struct pcx_parser_room, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_TOKEN_TYPE_UNLIGHTABLE,
                .attribute_value =
                PCX_AVT_ROOM_ATTRIBUTE_UNLIGHTABLE,
        },
        {
                offsetof(struct pcx_parser_room, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_TOKEN_TYPE_GAME_OVER,
                .attribute_value =
                PCX_AVT_ROOM_ATTRIBUTE_GAME_OVER,
        },
        {
                offsetof(struct pcx_parser_room, points),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_TOKEN_TYPE_POINTS,
                .min_value = 0, .max_value = UINT8_MAX,
        },
        {
                offsetof(struct pcx_parser_room, attributes),
                PCX_PARSER_VALUE_TYPE_CUSTOM_ATTRIBUTE,
                .attribute_set_offset =
                offsetof(struct pcx_parser, room_attributes),
        },
};

static enum pcx_parser_return
parse_room(struct pcx_parser *parser,
           struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_token(parser, PCX_LEXER_TOKEN_TYPE_ROOM, error);

        struct pcx_parser_room *room = pcx_calloc(sizeof *room);
        add_target(parser, &parser->rooms, &room->base);

        if (!assign_symbol(parser,
                           PCX_PARSER_TARGET_TYPE_ROOM,
                           &room->base,
                           "Expected room name",
                           error))
                return PCX_PARSER_RETURN_ERROR;

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_OPEN_BRACKET,
                      "Expected ‘{’",
                      error);

        while (true) {
                const struct pcx_lexer_token *token =
                        pcx_lexer_get_token(parser->lexer, error);

                if (token == NULL)
                        return PCX_PARSER_RETURN_ERROR;

                if (token->type == PCX_LEXER_TOKEN_TYPE_CLOSE_BRACKET)
                        break;

                pcx_lexer_put_token(parser->lexer);

                switch (parse_properties(parser,
                                         room_props,
                                         PCX_N_ELEMENTS(room_props),
                                         room,
                                         error)) {
                case PCX_PARSER_RETURN_OK:
                        continue;
                case PCX_PARSER_RETURN_NOT_MATCHED:
                        break;
                case PCX_PARSER_RETURN_ERROR:
                        return PCX_PARSER_RETURN_ERROR;
                }

                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Expected room item or ‘}’ at line %i",
                              pcx_lexer_get_line_num(parser->lexer));
                return PCX_PARSER_RETURN_ERROR;
        }

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_text(struct pcx_parser *parser,
           struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_token(parser, PCX_LEXER_TOKEN_TYPE_TEXT, error);

        struct pcx_parser_text *text = pcx_calloc(sizeof *text);
        add_target(parser, &parser->texts, &text->base);

        if (!assign_symbol(parser,
                           PCX_PARSER_TARGET_TYPE_TEXT,
                           &text->base,
                           "Expected text name",
                           error))
                return PCX_PARSER_RETURN_ERROR;

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_STRING,
                      "Expected string",
                      error);

        text->text = pcx_strdup(token->string_value);

        return PCX_PARSER_RETURN_OK;
}

static const struct pcx_parser_property
file_props[] = {
        {
                offsetof(struct pcx_parser, game_name),
                PCX_PARSER_VALUE_TYPE_STRING,
                PCX_LEXER_TOKEN_TYPE_NAME,
                "Room already has a name",
        },
        {
                offsetof(struct pcx_parser, game_author),
                PCX_PARSER_VALUE_TYPE_STRING,
                PCX_LEXER_TOKEN_TYPE_AUTHOR,
                "Room already has an author",
        },
        {
                offsetof(struct pcx_parser, game_year),
                PCX_PARSER_VALUE_TYPE_STRING,
                PCX_LEXER_TOKEN_TYPE_YEAR,
                "Room already has a year",
        },
        {
                offsetof(struct pcx_parser, game_intro),
                PCX_PARSER_VALUE_TYPE_STRING,
                PCX_LEXER_TOKEN_TYPE_INTRODUCTION,
                "Room already has an introduction",
        },
};

static bool
parse_file(struct pcx_parser *parser,
           struct pcx_error **error)
{
        while (true) {
                const struct pcx_lexer_token *token =
                        pcx_lexer_get_token(parser->lexer, error);

                if (token == NULL)
                        return false;

                if (token->type == PCX_LEXER_TOKEN_TYPE_EOF)
                        break;

                pcx_lexer_put_token(parser->lexer);

                static const item_parse_func funcs[] = {
                        parse_room,
                        parse_text,
                };

                for (unsigned i = 0; i < PCX_N_ELEMENTS(funcs); i++) {
                        switch (funcs[i](parser, error)) {
                        case PCX_PARSER_RETURN_OK:
                                goto found;
                        case PCX_PARSER_RETURN_NOT_MATCHED:
                                break;
                        case PCX_PARSER_RETURN_ERROR:
                                return false;
                        }
                }

                switch (parse_properties(parser,
                                         file_props,
                                         PCX_N_ELEMENTS(file_props),
                                         parser,
                                         error)) {
                case PCX_PARSER_RETURN_OK:
                        continue;
                case PCX_PARSER_RETURN_NOT_MATCHED:
                        break;
                case PCX_PARSER_RETURN_ERROR:
                        return false;
                }

                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Expected toplevel item at line %i",
                              pcx_lexer_get_line_num(parser->lexer));
                return false;

        found:
                continue;
        }

        return true;
}

static struct pcx_parser_target *
get_symbol_reference(struct pcx_parser *parser,
                     unsigned symbol)
{
        size_t n_symbols = (parser->symbols.length /
                            sizeof (struct pcx_parser_target *));
        struct pcx_parser_target **symbols =
                (struct pcx_parser_target **) parser->symbols.data;

        if (symbol >= n_symbols)
                return NULL;

        return symbols[symbol];
}

static bool
buffer_contains_pointer(struct pcx_buffer *buf,
                        void *ptr)
{
        size_t n_ptrs = buf->length / sizeof ptr;
        void **ptrs = (void **) buf->data;

        for (size_t i = 0; i < n_ptrs; i++) {
                if (ptrs[i] == ptr)
                        return true;
        }

        return false;
}

static bool
resolve_text_reference(struct pcx_parser *parser,
                       struct pcx_parser_text_reference *ref,
                       struct pcx_error **error)
{
        pcx_buffer_set_length(&parser->tmp_buf, 0);

        const char *text = NULL;

        while (true) {
                if (ref->resolved) {
                        text = ref->text;
                        goto found;
                }

                if (buffer_contains_pointer(&parser->tmp_buf, ref)) {
                        pcx_set_error(error,
                                      &pcx_parser_error,
                                      PCX_PARSER_ERROR_INVALID,
                                      "Cyclic reference detected at line %i",
                                      ref->line_num);
                        return false;
                }

                pcx_buffer_append(&parser->tmp_buf, &ref, sizeof ref);

                struct pcx_parser_target *target =
                        get_symbol_reference(parser, ref->id);

                if (target == NULL)
                        goto error;

                switch (target->type) {
                case PCX_PARSER_TARGET_TYPE_ROOM:
                        ref = &pcx_container_of(target,
                                                struct pcx_parser_room,
                                                base)->description;
                        continue;
                case PCX_PARSER_TARGET_TYPE_TEXT:
                        text = pcx_container_of(target,
                                                struct pcx_parser_text,
                                                base)->text;
                        goto found;
                }

                goto error;
        }

found: (void) 0;

        size_t n_refs = (parser->tmp_buf.length /
                         sizeof (struct pcx_parser_text_reference *));
        struct pcx_parser_text_reference **refs =
                (struct pcx_parser_text_reference **) parser->tmp_buf.data;

        for (size_t i = 0; i < n_refs; i++) {
                refs[i]->resolved = true;
                refs[i]->text = text;
        }

        return true;

error:
        pcx_set_error(error,
                      &pcx_parser_error,
                      PCX_PARSER_ERROR_INVALID,
                      "Invalid text reference at line %i",
                      ref->line_num);
        return false;
}

static char *
convert_symbol_to_name(struct pcx_parser *parser,
                       unsigned symbol)
{
        const char *symbol_name =
                pcx_lexer_get_symbol_name(parser->lexer, symbol);
        char *name = pcx_strdup(symbol_name);

        for (char *p = name; *p; p++) {
                if (*p == '_')
                        *p = ' ';
        }

        return name;
}

static bool
compile_room_movements(struct pcx_parser *parser,
                        struct pcx_parser_room *room,
                        struct pcx_avt_room *avt_room,
                        struct pcx_error **error)
{
        for (int i = 0; i < PCX_AVT_N_DIRECTIONS; i++) {
                if (room->movements[i].symbol == 0) {
                        avt_room->movements[i] = PCX_AVT_DIRECTION_BLOCKED;
                        continue;
                }

                struct pcx_parser_target *target =
                        get_symbol_reference(parser, room->movements[i].symbol);

                if (target == NULL ||
                    target->type != PCX_PARSER_TARGET_TYPE_ROOM) {
                        pcx_set_error(error,
                                      &pcx_parser_error,
                                      PCX_PARSER_ERROR_INVALID,
                                      "Invalid room reference on line %i",
                                      room->movements[i].line_num);
                        return false;
                }

                avt_room->movements[i] = target->num;
        }

        return true;
}

static bool
compile_room(struct pcx_parser *parser,
             struct pcx_parser_room *room,
             struct pcx_avt_room *avt_room,
             struct pcx_error **error)
{
        if (!text_reference_specified(&room->description)) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Room is missing description at line %i",
                              room->base.line_num);
                return false;
        }

        if (!resolve_text_reference(parser, &room->description, error))
                return false;

        avt_room->description = room->description.text;

        if (room->name) {
                avt_room->name = room->name;
                room->name = NULL;
        } else {
                avt_room->name = convert_symbol_to_name(parser, room->base.id);
        }

        avt_room->points = room->points;
        avt_room->attributes = room->attributes;

        if (!compile_room_movements(parser, room, avt_room, error))
                return false;

        return true;
}

static bool
compile_info(struct pcx_parser *parser,
             struct pcx_avt *avt,
             struct pcx_error **error)
{
        if (parser->game_name == NULL) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "The game is missing the “nomo”");
                return false;
        }

        avt->name = parser->game_name;
        parser->game_name = NULL;

        if (parser->game_author == NULL) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "The game is missing the “aŭtoro”");
                return false;
        }

        avt->author = parser->game_author;
        parser->game_author = NULL;

        if (parser->game_year == NULL) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "The game is missing the “jaro”");
                return false;
        }

        avt->year = parser->game_year;
        parser->game_year = NULL;

        avt->introduction = parser->game_intro;
        parser->game_intro = NULL;

        return true;
}

static bool
compile_file(struct pcx_parser *parser,
             struct pcx_avt *avt,
             struct pcx_error **error)
{
        struct pcx_parser_text *text;
        int string_num = 0;

        if (!compile_info(parser, avt, error))
                return false;

        if (pcx_list_empty(&parser->rooms)) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "The game needs at least one room");
                return false;
        }

        avt->n_rooms = pcx_list_length(&parser->rooms);
        avt->rooms = pcx_calloc(sizeof (struct pcx_avt_room) * avt->n_rooms);

        struct pcx_parser_room *room;
        int room_num = 0;

        pcx_list_for_each(room, &parser->rooms, base.link) {
                if (!compile_room(parser, room, avt->rooms + room_num, error))
                        return false;

                room_num++;
        }

        avt->n_strings = pcx_list_length(&parser->texts);
        avt->strings = pcx_alloc(sizeof (char *) * avt->n_strings);

        pcx_list_for_each(text, &parser->texts, base.link) {
                avt->strings[string_num] = text->text;
                text->text = NULL;
                string_num++;
        }

        return true;
}

static void
destroy_rooms(struct pcx_parser *parser)
{
        struct pcx_parser_room *room, *tmp;

        pcx_list_for_each_safe(room, tmp, &parser->rooms, base.link) {
                pcx_free(room->name);
                pcx_free(room);
        }
}

static void
destroy_texts(struct pcx_parser *parser)
{
        struct pcx_parser_text *text, *tmp;

        pcx_list_for_each_safe(text, tmp, &parser->texts, base.link) {
                pcx_free(text->text);
                pcx_free(text);
        }
}

static void
destroy_parser(struct pcx_parser *parser)
{
        destroy_rooms(parser);
        destroy_texts(parser);

        pcx_free(parser->game_name);
        pcx_free(parser->game_author);
        pcx_free(parser->game_year);
        pcx_free(parser->game_intro);

        pcx_buffer_destroy(&parser->room_attributes.symbols);

        pcx_buffer_destroy(&parser->symbols);

        pcx_buffer_destroy(&parser->tmp_buf);
}

struct pcx_avt *
pcx_parser_parse(struct pcx_source *source,
                 struct pcx_error **error)
{
        struct pcx_parser parser = {
                .lexer = pcx_lexer_new(source),
        };

        pcx_list_init(&parser.rooms);
        pcx_list_init(&parser.texts);
        pcx_buffer_init(&parser.symbols);
        pcx_buffer_init(&parser.room_attributes.symbols);
        parser.room_attributes.base_value = 4;
        pcx_buffer_init(&parser.tmp_buf);

        bool ret = parse_file(&parser, error);

        struct pcx_avt *avt = NULL;

        if (ret) {
                avt = pcx_calloc(sizeof *avt);
                if (!compile_file(&parser, avt, error)) {
                        pcx_avt_free(avt);
                        avt = NULL;
                }
        }

        pcx_lexer_free(parser.lexer);
        destroy_parser(&parser);

        return avt;
}
