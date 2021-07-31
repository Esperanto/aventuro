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
#include <assert.h>

#include "pcx-lexer.h"
#include "pcx-list.h"
#include "pcx-buffer.h"
#include "pcx-avt-hat.h"

struct pcx_error_domain
pcx_parser_error;

enum pcx_parser_return {
        PCX_PARSER_RETURN_OK,
        PCX_PARSER_RETURN_NOT_MATCHED,
        PCX_PARSER_RETURN_ERROR,
};

struct pcx_parser_attribute_set {
        /* Array of unsigned ints representing the symbols */
        struct pcx_buffer symbols;
};

struct pcx_parser {
        struct pcx_lexer *lexer;
        struct pcx_buffer symbols;
        struct pcx_list rooms;
        struct pcx_list objects;
        struct pcx_list texts;
        struct pcx_list rules;
        struct pcx_list verbs;
        struct pcx_parser_attribute_set room_attributes;
        struct pcx_parser_attribute_set object_attributes;
        struct pcx_parser_attribute_set player_attributes;
        struct pcx_buffer tmp_buf;
        struct pcx_list aliases;

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
        PCX_PARSER_TARGET_TYPE_OBJECT,
};

static const unsigned
object_base_attributes[] = {
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
};

static const unsigned
room_base_attributes[] = {
        PCX_LEXER_KEYWORD_LIT,
        PCX_LEXER_KEYWORD_UNLIGHTABLE,
        PCX_LEXER_KEYWORD_GAME_OVER,
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

enum pcx_parser_rule_parameter_type {
        PCX_PARSER_RULE_PARAMETER_TYPE_NONE,
        PCX_PARSER_RULE_PARAMETER_TYPE_INT,
        PCX_PARSER_RULE_PARAMETER_TYPE_OBJECT,
        PCX_PARSER_RULE_PARAMETER_TYPE_ROOM,
};

struct pcx_parser_rule_parameter {
        enum pcx_parser_rule_parameter_type type;
        union {
                struct pcx_parser_reference reference;
                long data;
        };
};

struct pcx_parser_verb {
        struct pcx_list link;
        char *name;
        /* Array of uint16_t to index into rules */
        struct pcx_buffer rules;
};

struct pcx_parser_rule_condition {
        enum pcx_avt_rule_subject subject;
        enum pcx_avt_condition condition;
        struct pcx_parser_rule_parameter param;
};

struct pcx_parser_rule_action {
        enum pcx_avt_rule_subject subject;
        enum pcx_avt_action action;
        struct pcx_parser_rule_parameter param;
};

struct pcx_parser_rule {
        struct pcx_parser_target base;
        char *verb;
        struct pcx_parser_text_reference message;
        long points;
        struct pcx_buffer conditions;
        struct pcx_buffer actions;
};

struct pcx_parser_text {
        struct pcx_parser_target base;
        char *text;
};

struct pcx_parser_direction {
        struct pcx_list link;
        struct pcx_parser_reference room;
        char *name;
        struct pcx_parser_text_reference description;
};

struct pcx_parser_room {
        struct pcx_parser_target base;
        char *name;
        struct pcx_parser_text_reference description;
        struct pcx_parser_reference movements[PCX_AVT_N_DIRECTIONS];
        struct pcx_list directions;
        uint32_t attributes;
        long points;
};

struct pcx_parser_pronoun {
        bool specified;
        enum pcx_avt_pronoun value;
};

struct pcx_parser_alias {
        struct pcx_list link;
        char *name;
        bool plural;
        enum pcx_parser_target_type target_type;
        int target_num;
};

struct pcx_parser_object {
        struct pcx_parser_target base;
        char *name;
        struct pcx_parser_text_reference description;
        struct pcx_parser_text_reference read_text;
        struct pcx_parser_reference location;
        bool carrying;
        struct pcx_parser_reference into;
        uint32_t attributes;
        long points;
        long weight;
        long size;
        long container_size;
        long shot_damage;
        long shots;
        long hit_damage;
        long stab_damage;
        long food_points;
        long trink_points;
        long burn_time;
        long end;
        struct pcx_parser_pronoun pronoun;
};

enum pcx_parser_value_type {
        PCX_PARSER_VALUE_TYPE_STRING,
        PCX_PARSER_VALUE_TYPE_TEXT,
        PCX_PARSER_VALUE_TYPE_REFERENCE,
        PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
        PCX_PARSER_VALUE_TYPE_CUSTOM_ATTRIBUTE,
        PCX_PARSER_VALUE_TYPE_INT,
        PCX_PARSER_VALUE_TYPE_BOOL,
        PCX_PARSER_VALUE_TYPE_PRONOUN,
};

struct pcx_parser_property {
        size_t offset;
        enum pcx_parser_value_type value_type;
        enum pcx_lexer_keyword prop_keyword;
        const char *duplicate_msg;
        union {
                struct {
                        long min_value, max_value;
                };
                size_t attribute_set_offset;
        };
};

typedef enum pcx_parser_return
(* item_parse_func)(struct pcx_parser *parser,
                    struct pcx_parser_target *parent_target,
                    struct pcx_error **error);

#define check_item_keyword(parser, keyword, error)                      \
        do {                                                            \
                token = pcx_lexer_get_token((parser)->lexer, (error));  \
                                                                        \
                if (token == NULL)                                      \
                        return PCX_PARSER_RETURN_ERROR;                 \
                                                                        \
                if (token->type != PCX_LEXER_TOKEN_TYPE_SYMBOL ||       \
                    token->symbol_value != (keyword)) {                 \
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

        check_item_keyword(parser, prop->prop_keyword, error);
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

        check_item_keyword(parser, prop->prop_keyword, error);
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
parse_bool_property(struct pcx_parser *parser,
                    const struct pcx_parser_property *prop,
                    void *object,
                    struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_keyword(parser, prop->prop_keyword, error);

        bool *field = (bool *) (((uint8_t *) object) + prop->offset);

        *field = true;

        return PCX_PARSER_RETURN_OK;
}

static bool
get_attribute_num(struct pcx_parser *parser,
                  struct pcx_parser_attribute_set *set,
                  unsigned symbol,
                  int *pos_out,
                  struct pcx_error **error)
{
        size_t n_symbols = set->symbols.length / sizeof (unsigned);
        const unsigned *symbols = (const unsigned *) set->symbols.data;
        int pos;

        for (pos = 0; pos < n_symbols; pos++) {
                if (symbols[pos] == symbol)
                        goto found;
        }

        if (n_symbols + 2 > 32) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Too many unique attributes at line %i",
                              pcx_lexer_get_line_num(parser->lexer));
                return false;
        }

        pcx_buffer_append(&set->symbols, &symbol, sizeof symbol);

found:
        *pos_out = pos + 1;

        return true;
}

static enum pcx_parser_return
parse_attribute_property(struct pcx_parser *parser,
                         const struct pcx_parser_property *prop,
                         void *object,
                         struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        if (prop->value_type == PCX_PARSER_VALUE_TYPE_CUSTOM_ATTRIBUTE) {
                check_item_keyword(parser, PCX_LEXER_KEYWORD_ATTRIBUTE, error);
                require_token(parser,
                              PCX_LEXER_TOKEN_TYPE_SYMBOL,
                              "Attribute name expected",
                              error);
        } else {
                check_item_keyword(parser, prop->prop_keyword, error);
                /* The keyword itself becomes the value to set */
        }

        struct pcx_parser_attribute_set *set =
                (struct pcx_parser_attribute_set *)
                (((uint8_t *) parser) + prop->attribute_set_offset);

        size_t old_length = set->symbols.length;
        int num;

        if (!get_attribute_num(parser, set, token->symbol_value, &num, error))
                return PCX_PARSER_RETURN_ERROR;

        assert(old_length == set->symbols.length ||
               prop->value_type == PCX_PARSER_VALUE_TYPE_CUSTOM_ATTRIBUTE);

        uint32_t *field = (uint32_t *) (((uint8_t *) object) + prop->offset);

        *field |= 1 << (uint32_t) num;

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_reference_property(struct pcx_parser *parser,
                         const struct pcx_parser_property *prop,
                         void *object,
                         struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_keyword(parser, prop->prop_keyword, error);
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

static bool
store_text_reference(struct pcx_parser *parser,
                     struct pcx_parser_text_reference *reference,
                     bool optional,
                     struct pcx_error **error)
{
        const struct pcx_lexer_token *token =
                pcx_lexer_get_token(parser->lexer, error);
        if (token == NULL)
                return false;

        reference->line_num = pcx_lexer_get_line_num(parser->lexer);

        switch (token->type) {
        case PCX_LEXER_TOKEN_TYPE_STRING:
                reference->resolved = true;
                reference->text = add_text(parser, token->string_value);
                break;
        case PCX_LEXER_TOKEN_TYPE_SYMBOL:
                if (token->symbol_value == PCX_LEXER_KEYWORD_NOTHING) {
                        if (optional) {
                                reference->resolved = false;
                                reference->id = 0;
                                break;
                        }
                } else {
                        reference->resolved = false;
                        reference->id = token->symbol_value;
                        break;
                }
                /* flow through */
        default:
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Expected text item%s at line %i",
                              optional ? " or “nenio”" : "",
                              pcx_lexer_get_line_num(parser->lexer));
                return false;
        }

        return true;
}

static enum pcx_parser_return
parse_text_property(struct pcx_parser *parser,
                    const struct pcx_parser_property *prop,
                    void *object,
                    struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_keyword(parser, prop->prop_keyword, error);

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

        if (!store_text_reference(parser,
                                  field,
                                  false, /* optional */
                                  error))
                return PCX_PARSER_RETURN_ERROR;

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_pronoun_property(struct pcx_parser *parser,
                       const struct pcx_parser_property *prop,
                       void *object,
                       struct pcx_error **error)
{
        const struct pcx_lexer_token *token =
                pcx_lexer_get_token(parser->lexer, error);

        if (token == NULL)
                return PCX_PARSER_RETURN_ERROR;

        if (token->type != PCX_LEXER_TOKEN_TYPE_SYMBOL) {
                pcx_lexer_put_token(parser->lexer);
                return PCX_PARSER_RETURN_NOT_MATCHED;
        }

        struct pcx_parser_pronoun *field =
                (struct pcx_parser_pronoun *)
                (((uint8_t *) object) + prop->offset);

        switch (token->symbol_value) {
        case PCX_LEXER_KEYWORD_MAN:
                field->value = PCX_AVT_PRONOUN_MAN;
                break;
        case PCX_LEXER_KEYWORD_WOMAN:
                field->value = PCX_AVT_PRONOUN_WOMAN;
                break;
        case PCX_LEXER_KEYWORD_ANIMAL:
                field->value = PCX_AVT_PRONOUN_ANIMAL;
                break;
        case PCX_LEXER_KEYWORD_PLURAL:
                field->value = PCX_AVT_PRONOUN_PLURAL;
                break;
        default:
                pcx_lexer_put_token(parser->lexer);
                return PCX_PARSER_RETURN_NOT_MATCHED;
        }

        if (field->specified) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Pronoun already specified at line %i",
                              pcx_lexer_get_line_num(parser->lexer));
                return PCX_PARSER_RETURN_ERROR;
        }

        field->specified = true;

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
                case PCX_PARSER_VALUE_TYPE_CUSTOM_ATTRIBUTE:
                        ret = parse_attribute_property(parser,
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
                case PCX_PARSER_VALUE_TYPE_BOOL:
                        ret = parse_bool_property(parser,
                                                  props + i,
                                                  object,
                                                  error);
                        break;
                case PCX_PARSER_VALUE_TYPE_PRONOUN:
                        ret = parse_pronoun_property(parser,
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

static enum pcx_parser_return
parse_items(struct pcx_parser *parser,
            const item_parse_func *funcs,
            size_t n_funcs,
            struct pcx_parser_target *parent_target,
            struct pcx_error **error)
{
        for (unsigned i = 0; i < n_funcs; i++) {
                enum pcx_parser_return ret =
                        funcs[i](parser, parent_target, error);

                if (ret != PCX_PARSER_RETURN_NOT_MATCHED)
                        return ret;
        }

        return PCX_PARSER_RETURN_NOT_MATCHED;
}

static const struct pcx_parser_property
rule_props[] = {
        {
                offsetof(struct pcx_parser_rule, message),
                PCX_PARSER_VALUE_TYPE_TEXT,
                PCX_LEXER_KEYWORD_MESSAGE,
                "Rule already has a message",
        },
        {
                offsetof(struct pcx_parser_rule, points),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_POINTS,
                .min_value = 0, .max_value = UINT8_MAX,
        },
};

static struct pcx_parser_rule_condition *
add_rule_condition(struct pcx_parser_rule *rule,
                   enum pcx_avt_rule_subject subject)
{
        pcx_buffer_set_length(&rule->conditions,
                              rule->conditions.length +
                              sizeof (struct pcx_parser_rule_condition));
        struct pcx_parser_rule_condition *condition =
                ((struct pcx_parser_rule_condition *)
                 (rule->conditions.data + rule->conditions.length)) - 1;

        condition->subject = subject;

        return condition;
}

static enum pcx_parser_return
parse_verb(struct pcx_parser *parser,
           struct pcx_parser_rule *rule,
           struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_keyword(parser, PCX_LEXER_KEYWORD_VERB, error);

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_STRING,
                      "String expected",
                      error);

        char *name = pcx_strdup(token->string_value);
        int len = strlen(name);

        if (len < 2 || name[len - 1] != 'i') {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Verb must end in ‘i’ at line %i",
                              pcx_lexer_get_line_num(parser->lexer));
                pcx_free(name);
                return PCX_PARSER_RETURN_ERROR;
        }

        name[len - 1] = '\0';

        struct pcx_parser_verb *verb;

        pcx_list_for_each(verb, &parser->verbs, link) {
                if (!strcmp(verb->name, name)) {
                        pcx_free(name);
                        goto found;
                }
        }

        verb = pcx_alloc(sizeof *verb);
        verb->name = name;
        pcx_buffer_init(&verb->rules);
        pcx_list_insert(parser->verbs.prev, &verb->link);

found: (void) 0;

        uint16_t rule_num = rule->base.num;
        pcx_buffer_append(&verb->rules, &rule_num, sizeof rule_num);

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_rule_int_param(struct pcx_parser *parser,
                     struct pcx_parser_rule_parameter *param,
                     struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_NUMBER,
                      "Number expected",
                      error);

        if (token->number_value < 0 ||
            token->number_value > 255) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Number out of range at line %i",
                              pcx_lexer_get_line_num(parser->lexer));
                return PCX_PARSER_RETURN_ERROR;
        }

        param->type = PCX_PARSER_RULE_PARAMETER_TYPE_INT;
        param->data = token->number_value;

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_rule_object_param(struct pcx_parser *parser,
                        struct pcx_parser_rule_parameter *param,
                        struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_SYMBOL,
                      "Object name expected",
                      error);

        param->type = PCX_PARSER_RULE_PARAMETER_TYPE_OBJECT;
        param->reference.symbol = token->symbol_value;
        param->reference.line_num = pcx_lexer_get_line_num(parser->lexer);

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_rule_attribute_param(struct pcx_parser *parser,
                           struct pcx_parser_rule_parameter *param,
                           bool *is_set,
                           struct pcx_parser_attribute_set *attribute_set,
                           struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_SYMBOL,
                      "Expected attribute name or “malvera”",
                      error);

        if (token->symbol_value == PCX_LEXER_KEYWORD_UNSET) {
                *is_set = false;

                require_token(parser,
                              PCX_LEXER_TOKEN_TYPE_SYMBOL,
                              "Expected attribute name",
                              error);
        } else {
                *is_set = true;
        }

        int num;

        if (!get_attribute_num(parser,
                               attribute_set,
                               token->symbol_value,
                               &num,
                               error))
                return PCX_PARSER_RETURN_ERROR;

        param->type = PCX_PARSER_RULE_PARAMETER_TYPE_INT;
        param->data = num;

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_condition_attribute_param(struct pcx_parser *parser,
                                struct pcx_parser_rule_condition *cond,
                                enum pcx_avt_condition set_condition,
                                enum pcx_avt_condition unset_condition,
                                struct pcx_parser_attribute_set *attribute_set,
                                struct pcx_error **error)
{
        bool is_set = false;

        enum pcx_parser_return ret =
                parse_rule_attribute_param(parser,
                                           &cond->param,
                                           &is_set,
                                           attribute_set,
                                           error);

        cond->condition = is_set ? set_condition : unset_condition;

        return ret;
}

static enum pcx_parser_return
parse_object_condition(struct pcx_parser *parser,
                       struct pcx_parser_rule_condition *cond,
                       struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_SYMBOL,
                      "Expected rule condition",
                      error);

        switch (token->symbol_value) {
        case PCX_LEXER_KEYWORD_SOMETHING:
                cond->condition = PCX_AVT_CONDITION_SOMETHING;
                cond->param.type = PCX_PARSER_RULE_PARAMETER_TYPE_NONE;
                return PCX_PARSER_RETURN_OK;
        case PCX_LEXER_KEYWORD_NOTHING:
                cond->condition = PCX_AVT_CONDITION_NOTHING;
                cond->param.type = PCX_PARSER_RULE_PARAMETER_TYPE_NONE;
                return PCX_PARSER_RETURN_OK;
        case PCX_LEXER_KEYWORD_SHOTS:
                cond->condition = PCX_AVT_CONDITION_SHOTS;
                return parse_rule_int_param(parser, &cond->param, error);
        case PCX_LEXER_KEYWORD_WEIGHT:
                cond->condition = PCX_AVT_CONDITION_WEIGHT;
                return parse_rule_int_param(parser, &cond->param, error);
        case PCX_LEXER_KEYWORD_SIZE:
                cond->condition = PCX_AVT_CONDITION_SIZE;
                return parse_rule_int_param(parser, &cond->param, error);
        case PCX_LEXER_KEYWORD_CONTAINER_SIZE:
                cond->condition = PCX_AVT_CONDITION_CONTAINER_SIZE;
                return parse_rule_int_param(parser, &cond->param, error);
        case PCX_LEXER_KEYWORD_BURN_TIME:
                cond->condition = PCX_AVT_CONDITION_BURN_TIME;
                return parse_rule_int_param(parser, &cond->param, error);
        case PCX_LEXER_KEYWORD_ADJECTIVE:
                cond->condition = PCX_AVT_CONDITION_OBJECT_SAME_ADJECTIVE;
                return parse_rule_object_param(parser, &cond->param, error);
        case PCX_LEXER_KEYWORD_NAME:
                cond->condition = PCX_AVT_CONDITION_OBJECT_SAME_NAME;
                return parse_rule_object_param(parser, &cond->param, error);
        case PCX_LEXER_KEYWORD_COPY:
                cond->condition = PCX_AVT_CONDITION_OBJECT_SAME_NOUN;
                return parse_rule_object_param(parser, &cond->param, error);
        case PCX_LEXER_KEYWORD_ATTRIBUTE:
                return parse_condition_attribute_param
                        (parser,
                         cond,
                         PCX_AVT_CONDITION_OBJECT_ATTRIBUTE,
                         PCX_AVT_CONDITION_NOT_OBJECT_ATTRIBUTE,
                         &parser->object_attributes,
                         error);
        }

        /* Anything else should directly be an object name */
        pcx_lexer_put_token(parser->lexer);
        cond->condition = PCX_AVT_CONDITION_OBJECT_IS;
        return parse_rule_object_param(parser, &cond->param, error);
}

static enum pcx_parser_return
parse_rule_room_param(struct pcx_parser *parser,
                      struct pcx_parser_rule_parameter *param,
                      struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_SYMBOL,
                      "Room name expected",
                      error);

        param->type = PCX_PARSER_RULE_PARAMETER_TYPE_ROOM;
        param->reference.symbol = token->symbol_value;
        param->reference.line_num = pcx_lexer_get_line_num(parser->lexer);

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_room_condition(struct pcx_parser *parser,
                     struct pcx_parser_rule_condition *cond,
                     struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_SYMBOL,
                      "Expected rule condition",
                      error);

        if (token->symbol_value == PCX_LEXER_KEYWORD_ATTRIBUTE) {
                return parse_condition_attribute_param
                        (parser,
                         cond,
                         PCX_AVT_CONDITION_ROOM_ATTRIBUTE,
                         PCX_AVT_CONDITION_NOT_ROOM_ATTRIBUTE,
                         &parser->room_attributes,
                         error);
        }

        /* Anything else should directly be a room name */
        pcx_lexer_put_token(parser->lexer);
        cond->condition = PCX_AVT_CONDITION_IN_ROOM;
        return parse_rule_room_param(parser, &cond->param, error);
}

static enum pcx_parser_return
parse_condition(struct pcx_parser *parser,
                struct pcx_parser_rule *rule,
                struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        token = pcx_lexer_get_token(parser->lexer, error);

        if (token == NULL)
                return PCX_PARSER_RETURN_ERROR;

        if (token->type != PCX_LEXER_TOKEN_TYPE_SYMBOL) {
                pcx_lexer_put_token(parser->lexer);
                return PCX_PARSER_RETURN_NOT_MATCHED;
        }

        struct pcx_parser_rule_condition *cond;

        switch (token->symbol_value) {
        case PCX_LEXER_KEYWORD_OBJECT:
                cond = add_rule_condition(rule, PCX_AVT_RULE_SUBJECT_OBJECT);
                return parse_object_condition(parser, cond, error);
        case PCX_LEXER_KEYWORD_TOOL:
                cond = add_rule_condition(rule, PCX_AVT_RULE_SUBJECT_TOOL);
                return parse_object_condition(parser, cond, error);
        case PCX_LEXER_KEYWORD_ROOM:
                cond = add_rule_condition(rule, PCX_AVT_RULE_SUBJECT_ROOM);
                return parse_room_condition(parser, cond, error);
        case PCX_LEXER_KEYWORD_ATTRIBUTE:
                cond = add_rule_condition(rule, PCX_AVT_RULE_SUBJECT_ROOM);
                return parse_condition_attribute_param
                        (parser,
                         cond,
                         PCX_AVT_CONDITION_PLAYER_ATTRIBUTE,
                         PCX_AVT_CONDITION_NOT_PLAYER_ATTRIBUTE,
                         &parser->player_attributes,
                         error);
        case PCX_LEXER_KEYWORD_PRESENT:
                cond = add_rule_condition(rule, PCX_AVT_RULE_SUBJECT_ROOM);
                cond->condition = PCX_AVT_CONDITION_ANOTHER_OBJECT_PRESENT;
                return parse_rule_object_param(parser, &cond->param, error);
        case PCX_LEXER_KEYWORD_CHANCE:
                cond = add_rule_condition(rule, PCX_AVT_RULE_SUBJECT_ROOM);
                cond->condition = PCX_AVT_CONDITION_CHANCE;
                return parse_rule_int_param(parser, &cond->param, error);
        default:
                pcx_lexer_put_token(parser->lexer);
                return PCX_PARSER_RETURN_NOT_MATCHED;
        }
}

static struct pcx_parser_rule_action *
add_rule_action(struct pcx_parser_rule *rule,
                enum pcx_avt_rule_subject subject)
{
        pcx_buffer_set_length(&rule->actions,
                              rule->actions.length +
                              sizeof (struct pcx_parser_rule_action));
        struct pcx_parser_rule_action *action =
                ((struct pcx_parser_rule_action *)
                 (rule->actions.data + rule->actions.length)) - 1;

        action->subject = subject;

        return action;
}

static enum pcx_parser_return
parse_action_attribute_param(struct pcx_parser *parser,
                             struct pcx_parser_rule_action *action,
                             enum pcx_avt_action set_action,
                             enum pcx_avt_action unset_action,
                             struct pcx_parser_attribute_set *attribute_set,
                             struct pcx_error **error)
{
        bool is_set = false;

        enum pcx_parser_return ret =
                parse_rule_attribute_param(parser,
                                           &action->param,
                                           &is_set,
                                           attribute_set,
                                           error);

        action->action = is_set ? set_action : unset_action;

        return ret;
}

static enum pcx_parser_return
parse_object_action(struct pcx_parser *parser,
                    struct pcx_parser_rule_action *action,
                    struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_SYMBOL,
                      "Expected rule action",
                      error);

        switch (token->symbol_value) {
        case PCX_LEXER_KEYWORD_NOTHING:
                action->action = PCX_AVT_ACTION_NOTHING;
                action->param.type = PCX_PARSER_RULE_PARAMETER_TYPE_NONE;
                return PCX_PARSER_RETURN_OK;
        case PCX_LEXER_KEYWORD_ELSEWHERE:
                action->action = PCX_AVT_ACTION_MOVE_TO;
                return parse_rule_room_param(parser, &action->param, error);
        case PCX_LEXER_KEYWORD_END:
                action->action = PCX_AVT_ACTION_CHANGE_END;
                return parse_rule_int_param(parser, &action->param, error);
        case PCX_LEXER_KEYWORD_SHOTS:
                action->action = PCX_AVT_ACTION_CHANGE_SHOTS;
                return parse_rule_int_param(parser, &action->param, error);
        case PCX_LEXER_KEYWORD_WEIGHT:
                action->action = PCX_AVT_ACTION_CHANGE_WEIGHT;
                return parse_rule_int_param(parser, &action->param, error);
        case PCX_LEXER_KEYWORD_SIZE:
                action->action = PCX_AVT_ACTION_CHANGE_SIZE;
                return parse_rule_int_param(parser, &action->param, error);
        case PCX_LEXER_KEYWORD_CONTAINER_SIZE:
                action->action = PCX_AVT_ACTION_CHANGE_CONTAINER_SIZE;
                return parse_rule_int_param(parser, &action->param, error);
        case PCX_LEXER_KEYWORD_BURN_TIME:
                action->action = PCX_AVT_ACTION_CHANGE_BURN_TIME;
                return parse_rule_int_param(parser, &action->param, error);
        case PCX_LEXER_KEYWORD_ADJECTIVE:
                action->action = PCX_AVT_ACTION_CHANGE_OBJECT_ADJECTIVE;
                return parse_rule_object_param(parser, &action->param, error);
        case PCX_LEXER_KEYWORD_NAME:
                action->action = PCX_AVT_ACTION_CHANGE_OBJECT_NAME;
                return parse_rule_object_param(parser, &action->param, error);
        case PCX_LEXER_KEYWORD_COPY:
                action->action = PCX_AVT_ACTION_COPY_OBJECT;
                return parse_rule_object_param(parser, &action->param, error);
        case PCX_LEXER_KEYWORD_ATTRIBUTE:
                return parse_action_attribute_param
                        (parser,
                         action,
                         PCX_AVT_ACTION_SET_OBJECT_ATTRIBUTE,
                         PCX_AVT_ACTION_UNSET_OBJECT_ATTRIBUTE,
                         &parser->object_attributes,
                         error);
        case PCX_LEXER_KEYWORD_CARRYING:
                action->action = PCX_AVT_ACTION_CARRY;
                action->param.type = PCX_PARSER_RULE_PARAMETER_TYPE_NONE;
                return PCX_PARSER_RETURN_OK;
        }

        /* Anything else should directly be an object name */
        pcx_lexer_put_token(parser->lexer);
        action->action = PCX_AVT_ACTION_REPLACE_OBJECT;
        return parse_rule_object_param(parser, &action->param, error);
}

static enum pcx_parser_return
parse_room_action(struct pcx_parser *parser,
                  struct pcx_parser_rule_action *action,
                  struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_SYMBOL,
                      "Expected rule action",
                      error);

        if (token->symbol_value == PCX_LEXER_KEYWORD_ATTRIBUTE) {
                return parse_action_attribute_param
                        (parser,
                         action,
                         PCX_AVT_ACTION_SET_ROOM_ATTRIBUTE,
                         PCX_AVT_ACTION_UNSET_ROOM_ATTRIBUTE,
                         &parser->room_attributes,
                         error);
        }

        /* Anything else should directly be a room name */
        pcx_lexer_put_token(parser->lexer);
        action->action = PCX_AVT_ACTION_MOVE_TO;
        return parse_rule_room_param(parser, &action->param, error);
}

static enum pcx_parser_return
parse_action(struct pcx_parser *parser,
             struct pcx_parser_rule *rule,
             struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_keyword(parser, PCX_LEXER_KEYWORD_NEW, error);
        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_SYMBOL,
                      "Expected action",
                      error);

        struct pcx_parser_rule_action *action;

        switch (token->symbol_value) {
        case PCX_LEXER_KEYWORD_OBJECT:
                action = add_rule_action(rule, PCX_AVT_RULE_SUBJECT_OBJECT);
                return parse_object_action(parser, action, error);
        case PCX_LEXER_KEYWORD_TOOL:
                action = add_rule_action(rule, PCX_AVT_RULE_SUBJECT_TOOL);
                return parse_object_action(parser, action, error);
        case PCX_LEXER_KEYWORD_ROOM:
                action = add_rule_action(rule, PCX_AVT_RULE_SUBJECT_ROOM);
                return parse_room_action(parser, action, error);
        case PCX_LEXER_KEYWORD_ATTRIBUTE:
                action = add_rule_action(rule, PCX_AVT_RULE_SUBJECT_ROOM);
                return parse_action_attribute_param
                        (parser,
                         action,
                         PCX_AVT_ACTION_SET_PLAYER_ATTRIBUTE,
                         PCX_AVT_ACTION_UNSET_PLAYER_ATTRIBUTE,
                         &parser->player_attributes,
                         error);
        default:
                pcx_lexer_put_token(parser->lexer);
                return PCX_PARSER_RETURN_NOT_MATCHED;
        }
}

static void
process_parent_condition(struct pcx_parser *parser,
                         struct pcx_parser_rule *rule,
                         struct pcx_parser_target *parent)
{
        if (parent == NULL)
                return;

        struct pcx_parser_rule_condition *cond;

        switch (parent->type) {
        case PCX_PARSER_TARGET_TYPE_OBJECT:
                cond = add_rule_condition(rule, PCX_AVT_RULE_SUBJECT_OBJECT);
                cond->condition = PCX_AVT_CONDITION_OBJECT_IS;
                cond->param.type = PCX_PARSER_RULE_PARAMETER_TYPE_INT;
                cond->param.data = parent->num;
                return;
        case PCX_PARSER_TARGET_TYPE_ROOM:
                cond = add_rule_condition(rule, PCX_AVT_RULE_SUBJECT_ROOM);
                cond->condition = PCX_AVT_CONDITION_IN_ROOM;
                cond->param.type = PCX_PARSER_RULE_PARAMETER_TYPE_INT;
                cond->param.data = parent->num;
                return;
        case PCX_PARSER_TARGET_TYPE_TEXT:
                assert(false);
        }

        assert(false);
}

static enum pcx_parser_return
parse_rule(struct pcx_parser *parser,
           struct pcx_parser_target *parent_target,
           struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_keyword(parser, PCX_LEXER_KEYWORD_RULE, error);
        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_OPEN_BRACKET,
                      "Expected ‘{’",
                      error);

        struct pcx_parser_rule *rule = pcx_calloc(sizeof *rule);
        add_target(parser, &parser->rules, &rule->base);

        pcx_buffer_init(&rule->conditions);
        pcx_buffer_init(&rule->actions);

        process_parent_condition(parser, rule, parent_target);

        bool had_verb = false;

        while (true) {
                const struct pcx_lexer_token *token =
                        pcx_lexer_get_token(parser->lexer, error);

                if (token == NULL)
                        return PCX_PARSER_RETURN_ERROR;

                if (token->type == PCX_LEXER_TOKEN_TYPE_CLOSE_BRACKET)
                        break;

                pcx_lexer_put_token(parser->lexer);

                switch (parse_properties(parser,
                                         rule_props,
                                         PCX_N_ELEMENTS(rule_props),
                                         rule,
                                         error)) {
                case PCX_PARSER_RETURN_OK:
                        continue;
                case PCX_PARSER_RETURN_NOT_MATCHED:
                        break;
                case PCX_PARSER_RETURN_ERROR:
                        return PCX_PARSER_RETURN_ERROR;
                }

                switch (parse_condition(parser, rule, error)) {
                case PCX_PARSER_RETURN_OK:
                        continue;
                case PCX_PARSER_RETURN_NOT_MATCHED:
                        break;
                case PCX_PARSER_RETURN_ERROR:
                        return PCX_PARSER_RETURN_ERROR;
                }

                switch (parse_action(parser, rule, error)) {
                case PCX_PARSER_RETURN_OK:
                        continue;
                case PCX_PARSER_RETURN_NOT_MATCHED:
                        break;
                case PCX_PARSER_RETURN_ERROR:
                        return PCX_PARSER_RETURN_ERROR;
                }

                switch (parse_verb(parser, rule, error)) {
                case PCX_PARSER_RETURN_OK:
                        had_verb = true;
                        continue;
                case PCX_PARSER_RETURN_NOT_MATCHED:
                        break;
                case PCX_PARSER_RETURN_ERROR:
                        return PCX_PARSER_RETURN_ERROR;
                }

                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Expected rule item or ‘}’ at line %i",
                              pcx_lexer_get_line_num(parser->lexer));

                return PCX_PARSER_RETURN_ERROR;
        }

        if (!had_verb) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Every rule needs at least one verb at line %i",
                              rule->base.line_num);

                return PCX_PARSER_RETURN_ERROR;
        }

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_alias(struct pcx_parser *parser,
            struct pcx_parser_target *target,
            struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_keyword(parser, PCX_LEXER_KEYWORD_ALIAS, error);

        struct pcx_parser_alias *alias = pcx_calloc(sizeof *alias);
        pcx_list_insert(parser->aliases.prev, &alias->link);

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_STRING,
                      "Expected alias name",
                      error);

        alias->name = pcx_strdup(token->string_value);

        int name_len = strlen(alias->name);

        if (name_len > 0 && alias->name[name_len - 1] == 'j') {
                alias->plural = true;
                name_len--;
        }
        if (name_len < 2 ||
            alias->name[name_len - 1] != 'o' ||
            !pcx_avt_hat_is_alphabetic_string(alias->name)) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Alias name must be a noun at line %i",
                              pcx_lexer_get_line_num(parser->lexer));
                return PCX_PARSER_RETURN_ERROR;
        }

        name_len--;

        alias->name[name_len] = '\0';

        alias->target_type = target->type;
        alias->target_num = target->num;

        return PCX_PARSER_RETURN_OK;
}

static const struct pcx_parser_property
object_props[] = {
        {
                offsetof(struct pcx_parser_object, name),
                PCX_PARSER_VALUE_TYPE_STRING,
                PCX_LEXER_KEYWORD_NAME,
                "Object already has a name",
        },
        {
                offsetof(struct pcx_parser_object, description),
                PCX_PARSER_VALUE_TYPE_TEXT,
                PCX_LEXER_KEYWORD_DESCRIPTION,
                "Object already has a description",
        },
        {
                offsetof(struct pcx_parser_object, read_text),
                PCX_PARSER_VALUE_TYPE_TEXT,
                PCX_LEXER_KEYWORD_LEGIBLE,
                "Object already has read text",
        },
        {
                offsetof(struct pcx_parser_object, points),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_POINTS,
                .min_value = 0, .max_value = UINT8_MAX,
        },
        {
                offsetof(struct pcx_parser_object, weight),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_WEIGHT,
                .min_value = 0, .max_value = UINT8_MAX,
        },
        {
                offsetof(struct pcx_parser_object, size),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_SIZE,
                .min_value = 0, .max_value = UINT8_MAX,
        },
        {
                offsetof(struct pcx_parser_object, container_size),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_CONTAINER_SIZE,
                .min_value = 0, .max_value = UINT8_MAX,
        },
        {
                offsetof(struct pcx_parser_object, shot_damage),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_SHOT_DAMAGE,
                .min_value = 0, .max_value = UINT8_MAX,
        },
        {
                offsetof(struct pcx_parser_object, shots),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_SHOTS,
                .min_value = 0, .max_value = UINT8_MAX,
        },
        {
                offsetof(struct pcx_parser_object, hit_damage),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_HIT_DAMAGE,
                .min_value = 0, .max_value = UINT8_MAX,
        },
        {
                offsetof(struct pcx_parser_object, stab_damage),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_STAB_DAMAGE,
                .min_value = 0, .max_value = UINT8_MAX,
        },
        {
                offsetof(struct pcx_parser_object, food_points),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_FOOD_POINTS,
                .min_value = 0, .max_value = UINT8_MAX,
        },
        {
                offsetof(struct pcx_parser_object, trink_points),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_TRINK_POINTS,
                .min_value = 0, .max_value = UINT8_MAX,
        },
        {
                offsetof(struct pcx_parser_object, burn_time),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_BURN_TIME,
                .min_value = 0, .max_value = UINT8_MAX,
        },
        {
                offsetof(struct pcx_parser_object, end),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_END,
                .min_value = 0, .max_value = UINT8_MAX,
        },
        {
                offsetof(struct pcx_parser_object, attributes),
                PCX_PARSER_VALUE_TYPE_CUSTOM_ATTRIBUTE,
                .attribute_set_offset =
                offsetof(struct pcx_parser, object_attributes),
        },
        {
                offsetof(struct pcx_parser_object, into),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_KEYWORD_INTO,
                "Object already has an into direction",
        },
        {
                offsetof(struct pcx_parser_object, location),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_KEYWORD_LOCATION,
                "Object already has a location",
        },
        {
                offsetof(struct pcx_parser_object, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_PORTABLE,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 object_attributes),
        },
        {
                offsetof(struct pcx_parser_object, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_CLOSABLE,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 object_attributes),
        },
        {
                offsetof(struct pcx_parser_object, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_CLOSED,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 object_attributes),
        },
        {
                offsetof(struct pcx_parser_object, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_LIGHTABLE,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 object_attributes),
        },
        {
                offsetof(struct pcx_parser_object, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_OBJECT_LIT,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 object_attributes),
        },
        {
                offsetof(struct pcx_parser_object, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_FLAMMABLE,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 object_attributes),
        },
        {
                offsetof(struct pcx_parser_object, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_LIGHTER,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 object_attributes),
        },
        {
                offsetof(struct pcx_parser_object, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_BURNING,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 object_attributes),
        },
        {
                offsetof(struct pcx_parser_object, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_BURNT_OUT,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 object_attributes),
        },
        {
                offsetof(struct pcx_parser_object, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_EDIBLE,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 object_attributes),
        },
        {
                offsetof(struct pcx_parser_object, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_DRINKABLE,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 object_attributes),
        },
        {
                offsetof(struct pcx_parser_object, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_POISONOUS,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 object_attributes),
        },
        {
                offsetof(struct pcx_parser_object, carrying),
                PCX_PARSER_VALUE_TYPE_BOOL,
                PCX_LEXER_KEYWORD_CARRYING,
        },
        {
                offsetof(struct pcx_parser_object, pronoun),
                PCX_PARSER_VALUE_TYPE_PRONOUN,
        },
};

static enum pcx_parser_return
parse_unportable(struct pcx_parser *parser,
                 struct pcx_parser_object *object,
                 struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_keyword(parser, PCX_LEXER_KEYWORD_UNPORTABLE, error);

        object->attributes &= ~(uint32_t) PCX_AVT_OBJECT_ATTRIBUTE_PORTABLE;

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_object(struct pcx_parser *parser,
             struct pcx_parser_target *parent_target,
             struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_keyword(parser, PCX_LEXER_KEYWORD_OBJECT, error);

        struct pcx_parser_object *object = pcx_calloc(sizeof *object);
        add_target(parser, &parser->objects, &object->base);

        if (!assign_symbol(parser,
                           PCX_PARSER_TARGET_TYPE_OBJECT,
                           &object->base,
                           "Expected object name",
                           error))
                return PCX_PARSER_RETURN_ERROR;

        if (parent_target) {
                object->location.symbol = parent_target->id;
                object->location.line_num = object->base.line_num;
        }

        /* Objects are portable by default */
        object->attributes = PCX_AVT_OBJECT_ATTRIBUTE_PORTABLE;

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
                                         object_props,
                                         PCX_N_ELEMENTS(object_props),
                                         object,
                                         error)) {
                case PCX_PARSER_RETURN_OK:
                        continue;
                case PCX_PARSER_RETURN_NOT_MATCHED:
                        break;
                case PCX_PARSER_RETURN_ERROR:
                        return PCX_PARSER_RETURN_ERROR;
                }

                static const item_parse_func funcs[] = {
                        parse_object,
                        parse_rule,
                };

                switch (parse_items(parser,
                                    funcs,
                                    PCX_N_ELEMENTS(funcs),
                                    &object->base, /* parent_target */
                                    error)) {
                case PCX_PARSER_RETURN_OK:
                        continue;
                case PCX_PARSER_RETURN_NOT_MATCHED:
                        break;
                case PCX_PARSER_RETURN_ERROR:
                        return PCX_PARSER_RETURN_ERROR;
                }

                switch (parse_alias(parser, &object->base, error)) {
                case PCX_PARSER_RETURN_OK:
                        continue;
                case PCX_PARSER_RETURN_NOT_MATCHED:
                        break;
                case PCX_PARSER_RETURN_ERROR:
                        return PCX_PARSER_RETURN_ERROR;
                }

                switch (parse_unportable(parser, object, error)) {
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
                              "Expected object item or ‘}’ at line %i",
                              pcx_lexer_get_line_num(parser->lexer));
                return PCX_PARSER_RETURN_ERROR;
        }

        return PCX_PARSER_RETURN_OK;
}

static const struct pcx_parser_property
room_props[] = {
        {
                offsetof(struct pcx_parser_room, name),
                PCX_PARSER_VALUE_TYPE_STRING,
                PCX_LEXER_KEYWORD_NAME,
                "Room already has a name",
        },
        {
                offsetof(struct pcx_parser_room, description),
                PCX_PARSER_VALUE_TYPE_TEXT,
                PCX_LEXER_KEYWORD_DESCRIPTION,
                "Room already has a description",
        },
        {
                offsetof(struct pcx_parser_room, movements[0]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_KEYWORD_NORTH,
                "Room already has a north direction",
        },
        {
                offsetof(struct pcx_parser_room, movements[1]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_KEYWORD_EAST,
                "Room already has an east direction",
        },
        {
                offsetof(struct pcx_parser_room, movements[2]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_KEYWORD_SOUTH,
                "Room already has a south direction",
        },
        {
                offsetof(struct pcx_parser_room, movements[3]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_KEYWORD_WEST,
                "Room already has a west direction",
        },
        {
                offsetof(struct pcx_parser_room, movements[4]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_KEYWORD_UP,
                "Room already has an up direction",
        },
        {
                offsetof(struct pcx_parser_room, movements[5]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_KEYWORD_DOWN,
                "Room already has a down direction",
        },
        {
                offsetof(struct pcx_parser_room, movements[6]),
                PCX_PARSER_VALUE_TYPE_REFERENCE,
                PCX_LEXER_KEYWORD_EXIT,
                "Room already has an exit direction",
        },
        {
                offsetof(struct pcx_parser_room, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_LIT,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 room_attributes),
        },
        {
                offsetof(struct pcx_parser_room, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_UNLIGHTABLE,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 room_attributes),
        },
        {
                offsetof(struct pcx_parser_room, attributes),
                PCX_PARSER_VALUE_TYPE_ATTRIBUTE,
                PCX_LEXER_KEYWORD_GAME_OVER,
                .attribute_set_offset = offsetof(struct pcx_parser,
                                                 room_attributes),
        },
        {
                offsetof(struct pcx_parser_room, points),
                PCX_PARSER_VALUE_TYPE_INT,
                PCX_LEXER_KEYWORD_POINTS,
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
parse_direction(struct pcx_parser *parser,
                struct pcx_parser_room *room,
                struct pcx_error **error)
{
        const struct pcx_lexer_token *token;

        check_item_keyword(parser, PCX_LEXER_KEYWORD_DIRECTION, error);

        struct pcx_parser_direction *direction = pcx_calloc(sizeof *direction);
        pcx_list_insert(room->directions.prev, &direction->link);

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_STRING,
                      "Expected direction name",
                      error);

        direction->name = pcx_strdup(token->string_value);

        int name_len = strlen(direction->name);

        if (name_len > 0 && direction->name[name_len - 1] == 'j')
                name_len--;
        if (name_len < 2 ||
            direction->name[name_len - 1] != 'o' ||
            !pcx_avt_hat_is_alphabetic_string(direction->name)) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Direction name must be a noun at line %i",
                              pcx_lexer_get_line_num(parser->lexer));
                return PCX_PARSER_RETURN_ERROR;
        }

        name_len--;

        direction->name[name_len] = '\0';

        require_token(parser,
                      PCX_LEXER_TOKEN_TYPE_SYMBOL,
                      "Expected room name",
                      error);

        direction->room.symbol = token->symbol_value;
        direction->room.line_num = pcx_lexer_get_line_num(parser->lexer);

        if (!store_text_reference(parser,
                                  &direction->description,
                                  true, /* optional */
                                  error))
                return PCX_PARSER_RETURN_ERROR;

        return PCX_PARSER_RETURN_OK;
}

static enum pcx_parser_return
parse_room(struct pcx_parser *parser,
           struct pcx_parser_target *parent_target,
           struct pcx_error **error)
{
        assert(parent_target == NULL);

        const struct pcx_lexer_token *token;

        check_item_keyword(parser, PCX_LEXER_KEYWORD_ROOM, error);

        struct pcx_parser_room *room = pcx_calloc(sizeof *room);
        add_target(parser, &parser->rooms, &room->base);
        pcx_list_init(&room->directions);

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

                static const item_parse_func funcs[] = {
                        parse_object,
                        parse_rule,
                };

                switch (parse_items(parser,
                                    funcs,
                                    PCX_N_ELEMENTS(funcs),
                                    &room->base, /* parent_target */
                                    error)) {
                case PCX_PARSER_RETURN_OK:
                        continue;
                case PCX_PARSER_RETURN_NOT_MATCHED:
                        break;
                case PCX_PARSER_RETURN_ERROR:
                        return PCX_PARSER_RETURN_ERROR;
                }

                switch (parse_direction(parser, room, error)) {
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
           struct pcx_parser_target *parent_target,
           struct pcx_error **error)
{
        assert(parent_target == 0);

        const struct pcx_lexer_token *token;

        check_item_keyword(parser, PCX_LEXER_KEYWORD_TEXT, error);

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
                PCX_LEXER_KEYWORD_NAME,
                "Room already has a name",
        },
        {
                offsetof(struct pcx_parser, game_author),
                PCX_PARSER_VALUE_TYPE_STRING,
                PCX_LEXER_KEYWORD_AUTHOR,
                "Room already has an author",
        },
        {
                offsetof(struct pcx_parser, game_year),
                PCX_PARSER_VALUE_TYPE_STRING,
                PCX_LEXER_KEYWORD_YEAR,
                "Room already has a year",
        },
        {
                offsetof(struct pcx_parser, game_intro),
                PCX_PARSER_VALUE_TYPE_STRING,
                PCX_LEXER_KEYWORD_INTRODUCTION,
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
                        parse_object,
                        parse_rule,
                };

                switch (parse_items(parser,
                                    funcs,
                                    PCX_N_ELEMENTS(funcs),
                                    NULL, /* parent_target */
                                    error)) {
                case PCX_PARSER_RETURN_OK:
                        continue;
                case PCX_PARSER_RETURN_NOT_MATCHED:
                        break;
                case PCX_PARSER_RETURN_ERROR:
                        return false;
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
                case PCX_PARSER_TARGET_TYPE_OBJECT:
                        ref = &pcx_container_of(target,
                                                struct pcx_parser_object,
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
compile_room_directions(struct pcx_parser *parser,
                        struct pcx_parser_room *room,
                        struct pcx_avt_room *avt_room,
                        struct pcx_error **error)
{
        size_t n_directions = pcx_list_length(&room->directions);

        if (n_directions == 0)
                return true;

        avt_room->n_directions = n_directions;
        avt_room->directions = pcx_calloc(sizeof (struct pcx_avt_direction) *
                                          n_directions);

        struct pcx_parser_direction *dir;
        int dir_num = 0;

        pcx_list_for_each(dir, &room->directions, link) {
                struct pcx_avt_direction *avt_dir =
                        avt_room->directions + dir_num;

                avt_dir->name = dir->name;
                dir->name = NULL;

                struct pcx_parser_target *target =
                        get_symbol_reference(parser, dir->room.symbol);

                if (target == NULL ||
                    target->type != PCX_PARSER_TARGET_TYPE_ROOM) {
                        pcx_set_error(error,
                                      &pcx_parser_error,
                                      PCX_PARSER_ERROR_INVALID,
                                      "Invalid room reference on line %i",
                                      dir->room.line_num);
                        return false;
                }

                avt_dir->target = target->num;

                if (text_reference_specified(&dir->description)) {
                        if (!resolve_text_reference(parser,
                                                    &dir->description,
                                                    error))
                                return false;

                        avt_dir->description = dir->description.text;
                }

                dir_num++;
        }

        return true;
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
        if (!compile_room_directions(parser, room, avt_room, error))
                return false;

        return true;
}

static bool
compile_movable_location(struct pcx_parser *parser,
                         struct pcx_avt_movable *movable,
                         struct pcx_parser_reference *ref,
                         struct pcx_error **error)
{
        if (ref->symbol == 0) {
                movable->location_type = PCX_AVT_LOCATION_TYPE_NOWHERE;
                return true;
        }

        struct pcx_parser_target *target =
                get_symbol_reference(parser, ref->symbol);

        if (target == NULL)
                goto error;

        switch (target->type) {
        case PCX_PARSER_TARGET_TYPE_ROOM:
                movable->location_type = PCX_AVT_LOCATION_TYPE_IN_ROOM;
                movable->location = target->num;
                return true;
        case PCX_PARSER_TARGET_TYPE_OBJECT:
                movable->location_type = PCX_AVT_LOCATION_TYPE_IN_OBJECT;
                movable->location = target->num;
                return true;
        case PCX_PARSER_TARGET_TYPE_TEXT:
                break;
        }

error:
        pcx_set_error(error,
                      &pcx_parser_error,
                      PCX_PARSER_ERROR_INVALID,
                      "Invalid location reference on line %i",
                      ref->line_num);
        return false;
}

static bool
compile_movable_name(struct pcx_parser *parser,
                     struct pcx_avt_movable *movable,
                     struct pcx_parser_pronoun *pronoun,
                     int line_num,
                     unsigned name_symbol,
                     const char *name_str,
                     struct pcx_error **error)
{
        char *name;

        if (name_str == NULL)
                name = convert_symbol_to_name(parser, name_symbol);
        else
                name = pcx_strdup(name_str);

        char *space = strchr(name, ' ');

        if (space == NULL)
                goto error;

        char *adjective = name;
        char *noun = space + 1;

        bool adjective_plural = false, noun_plural = false;

        int adjective_len = space - name;

        if (adjective_len >= 1 && adjective[adjective_len - 1] == 'j') {
                adjective_len--;
                adjective_plural = true;
        }
        if (adjective_len <= 2 || adjective[adjective_len - 1] != 'a')
                goto error;
        adjective[--adjective_len] = '\0';

        int noun_len = strlen(noun);

        if (noun_len >= 1 && noun[noun_len - 1] == 'j') {
                noun_len--;
                noun_plural = true;
        }
        if (noun_len <= 2 || noun[noun_len - 1] != 'o')
                goto error;
        noun[--noun_len] = '\0';

        if (!pcx_avt_hat_is_alphabetic_string(noun) ||
            !pcx_avt_hat_is_alphabetic_string(adjective))
                goto error;

        movable->name = pcx_strdup(noun);
        movable->adjective = pcx_strdup(adjective);
        pcx_free(name);

        if (adjective_plural != noun_plural) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Object’s adjective and noun don’t have the "
                              "same plurality at line %i",
                              line_num);
                return false;
        }

        if (noun_plural) {
                if (pronoun->specified &&
                    pronoun->value != PCX_AVT_PRONOUN_PLURAL) {
                        pcx_set_error(error,
                                      &pcx_parser_error,
                                      PCX_PARSER_ERROR_INVALID,
                                      "Object has a plural name but a "
                                      "non-plural pronoun at line %i",
                                      line_num);
                        return false;
                }

                pronoun->specified = true;
                pronoun->value = PCX_AVT_PRONOUN_PLURAL;
        }

        return true;

error:
        pcx_free(name);

        pcx_set_error(error,
                      &pcx_parser_error,
                      PCX_PARSER_ERROR_INVALID,
                      "The object name must be an adjective followed by a noun "
                      "at line %i",
                      line_num);
        return false;
}

static bool
compile_object(struct pcx_parser *parser,
               struct pcx_parser_object *object,
               struct pcx_avt_object *avt_object,
               struct pcx_error **error)
{
        if (text_reference_specified(&object->description)) {
                if (!resolve_text_reference(parser,
                                            &object->description,
                                            error))
                        return false;

                avt_object->base.description = object->description.text;
        }

        if (text_reference_specified(&object->read_text)) {
                if (!resolve_text_reference(parser, &object->read_text, error))
                        return false;

                avt_object->read_text = object->read_text.text;
        }

        if (object->into.symbol == 0) {
                avt_object->enter_room = PCX_AVT_DIRECTION_BLOCKED;
        } else {
                struct pcx_parser_target *target =
                        get_symbol_reference(parser, object->into.symbol);

                if (target == NULL ||
                    target->type != PCX_PARSER_TARGET_TYPE_ROOM) {
                        pcx_set_error(error,
                                      &pcx_parser_error,
                                      PCX_PARSER_ERROR_INVALID,
                                      "Invalid room reference on line %i",
                                      object->into.line_num);
                        return false;
                }

                avt_object->enter_room = target->num;
        }

        if (object->carrying && object->location.symbol) {
                pcx_set_error(error,
                              &pcx_parser_error,
                              PCX_PARSER_ERROR_INVALID,
                              "Object is marked as carried but it also has a "
                              "location at line %i",
                              object->base.line_num);
                return false;
        }

        if (object->carrying) {
                avt_object->base.location_type = PCX_AVT_LOCATION_TYPE_CARRYING;
        } else if (!compile_movable_location(parser,
                                             &avt_object->base,
                                             &object->location,
                                             error)) {
                return false;
        }

        if (!compile_movable_name(parser,
                                  &avt_object->base,
                                  &object->pronoun,
                                  object->base.line_num,
                                  object->base.id,
                                  object->name,
                                  error))
                return false;

        avt_object->base.attributes = object->attributes;

        avt_object->points = object->points;
        avt_object->weight = object->weight;
        avt_object->size = object->size;
        avt_object->shot_damage = object->shot_damage;
        avt_object->shots = object->shots;
        avt_object->hit_damage = object->hit_damage;
        avt_object->stab_damage = object->stab_damage;
        avt_object->food_points = object->food_points;
        avt_object->trink_points = object->trink_points;
        avt_object->burn_time = object->burn_time;
        avt_object->end = object->end;
        avt_object->container_size = object->container_size;

        if (object->pronoun.specified)
                avt_object->base.pronoun = object->pronoun.value;
        else
                avt_object->base.pronoun = PCX_AVT_PRONOUN_ANIMAL;

        return true;
}

static bool
compile_alias(struct pcx_parser *parser,
              struct pcx_parser_alias *alias,
              struct pcx_avt_alias *avt_alias,
              struct pcx_error **error)
{
        switch (alias->target_type) {
        case PCX_PARSER_TARGET_TYPE_OBJECT:
                avt_alias->type = PCX_AVT_ALIAS_TYPE_OBJECT;
                break;
        case PCX_PARSER_TARGET_TYPE_TEXT:
        case PCX_PARSER_TARGET_TYPE_ROOM:
                assert(false);
        }

        avt_alias->plural = alias->plural;
        avt_alias->index = alias->target_num;
        avt_alias->name = alias->name;
        alias->name = NULL;

        return true;
}

static bool
compile_verb(struct pcx_parser *parser,
             struct pcx_parser_verb *verb,
             struct pcx_avt_verb *avt_verb,
             struct pcx_error **error)
{
        avt_verb->name = verb->name;
        verb->name = NULL;

        avt_verb->n_rules = verb->rules.length / sizeof(uint16_t);
        avt_verb->rules = pcx_memdup(verb->rules.data, verb->rules.length);

        return true;
}

static bool
compile_rule_param(struct pcx_parser *parser,
                   const struct pcx_parser_rule_parameter *param,
                   uint8_t *data,
                   struct pcx_error **error)
{
        struct pcx_parser_target *target;

        switch (param->type) {
        case PCX_PARSER_RULE_PARAMETER_TYPE_NONE:
                break;
        case PCX_PARSER_RULE_PARAMETER_TYPE_INT:
                *data = param->data;
                break;
        case PCX_PARSER_RULE_PARAMETER_TYPE_OBJECT:
                target = get_symbol_reference(parser,
                                              param->reference.symbol);
                if (target == NULL ||
                    target->type != PCX_PARSER_TARGET_TYPE_OBJECT) {
                        pcx_set_error(error,
                                      &pcx_parser_error,
                                      PCX_PARSER_ERROR_INVALID,
                                      "Expected object name at line %i",
                                      param->reference.line_num);
                        return false;
                }
                *data = target->num;
                break;
        case PCX_PARSER_RULE_PARAMETER_TYPE_ROOM:
                target = get_symbol_reference(parser,
                                              param->reference.symbol);
                if (target == NULL ||
                    target->type != PCX_PARSER_TARGET_TYPE_ROOM) {
                        pcx_set_error(error,
                                      &pcx_parser_error,
                                      PCX_PARSER_ERROR_INVALID,
                                      "Expected room name at line %i",
                                      param->reference.line_num);
                        return false;
                }
                *data = target->num;
                break;
        }

        return true;
}

static bool
compile_conditions(struct pcx_parser *parser,
                   size_t n_conditions,
                   const struct pcx_parser_rule_condition *conditions,
                   struct pcx_avt_condition_data *avt_conditions,
                   struct pcx_error **error)
{
        for (size_t i = 0; i < n_conditions; i++) {
                avt_conditions[i].subject = conditions[i].subject;
                avt_conditions[i].condition = conditions[i].condition;

                if (!compile_rule_param(parser,
                                        &conditions[i].param,
                                        &avt_conditions[i].data,
                                        error))
                        return false;
        }

        return true;
}

static bool
compile_actions(struct pcx_parser *parser,
                size_t n_actions,
                const struct pcx_parser_rule_action *actions,
                struct pcx_avt_action_data *avt_actions,
                struct pcx_error **error)
{
        for (size_t i = 0; i < n_actions; i++) {
                avt_actions[i].subject = actions[i].subject;
                avt_actions[i].action = actions[i].action;

                if (!compile_rule_param(parser,
                                        &actions[i].param,
                                        &avt_actions[i].data,
                                        error))
                        return false;
        }

        return true;
}

static bool
rule_has_condition_subject(const struct pcx_parser_rule *rule,
                           enum pcx_avt_rule_subject subject)
{
        size_t n_conditions = (rule->conditions.length /
                               sizeof (struct pcx_parser_rule_condition));
        const struct pcx_parser_rule_condition *conditions =
                (const struct pcx_parser_rule_condition *)
                rule->conditions.data;

        for (size_t i = 0; i < n_conditions; i++) {
                if (conditions[i].subject == subject)
                        return true;
        }

        return false;
}

static void
add_default_conditions(struct pcx_parser_rule *rule)
{
        static const enum pcx_avt_rule_subject default_rule_subjects[] = {
                PCX_AVT_RULE_SUBJECT_OBJECT,
                PCX_AVT_RULE_SUBJECT_TOOL,
                PCX_AVT_RULE_SUBJECT_MONSTER,
        };

        for (size_t i = 0; i < PCX_N_ELEMENTS(default_rule_subjects); i++) {
                if (!rule_has_condition_subject(rule,
                                                default_rule_subjects[i])) {
                        struct pcx_parser_rule_condition *cond =
                                add_rule_condition(rule,
                                                   default_rule_subjects[i]);
                        cond->condition = PCX_AVT_CONDITION_NOTHING;
                        cond->param.type = PCX_PARSER_RULE_PARAMETER_TYPE_NONE;
                }
        }
}

static bool
compile_rule(struct pcx_parser *parser,
             struct pcx_parser_rule *rule,
             struct pcx_avt_rule *avt_rule,
             struct pcx_error **error)
{
        if (text_reference_specified(&rule->message)) {
                if (!resolve_text_reference(parser,
                                            &rule->message,
                                            error))
                        return false;

                avt_rule->text = rule->message.text;
        }

        avt_rule->points = rule->points;

        add_default_conditions(rule);

        avt_rule->n_conditions = (rule->conditions.length /
                                  sizeof (struct pcx_parser_rule_condition));

        if (avt_rule->n_conditions > 0) {
                avt_rule->conditions =
                        pcx_alloc(avt_rule->n_conditions *
                                  sizeof (struct pcx_avt_condition_data));
                if (!compile_conditions(parser,
                                        avt_rule->n_conditions,
                                        (struct pcx_parser_rule_condition *)
                                        rule->conditions.data,
                                        avt_rule->conditions,
                                        error))
                        return false;
        }

        avt_rule->n_actions = (rule->actions.length /
                               sizeof (struct pcx_parser_rule_action));

        if (avt_rule->n_actions > 0) {
                avt_rule->actions =
                        pcx_alloc(avt_rule->n_actions *
                                  sizeof (struct pcx_avt_action_data));
                if (!compile_actions(parser,
                                     avt_rule->n_actions,
                                     (struct pcx_parser_rule_action *)
                                     rule->actions.data,
                                     avt_rule->actions,
                                     error))
                        return false;
        }

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

        avt->n_objects = pcx_list_length(&parser->objects);
        avt->objects = pcx_calloc(sizeof (struct pcx_avt_object) *
                                  avt->n_objects);

        struct pcx_parser_object *object;
        int object_num = 0;

        pcx_list_for_each(object, &parser->objects, base.link) {
                if (!compile_object(parser,
                                    object,
                                    avt->objects + object_num, error))
                        return false;

                object_num++;
        }

        avt->n_aliases = pcx_list_length(&parser->aliases);

        if (avt->n_aliases > 0) {
                avt->aliases = pcx_calloc(sizeof (struct pcx_avt_alias) *
                                          avt->n_aliases);

                struct pcx_parser_alias *alias;
                int alias_num = 0;

                pcx_list_for_each(alias, &parser->aliases, link) {
                        if (!compile_alias(parser,
                                           alias,
                                           avt->aliases + alias_num, error))
                                return false;

                        alias_num++;
                }
        }

        avt->n_verbs = pcx_list_length(&parser->verbs);

        if (avt->n_verbs > 0) {
                avt->verbs = pcx_calloc(sizeof (struct pcx_avt_verb) *
                                        avt->n_verbs);

                struct pcx_parser_verb *verb;
                int verb_num = 0;

                pcx_list_for_each(verb, &parser->verbs, link) {
                        if (!compile_verb(parser,
                                          verb,
                                          avt->verbs + verb_num,
                                          error))
                                return false;

                        verb_num++;
                }
        }

        avt->n_rules = pcx_list_length(&parser->rules);

        if (avt->n_rules > 0) {
                avt->rules = pcx_calloc(sizeof (struct pcx_avt_rule) *
                                        avt->n_rules);

                struct pcx_parser_rule *rule;
                int rule_num = 0;

                pcx_list_for_each(rule, &parser->rules, base.link) {
                        if (!compile_rule(parser,
                                          rule,
                                          avt->rules + rule_num, error))
                                return false;

                        rule_num++;
                }
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
destroy_verbs(struct pcx_parser *parser)
{
        struct pcx_parser_verb *verb, *tmp;

        pcx_list_for_each_safe(verb, tmp, &parser->verbs, link) {
                pcx_buffer_destroy(&verb->rules);
                pcx_free(verb->name);
                pcx_free(verb);
        }
}

static void
destroy_rules(struct pcx_parser *parser)
{
        struct pcx_parser_rule *rule, *tmp;

        pcx_list_for_each_safe(rule, tmp, &parser->rules, base.link) {
                pcx_buffer_destroy(&rule->conditions);
                pcx_buffer_destroy(&rule->actions);
                pcx_free(rule);
        }
}

static void
destroy_rooms(struct pcx_parser *parser)
{
        struct pcx_parser_room *room, *tmp;

        pcx_list_for_each_safe(room, tmp, &parser->rooms, base.link) {
                struct pcx_parser_direction *dir, *tdir;

                pcx_list_for_each_safe(dir, tdir, &room->directions, link) {
                        pcx_free(dir->name);
                        pcx_free(dir);
                }

                pcx_free(room->name);
                pcx_free(room);
        }
}

static void
destroy_objects(struct pcx_parser *parser)
{
        struct pcx_parser_object *object, *tmp;

        pcx_list_for_each_safe(object, tmp, &parser->objects, base.link) {
                pcx_free(object->name);
                pcx_free(object);
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
destroy_aliases(struct pcx_parser *parser)
{
        struct pcx_parser_alias *alias, *tmp;

        pcx_list_for_each_safe(alias, tmp, &parser->aliases, link) {
                pcx_free(alias->name);
                pcx_free(alias);
        }
}

static void
destroy_parser(struct pcx_parser *parser)
{
        destroy_rooms(parser);
        destroy_objects(parser);
        destroy_texts(parser);
        destroy_aliases(parser);
        destroy_rules(parser);
        destroy_verbs(parser);

        pcx_free(parser->game_name);
        pcx_free(parser->game_author);
        pcx_free(parser->game_year);
        pcx_free(parser->game_intro);

        pcx_buffer_destroy(&parser->room_attributes.symbols);
        pcx_buffer_destroy(&parser->object_attributes.symbols);
        pcx_buffer_destroy(&parser->player_attributes.symbols);

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
        pcx_list_init(&parser.objects);
        pcx_list_init(&parser.rules);
        pcx_list_init(&parser.verbs);
        pcx_list_init(&parser.texts);
        pcx_list_init(&parser.aliases);
        pcx_buffer_init(&parser.symbols);
        pcx_buffer_init(&parser.room_attributes.symbols);
        pcx_buffer_append(&parser.room_attributes.symbols,
                          room_base_attributes,
                          sizeof room_base_attributes);
        pcx_buffer_init(&parser.object_attributes.symbols);
        pcx_buffer_append(&parser.object_attributes.symbols,
                          object_base_attributes,
                          sizeof object_base_attributes);
        pcx_buffer_init(&parser.player_attributes.symbols);
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
