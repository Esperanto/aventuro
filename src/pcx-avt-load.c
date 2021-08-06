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

#include "pcx-avt-load.h"

#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "pcx-util.h"
#include "pcx-buffer.h"

#define PCX_AVT_LOAD_TEXT_OFFSET 0xca80
#define PCX_AVT_LOAD_TEXT_CHUNK_SIZE 128

#define PCX_AVT_LOAD_ROOMS_OFFSET 0x101
#define PCX_AVT_LOAD_ROOM_SIZE 31

#define PCX_AVT_LOAD_OBJECTS_OFFSET 0x43ff
#define PCX_AVT_LOAD_OBJECT_SIZE 62

#define PCX_AVT_LOAD_MONSTERS_OFFSET 0x8c43
#define PCX_AVT_LOAD_MONSTER_SIZE 57

#define PCX_AVT_LOAD_DIRECTIONS_OFFSET 0x132b
#define PCX_AVT_LOAD_DIRECTION_SIZE 25

#define PCX_AVT_LOAD_DIRECTION_DESCRIPTION 21
#define PCX_AVT_LOAD_DIRECTION_SOURCE 23
#define PCX_AVT_LOAD_DIRECTION_TARGET 24

#define PCX_AVT_LOAD_ATTRIBUTES_OFFSET 0xc51a

#define PCX_AVT_LOAD_N_OBJECT_ATTRIBUTES 20
#define PCX_AVT_LOAD_BYTES_PER_OBJECT_ATTRIBUTE 19
#define PCX_AVT_LOAD_N_MONSTER_ATTRIBUTES 20
#define PCX_AVT_LOAD_BYTES_PER_MONSTER_ATTRIBUTE 10
#define PCX_AVT_LOAD_N_ROOM_ATTRIBUTES 20
#define PCX_AVT_LOAD_BYTES_PER_ROOM_ATTRIBUTE 19

#define PCX_AVT_LOAD_VERBS_OFFSET 0xbc41
#define PCX_AVT_LOAD_VERB_SIZE 11

#define PCX_AVT_LOAD_RULES_OFFSET 0x9cf6
#define PCX_AVT_LOAD_RULE_SIZE 20

#define PCX_AVT_LOAD_STRING_POINTERS_OFFSET 0xac96
#define PCX_AVT_LOAD_MAX_N_STRINGS 1002

#define PCX_AVT_LOAD_ALIASES_OFFSET 0x6853
#define PCX_AVT_LOAD_ALIAS_SIZE 23

#define PCX_AVT_LOAD_INFORMATION_OFFSET 0xc8e1
#define PCX_AVT_LOAD_INFORMATION_SIZE 51

enum pcx_avt_load_alias_type {
        PCX_AVT_LOAD_ALIAS_TYPE_OBJECT = 1,
        PCX_AVT_LOAD_ALIAS_TYPE_MONSTER = 3,
};

struct pcx_error_domain
pcx_avt_load_error;

struct load_data {
        struct pcx_avt *avt;
        struct pcx_source *source;
        /* Array of buffers to store the list of rules for each verb */
        struct pcx_buffer *rule_verbs;
};

static bool
seek_or_error(struct load_data *data,
              size_t position,
              struct pcx_error **error)
{
        return data->source->seek_source(data->source, position, error);
}

static bool
convert_string_chunk(struct pcx_buffer *buf,
                     const uint8_t *chunk,
                     size_t len,
                     struct pcx_error **error)
{
        for (size_t i = 0; i < len; i++) {
                switch (chunk[i]) {
                case 0x92:
                        pcx_buffer_append_string(buf, "ĥ");
                        break;
                case 0xa5:
                        pcx_buffer_append_string(buf, "ŝ");
                        break;
                case 0x90:
                        pcx_buffer_append_string(buf, "ĝ");
                        break;
                case 0x80:
                        pcx_buffer_append_string(buf, "ĉ");
                        break;
                case 0x96:
                        pcx_buffer_append_string(buf, "ĵ");
                        break;
                case 0x97:
                        pcx_buffer_append_string(buf, "ŭ");
                        break;
                case 0x99:
                        pcx_buffer_append_string(buf, "Ĥ");
                        break;
                case 0xa7:
                        pcx_buffer_append_string(buf, "Ŝ");
                        break;
                case 0x91:
                        pcx_buffer_append_string(buf, "Ĝ");
                        break;
                case 0x8e:
                        pcx_buffer_append_string(buf, "Ĉ");
                        break;
                case 0x9a:
                        pcx_buffer_append_string(buf, "Ŭ");
                        break;
                default:
                        if (chunk[i] == '@' ||
                            chunk[i] < 32 ||
                            chunk[i] >= 128) {
                                pcx_set_error(error,
                                              &pcx_avt_load_error,
                                              PCX_AVT_LOAD_ERROR_INVALID_STRING,
                                              "Invalid string chunk "
                                              "encountered");
                                return false;
                        }

                        pcx_buffer_append_c(buf, chunk[i]);

                        break;
                }
        }

        return true;
}

static bool
load_zero_terminated_data(struct load_data *data,
                          struct pcx_buffer *buf,
                          size_t chunk_size,
                          size_t max_entries,
                          struct pcx_error **error)
{
        size_t n_entries = 0;

        while (true) {
                if (n_entries >= max_entries)
                        goto error;

                pcx_buffer_ensure_size(buf, buf->length + chunk_size);

                size_t got = data->source->read_source(data->source,
                                                       buf->data + buf->length,
                                                       chunk_size);

                if (got < chunk_size)
                        goto error;

                if (buf->data[buf->length] == 0)
                        break;

                n_entries++;
                buf->length += chunk_size;
        }

        return true;

error:
        pcx_set_error(error,
                      &pcx_avt_load_error,
                      PCX_AVT_LOAD_ERROR_INVALID_ITEM_LIST,
                      "Invalid item list");

        return false;
}

static char *
extract_string(const uint8_t *source,
               size_t max_length,
               struct pcx_error **error)
{
        if (*source > max_length) {
                pcx_set_error(error,
                              &pcx_avt_load_error,
                              PCX_AVT_LOAD_ERROR_INVALID_STRING,
                              "String too long for the space");
                return NULL;
        }

        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;

        if (convert_string_chunk(&buf, source + 1, *source, error)) {
                pcx_buffer_append_c(&buf, '\0');
                return (char *) buf.data;
        } else {
                pcx_buffer_destroy(&buf);
                return NULL;
        }
}

static const char *
extract_string_num(struct load_data *data,
                   const uint8_t *buf,
                   struct pcx_error **error)
{
        uint16_t string_num = buf[0] | (((uint16_t) buf[1]) << 8);

        if (string_num < 1 ||
            string_num > data->avt->n_strings) {
                pcx_set_error(error,
                              &pcx_avt_load_error,
                              PCX_AVT_LOAD_ERROR_INVALID_STRING_NUM,
                              "An invalid string was referenced");
                return NULL;
        }

        return data->avt->strings[string_num - 1];
}

static bool
extract_optional_string_num(struct load_data *data,
                            const uint8_t *buf,
                            const char **out,
                            struct pcx_error **error)
{
        if (buf[0] == 0 && buf[1] == 0) {
                *out = NULL;
                return true;
        } else {
                *out = extract_string_num(data, buf, error);
                return *out != NULL;
        }
}

static bool
extract_pronoun(const uint8_t *buf,
                enum pcx_avt_pronoun *pronoun,
                struct pcx_error **error)
{
        switch ((enum pcx_avt_pronoun) *buf) {
        case PCX_AVT_PRONOUN_MAN:
        case PCX_AVT_PRONOUN_WOMAN:
        case PCX_AVT_PRONOUN_ANIMAL:
        case PCX_AVT_PRONOUN_PLURAL:
                *pronoun = *buf;
                return true;
        }

        pcx_set_error(error,
                      &pcx_avt_load_error,
                      PCX_AVT_LOAD_ERROR_INVALID_PRONOUN,
                      "An invalid pronoun number was encountered.");
        return false;
}

static bool
extract_location_type(const uint8_t *buf,
                      enum pcx_avt_location_type *location_type,
                      struct pcx_error **error)
{
        switch ((enum pcx_avt_location_type) *buf) {
        case PCX_AVT_LOCATION_TYPE_IN_ROOM:
        case PCX_AVT_LOCATION_TYPE_CARRYING:
        case PCX_AVT_LOCATION_TYPE_NOWHERE:
        case PCX_AVT_LOCATION_TYPE_WITH_MONSTER:
        case PCX_AVT_LOCATION_TYPE_IN_OBJECT:
                *location_type = *buf;
                return true;
        }

        pcx_set_error(error,
                      &pcx_avt_load_error,
                      PCX_AVT_LOAD_ERROR_INVALID_LOCATION_TYPE,
                      "An invalid location type was encountered.");
        return false;
}

static void
normalize_ending(char *dir, char ending)
{
        int len = strlen(dir);

        if (len >= 2 && dir[len - 1] == 'j')
                len--;
        if (len >= 2 && dir[len - 1] == ending)
                len--;

        dir[len] = '\0';
}

static const uint8_t *
extract_movable_base(struct load_data *data,
                     const uint8_t *movable_data,
                     struct pcx_avt_movable *movable,
                     struct pcx_error **error)
{
        movable->name = extract_string(movable_data, 20, error);

        if (movable->name == NULL)
                return NULL;

        normalize_ending(movable->name, 'o');

        movable_data += 21;

        movable->adjective = extract_string(movable_data, 20, error);

        if (movable->adjective == NULL)
                return NULL;

        normalize_ending(movable->adjective, 'a');

        movable_data += 21;

        if (!extract_optional_string_num(data,
                                         movable_data,
                                         &movable->description,
                                         error))
                return NULL;

        movable_data += 2;

        if (!extract_pronoun(movable_data,
                             &movable->pronoun,
                             error))
                return NULL;

        movable_data++;

        return movable_data;
}

static const uint8_t *
extract_movable_location(const uint8_t *movable_data,
                         struct pcx_avt_movable *movable,
                         struct pcx_error **error)
{
        if (!extract_location_type(movable_data,
                                   &movable->location_type,
                                   error))
                return NULL;

        movable_data++;

        movable->location = *(movable_data++);

        return movable_data;
}

static bool
load_monsters(struct load_data *data,
              struct pcx_error **error)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        bool ret = true;

        if (!seek_or_error(data, PCX_AVT_LOAD_MONSTERS_OFFSET, error)) {
                ret = false;
                goto done;
        }

        if (!load_zero_terminated_data(data,
                                       &buf,
                                       PCX_AVT_LOAD_MONSTER_SIZE,
                                       75, /* max_entries */
                                       error)) {
                ret = false;
                goto done;
        }

        data->avt->n_monsters = buf.length / PCX_AVT_LOAD_MONSTER_SIZE;
        data->avt->monsters = pcx_calloc(data->avt->n_monsters *
                                        sizeof (struct pcx_avt_monster));

        for (size_t obj = 0; obj < data->avt->n_monsters; obj++) {
                struct pcx_avt_monster *monster = data->avt->monsters + obj;
                const uint8_t *monster_data =
                        buf.data + obj * PCX_AVT_LOAD_MONSTER_SIZE;

                monster_data = extract_movable_base(data,
                                                    monster_data,
                                                    &monster->base,
                                                    error);

                if (monster_data == NULL) {
                        ret = false;
                        goto done;
                }

                monster->dead_object = *(monster_data++);

                if (monster->dead_object < 1 ||
                    monster->dead_object > data->avt->n_objects) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_ROOM,
                                      "A monster has an invalid dead object "
                                      "number.");
                        ret = false;
                        goto done;
                }

                monster->hunger = *(monster_data++);
                monster->thrist = *(monster_data++);

                monster->aggression = ((((int16_t) monster_data[1]) << 8) |
                                       monster_data[0]);
                monster_data += 2;

                monster->attack = *(monster_data++);
                monster->protection = *(monster_data++);
                monster->lives = *(monster_data++);
                monster->escape = *(monster_data++);
                monster->wander = *(monster_data++);

                monster_data = extract_movable_location(monster_data,
                                                        &monster->base,
                                                        error);

                if (monster_data == NULL) {
                        ret = false;
                        goto done;
                }
        }

done:
        pcx_buffer_destroy(&buf);
        return ret;
}

static void
extract_alias_index(const uint8_t *alias_data,
                    uint8_t *type,
                    uint8_t *index)
{
        *type = alias_data[21];
        *index = alias_data[22];
}

static size_t
count_aliases(const uint8_t *alias_data,
              int index,
              size_t total_n_aliases)
{
        size_t n_aliases = 1;
        uint8_t type_a, index_a;

        extract_alias_index(alias_data + index * PCX_AVT_LOAD_ALIAS_SIZE,
                            &type_a,
                            &index_a);

        while (index + n_aliases < total_n_aliases) {
                uint8_t type_b, index_b;

                extract_alias_index(alias_data +
                                    (index + n_aliases) *
                                    PCX_AVT_LOAD_ALIAS_SIZE,
                                    &type_b,
                                    &index_b);
                if (type_a != type_b || index_a != index_b)
                        break;

                n_aliases++;
        }

        return n_aliases;
}

static int
compare_alias(const void *a, const void *b)
{
        uint8_t type_a, index_a;
        uint8_t type_b, index_b;

        extract_alias_index(a, &type_a, &index_a);
        extract_alias_index(b, &type_b, &index_b);

        return (((((int) type_a) << 8) | (int) index_a) -
                ((((int) type_b) << 8) | (int) index_b));
}

static bool
load_aliases(struct load_data *data,
             struct pcx_error **error)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        bool ret = true;

        if (!seek_or_error(data, PCX_AVT_LOAD_ALIASES_OFFSET, error)) {
                ret = false;
                goto done;
        }

        if (!load_zero_terminated_data(data,
                                       &buf,
                                       PCX_AVT_LOAD_ALIAS_SIZE,
                                       400, /* max_entries */
                                       error)) {
                ret = false;
                goto done;
        }

        size_t total_n_aliases = buf.length / PCX_AVT_LOAD_ALIAS_SIZE;

        /* Sort the aliases so we can easily count how many there are
         * for each movable.
         */
        qsort(buf.data,
              total_n_aliases,
              PCX_AVT_LOAD_ALIAS_SIZE,
              compare_alias);

        for (size_t a = 0; a < total_n_aliases;) {
                const uint8_t *alias_data =
                        buf.data + a * PCX_AVT_LOAD_ALIAS_SIZE;
                uint8_t type, index;

                extract_alias_index(alias_data, &type, &index);

                struct pcx_avt_movable *movable;

                switch (type) {
                case PCX_AVT_LOAD_ALIAS_TYPE_OBJECT:
                        if (index < 1 || index > data->avt->n_objects) {
                                pcx_set_error(error,
                                              &pcx_avt_load_error,
                                              PCX_AVT_LOAD_ERROR_INVALID_OBJECT,
                                              "An invalid object number was "
                                              "referenced");
                                ret = false;
                                goto done;
                        }
                        movable = &data->avt->objects[index - 1].base;
                        goto found_type;
                case PCX_AVT_LOAD_ALIAS_TYPE_MONSTER:
                        if (index < 1 || index > data->avt->n_monsters) {
                                int e = PCX_AVT_LOAD_ERROR_INVALID_MONSTER;
                                pcx_set_error(error,
                                              &pcx_avt_load_error,
                                              e,
                                              "An invalid monster number was "
                                              "referenced");
                                ret = false;
                                goto done;
                        }
                        movable = &data->avt->monsters[index - 1].base;
                        goto found_type;
                }

                pcx_set_error(error,
                              &pcx_avt_load_error,
                              PCX_AVT_LOAD_ERROR_INVALID_ALIAS,
                              "An alias has an invalid type (0x%x)",
                              type);
                ret = false;
                goto done;

        found_type:
                assert(movable->n_aliases == 0);
                assert(movable->aliases == NULL);

                movable->n_aliases = count_aliases(buf.data,
                                                   a,
                                                   total_n_aliases);

                movable->aliases = pcx_calloc(sizeof (struct pcx_avt_alias) *
                                              movable->n_aliases);

                for (size_t i = 0; i < movable->n_aliases; i++) {
                        struct pcx_avt_alias *alias =
                                movable->aliases + i;

                        alias->name = extract_string(alias_data, 20, error);

                        if (alias->name == NULL) {
                                ret = false;
                                goto done;
                        }

                        int name_len = strlen(alias->name);

                        if (name_len > 2 && alias->name[name_len - 1] == 'j') {
                                alias->plural = true;
                                name_len--;
                        }

                        if (name_len > 2 && alias->name[name_len - 1] == 'o')
                                name_len--;

                        alias->name[name_len] = '\0';

                        alias_data += PCX_AVT_LOAD_ALIAS_SIZE;
                }

                a += movable->n_aliases;
        }

done:
        pcx_buffer_destroy(&buf);
        return ret;
}

static bool
load_attributes_array(struct load_data *data,
                      void *array,
                      size_t array_size,
                      size_t attributes_offset,
                      size_t array_entry_size,
                      size_t n_attributes,
                      size_t bytes_per_attribute,
                      struct pcx_error **error)
{
        assert(array_size <= bytes_per_attribute * 8);

        uint8_t *bytes = alloca(bytes_per_attribute);

        for (int att = 0; att < n_attributes; att++) {
                size_t got = data->source->read_source(data->source,
                                                       bytes,
                                                       bytes_per_attribute);

                if (got < bytes_per_attribute) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_ATTRIBUTES,
                                      "Invalid attributes");
                        return false;
                }

                for (int i = 1; i <= array_size; i++) {
                        if ((bytes[i / 8] & (1 << (i % 8)))) {
                                uint32_t *mask = ((uint32_t *)
                                                  ((uint8_t *) array +
                                                   (i - 1) * array_entry_size +
                                                   attributes_offset));
                                /* First attribute is called 1 in the
                                 * phenomenons so let’s use bit 1.
                                 */
                                *mask |= UINT32_C(2) << att;
                        }
                }
        }

        return true;
}

static bool
load_game_attributes(struct load_data *data,
                     struct pcx_error **error)
{
        uint8_t bytes[6];
        size_t got = data->source->read_source(data->source,
                                               bytes,
                                               sizeof bytes);

        if (got < sizeof bytes) {
                pcx_set_error(error,
                              &pcx_avt_load_error,
                              PCX_AVT_LOAD_ERROR_INVALID_ATTRIBUTES,
                              "Invalid attributes");
                return false;
        }


        for (int i = 0; i < sizeof bytes; i++)
                data->avt->game_attributes |= ((uint64_t) bytes[i]) << (8 * i);

        return true;
}

static bool
load_attributes(struct load_data *data,
                struct pcx_error **error)
{
        if (!seek_or_error(data, PCX_AVT_LOAD_ATTRIBUTES_OFFSET, error))
                return false;

        if (!load_attributes_array(data,
                                   data->avt->objects,
                                   data->avt->n_objects,
                                   offsetof(struct pcx_avt_object,
                                            base.attributes),
                                   sizeof(struct pcx_avt_object),
                                   PCX_AVT_LOAD_N_OBJECT_ATTRIBUTES,
                                   PCX_AVT_LOAD_BYTES_PER_OBJECT_ATTRIBUTE,
                                   error))
                return false;

        if (!load_attributes_array(data,
                                   data->avt->monsters,
                                   data->avt->n_monsters,
                                   offsetof(struct pcx_avt_monster,
                                            base.attributes),
                                   sizeof(struct pcx_avt_monster),
                                   PCX_AVT_LOAD_N_MONSTER_ATTRIBUTES,
                                   PCX_AVT_LOAD_BYTES_PER_MONSTER_ATTRIBUTE,
                                   error))
                return false;

        if (!load_attributes_array(data,
                                   data->avt->rooms,
                                   data->avt->n_rooms,
                                   offsetof(struct pcx_avt_room, attributes),
                                   sizeof(struct pcx_avt_room),
                                   PCX_AVT_LOAD_N_ROOM_ATTRIBUTES,
                                   PCX_AVT_LOAD_BYTES_PER_ROOM_ATTRIBUTE,
                                   error))
                return false;

        if (!load_game_attributes(data, error))
                return false;

        return true;
}

static bool
load_information(struct load_data *data,
                 struct pcx_error **error)
{
        if (!seek_or_error(data, PCX_AVT_LOAD_INFORMATION_OFFSET, error))
                return false;

        uint8_t information_data[PCX_AVT_LOAD_INFORMATION_SIZE];

        size_t got = data->source->read_source(data->source,
                                               information_data,
                                               sizeof information_data);

        if (got < sizeof information_data) {
                pcx_set_error(error,
                              &pcx_avt_load_error,
                              PCX_AVT_LOAD_ERROR_INVALID_INFORMATION,
                              "Invalid information list");
                return false;
        }

        const uint8_t *data_pos = information_data;

        data->avt->name = extract_string(data_pos, 20, error);
        if (data->avt->name == NULL)
                return false;
        data_pos += 21;

        data->avt->author = extract_string(data_pos, 20, error);
        if (data->avt->author == NULL)
                return false;
        data_pos += 21;

        data->avt->year = extract_string(data_pos, 4, error);
        if (data->avt->year == NULL)
                return false;
        data_pos += 5;

        data->avt->start_thirst =
                data_pos[0] | (((uint16_t) data_pos[1]) << 8);
        data_pos += 2;

        data->avt->start_hunger =
                data_pos[0] | (((uint16_t) data_pos[1]) << 8);
        data_pos += 2;

        return true;
}

static bool
load_verbs(struct load_data *data,
           struct pcx_error **error)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        bool ret = true;

        if (!seek_or_error(data, PCX_AVT_LOAD_VERBS_OFFSET, error)) {
                ret = false;
                goto done;
        }

        if (!load_zero_terminated_data(data,
                                       &buf,
                                       PCX_AVT_LOAD_VERB_SIZE,
                                       /* This really is 199 and not 200 */
                                       199, /* max_entries */
                                       error)) {
                ret = false;
                goto done;
        }

        data->avt->n_verbs = buf.length / PCX_AVT_LOAD_VERB_SIZE;
        data->avt->verbs = pcx_calloc(data->avt->n_verbs *
                                      sizeof (struct pcx_avt_verb));
        data->rule_verbs = pcx_alloc(data->avt->n_verbs *
                                     sizeof (struct pcx_buffer));

        for (size_t i = 0; i < data->avt->n_verbs; i++)
                pcx_buffer_init(data->rule_verbs + i);

        for (size_t i = 0; i < data->avt->n_verbs; i++) {
                data->avt->verbs[i].name =
                        extract_string(buf.data + i * PCX_AVT_LOAD_VERB_SIZE,
                                       10,
                                       error);
                if (data->avt->verbs[i].name == NULL) {
                        ret = false;
                        goto done;
                }
        }

done:
        pcx_buffer_destroy(&buf);
        return ret;
}

static bool
extract_condition(struct load_data *data,
                  enum pcx_avt_rule_subject subject,
                  struct pcx_avt_condition_data *condition,
                  const uint8_t *rule_data,
                  struct pcx_error **error)
{
        condition->subject = subject;
        condition->condition = rule_data[0];
        condition->data = rule_data[1];

        switch (condition->condition) {
        case PCX_AVT_CONDITION_OBJECT_IS:
        case PCX_AVT_CONDITION_ANOTHER_OBJECT_PRESENT:
        case PCX_AVT_CONDITION_OBJECT_SAME_ADJECTIVE:
        case PCX_AVT_CONDITION_OBJECT_SAME_NAME:
        case PCX_AVT_CONDITION_OBJECT_SAME_NOUN:
                if (condition->data < 1 ||
                    condition->data > data->avt->n_objects) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_OBJECT,
                                      "An invalid object number was "
                                      "referenced");
                        return false;
                }
                condition->data--;
                return true;

        case PCX_AVT_CONDITION_MONSTER_IS:
        case PCX_AVT_CONDITION_ANOTHER_MONSTER_PRESENT:
        case PCX_AVT_CONDITION_MONSTER_SAME_ADJECTIVE:
        case PCX_AVT_CONDITION_MONSTER_SAME_NAME:
        case PCX_AVT_CONDITION_MONSTER_SAME_NOUN:
                if (condition->data < 1 ||
                    condition->data > data->avt->n_monsters) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_MONSTER,
                                      "An invalid monster number was "
                                      "referenced");
                        return false;
                }
                condition->data--;
                return true;

        case PCX_AVT_CONDITION_IN_ROOM:
                if (condition->data < 1 ||
                    condition->data > data->avt->n_rooms) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_ROOM,
                                      "An invalid room number was "
                                      "referenced");
                        return false;
                }
                condition->data--;
                return true;

        case PCX_AVT_CONDITION_NONE:
        case PCX_AVT_CONDITION_SHOTS:
        case PCX_AVT_CONDITION_WEIGHT:
        case PCX_AVT_CONDITION_SIZE:
        case PCX_AVT_CONDITION_CONTAINER_SIZE:
        case PCX_AVT_CONDITION_BURN_TIME:
        case PCX_AVT_CONDITION_SOMETHING:
        case PCX_AVT_CONDITION_NOTHING:
        case PCX_AVT_CONDITION_OBJECT_ATTRIBUTE:
        case PCX_AVT_CONDITION_NOT_OBJECT_ATTRIBUTE:
        case PCX_AVT_CONDITION_ROOM_ATTRIBUTE:
        case PCX_AVT_CONDITION_NOT_ROOM_ATTRIBUTE:
        case PCX_AVT_CONDITION_MONSTER_ATTRIBUTE:
        case PCX_AVT_CONDITION_NOT_MONSTER_ATTRIBUTE:
        case PCX_AVT_CONDITION_PLAYER_ATTRIBUTE:
        case PCX_AVT_CONDITION_NOT_PLAYER_ATTRIBUTE:
        case PCX_AVT_CONDITION_CHANCE:
                return true;
        }

        pcx_set_error(error,
                      &pcx_avt_load_error,
                      PCX_AVT_LOAD_ERROR_INVALID_CONDITION,
                      "An invalid condition number (0x%02x) was used",
                      condition->condition);

        return false;
}

static bool
extract_action(struct load_data *data,
               enum pcx_avt_rule_subject subject,
               struct pcx_avt_action_data *action,
               const uint8_t *rule_data,
               struct pcx_error **error)
{
        action->subject = subject;
        action->action = rule_data[0];
        action->data = rule_data[1];

        switch (action->action) {
        case PCX_AVT_ACTION_REPLACE_OBJECT_IN_ROOM:
        case PCX_AVT_ACTION_REPLACE_OBJECT_IN_ROOM_2:
        case PCX_AVT_ACTION_CHANGE_OBJECT_ADJECTIVE:
        case PCX_AVT_ACTION_CHANGE_OBJECT_NAME:
        case PCX_AVT_ACTION_COPY_OBJECT:
                if (action->data < 1 ||
                    action->data > data->avt->n_objects) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_OBJECT,
                                      "An invalid object number was "
                                      "referenced");
                        return false;
                }
                action->data--;
                return true;

        case PCX_AVT_ACTION_REPLACE_MONSTER_IN_ROOM:
        case PCX_AVT_ACTION_REPLACE_MONSTER_IN_ROOM_2:
        case PCX_AVT_ACTION_CHANGE_MONSTER_ADJECTIVE:
        case PCX_AVT_ACTION_CHANGE_MONSTER_NAME:
        case PCX_AVT_ACTION_COPY_MONSTER:
                if (action->data < 1 ||
                    action->data > data->avt->n_monsters) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_MONSTER,
                                      "An invalid monster number was "
                                      "referenced");
                        return false;
                }
                action->data--;
                return true;

        case PCX_AVT_ACTION_MOVE_TO:
                if (action->data < 1 ||
                    action->data > data->avt->n_rooms) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_ROOM,
                                      "An invalid room number was "
                                      "referenced");
                        return false;
                }
                action->data--;
                return true;

        case PCX_AVT_ACTION_CHANGE_END:
        case PCX_AVT_ACTION_CHANGE_SHOTS:
        case PCX_AVT_ACTION_CHANGE_WEIGHT:
        case PCX_AVT_ACTION_CHANGE_SIZE:
        case PCX_AVT_ACTION_CHANGE_CONTAINER_SIZE:
        case PCX_AVT_ACTION_CHANGE_BURN_TIME:
        case PCX_AVT_ACTION_SOMETHING:
        case PCX_AVT_ACTION_NOTHING:
        case PCX_AVT_ACTION_NOTHING_ROOM:
        case PCX_AVT_ACTION_CARRY:
        case PCX_AVT_ACTION_SET_OBJECT_ATTRIBUTE:
        case PCX_AVT_ACTION_UNSET_OBJECT_ATTRIBUTE:
        case PCX_AVT_ACTION_SET_ROOM_ATTRIBUTE:
        case PCX_AVT_ACTION_UNSET_ROOM_ATTRIBUTE:
        case PCX_AVT_ACTION_SET_MONSTER_ATTRIBUTE:
        case PCX_AVT_ACTION_UNSET_MONSTER_ATTRIBUTE:
        case PCX_AVT_ACTION_SET_PLAYER_ATTRIBUTE:
        case PCX_AVT_ACTION_UNSET_PLAYER_ATTRIBUTE:
                return true;

                /* New actions not in the original file format */
        case PCX_AVT_ACTION_RUN_RULE:
        case PCX_AVT_ACTION_ANOTHER_OBJECT_APPEAR:
        case PCX_AVT_ACTION_CARRY_ANOTHER_OBJECT:
        case PCX_AVT_ACTION_REPLACE_OBJECT:
                break;
        }

        pcx_set_error(error,
                      &pcx_avt_load_error,
                      PCX_AVT_LOAD_ERROR_INVALID_ACTION,
                      "An invalid action number (0x%02x) was used",
                      action->action);

        return false;
}

static bool
load_rules(struct load_data *data,
           struct pcx_error **error)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        bool ret = true;

        if (!seek_or_error(data, PCX_AVT_LOAD_RULES_OFFSET, error)) {
                ret = false;
                goto done;
        }

        if (!load_zero_terminated_data(data,
                                       &buf,
                                       PCX_AVT_LOAD_RULE_SIZE,
                                       200, /* max_entries */
                                       error)) {
                ret = false;
                goto done;
        }

        data->avt->n_rules = buf.length / PCX_AVT_LOAD_RULE_SIZE;
        data->avt->rules = pcx_calloc(data->avt->n_rules *
                                      sizeof (struct pcx_avt_rule));

        for (size_t i = 0; i < data->avt->n_rules; i++) {
                const uint8_t *rule_data =
                        buf.data + i * PCX_AVT_LOAD_RULE_SIZE;
                struct pcx_avt_rule *rule = data->avt->rules + i;

                if (rule_data[0] < 1 || rule_data[0] > data->avt->n_verbs) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_VERB,
                                      "An invalid verb was referenced");
                        ret = false;
                        goto done;
                }

                uint16_t rule_num = i;
                pcx_buffer_append(data->rule_verbs + rule_data[0] - 1,
                                  &rule_num,
                                  sizeof rule_num);

                rule_data++;

                rule->n_conditions = 4;
                rule->conditions =
                        pcx_alloc(sizeof(struct pcx_avt_condition_data) *
                                  rule->n_conditions);
                if (!extract_condition(data,
                                       PCX_AVT_RULE_SUBJECT_ROOM,
                                       rule->conditions + 0,
                                       rule_data + 0,
                                       error) ||
                    !extract_condition(data,
                                       PCX_AVT_RULE_SUBJECT_OBJECT,
                                       rule->conditions + 1,
                                       rule_data + 2,
                                       error) ||
                    !extract_condition(data,
                                       PCX_AVT_RULE_SUBJECT_TOOL,
                                       rule->conditions + 2,
                                       rule_data + 4,
                                       error) ||
                    !extract_condition(data,
                                       PCX_AVT_RULE_SUBJECT_MONSTER,
                                       rule->conditions + 3,
                                       rule_data + 6,
                                       error)) {
                        ret = false;
                        goto done;
                }

                rule_data += 8;

                if (!extract_optional_string_num(data,
                                                 rule_data,
                                                 &rule->text,
                                                 error))
                        return NULL;

                rule_data += 2;

                rule->n_actions = 4;
                rule->actions =
                        pcx_alloc(sizeof(struct pcx_avt_action_data) *
                                  rule->n_actions);

                if (!extract_action(data,
                                    PCX_AVT_RULE_SUBJECT_ROOM,
                                    rule->actions + 0,
                                    rule_data + 0,
                                    error) ||
                    !extract_action(data,
                                    PCX_AVT_RULE_SUBJECT_OBJECT,
                                    rule->actions + 1,
                                    rule_data + 2,
                                    error) ||
                    !extract_action(data,
                                    PCX_AVT_RULE_SUBJECT_TOOL,
                                    rule->actions + 2,
                                    rule_data + 4,
                                    error) ||
                    !extract_action(data,
                                    PCX_AVT_RULE_SUBJECT_MONSTER,
                                    rule->actions + 3,
                                    rule_data + 6,
                                    error)) {
                        ret = false;
                        goto done;
                }

                rule_data += 8;

                rule->points = *(rule_data++);
        }

done:
        pcx_buffer_destroy(&buf);
        return ret;
}

static bool
extract_insideness(struct load_data *data,
                   const uint8_t *buf,
                   struct pcx_avt_object *object,
                   struct pcx_error **error)
{
        object->enter_room = PCX_AVT_DIRECTION_BLOCKED;

        switch (buf[0]) {
        case 0x09:
                object->container_size = buf[1];
                return true;
        case 0x0c:
                return true;
        case 0x00:
                if (buf[1] < 1 || buf[1] > data->avt->n_rooms) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_INSIDENESS,
                                      "An object leads to an invalid room");
                        return false;
                }
                object->enter_room = buf[1] - 1;
                return true;
        }

        pcx_set_error(error,
                      &pcx_avt_load_error,
                      PCX_AVT_LOAD_ERROR_INVALID_INSIDENESS,
                      "An invalid insideness value was encountered.");
        return false;
}

static bool
load_objects(struct load_data *data,
             struct pcx_error **error)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        bool ret = true;

        if (!seek_or_error(data, PCX_AVT_LOAD_OBJECTS_OFFSET, error)) {
                ret = false;
                goto done;
        }

        if (!load_zero_terminated_data(data,
                                       &buf,
                                       PCX_AVT_LOAD_OBJECT_SIZE,
                                       150, /* max_entries */
                                       error)) {
                ret = false;
                goto done;
        }

        data->avt->n_objects = buf.length / PCX_AVT_LOAD_OBJECT_SIZE;
        data->avt->objects = pcx_calloc(data->avt->n_objects *
                                        sizeof (struct pcx_avt_object));

        for (size_t obj = 0; obj < data->avt->n_objects; obj++) {
                struct pcx_avt_object *object = data->avt->objects + obj;
                const uint8_t *object_data =
                        buf.data + obj * PCX_AVT_LOAD_OBJECT_SIZE;

                object_data = extract_movable_base(data,
                                                   object_data,
                                                   &object->base,
                                                   error);

                if (object_data == NULL) {
                        ret = false;
                        goto done;
                }

                object->points = *(object_data++);
                object->weight = *(object_data++);
                object->size = *(object_data++);
                object->shot_damage = *(object_data++);
                object->shots = *(object_data++);
                object->hit_damage = *(object_data++);
                object->stab_damage = *(object_data++);

                if (!extract_optional_string_num(data,
                                                 object_data,
                                                 &object->read_text,
                                                 error)) {
                        ret = false;
                        goto done;
                }

                object_data += 2;

                object->food_points = *(object_data++);
                object->trink_points = *(object_data++);
                object->burn_time = *(object_data++);
                object->end = *(object_data++);

                object_data = extract_movable_location(object_data,
                                                       &object->base,
                                                       error);

                if (object_data == NULL) {
                        ret = false;
                        goto done;
                }

                if (!extract_insideness(data,
                                        object_data,
                                        object,
                                        error)) {
                        ret = false;
                        goto done;
                }

                object_data += 2;
        }

done:
        pcx_buffer_destroy(&buf);
        return ret;
}

static bool
extract_movements(struct load_data *data,
                  const uint8_t *room_data,
                  struct pcx_avt_room *room,
                  struct pcx_error **error)
{
        for (int i = 0; i < PCX_AVT_N_DIRECTIONS; i++) {
                if (room_data[i] == 0) {
                        room->movements[i] = PCX_AVT_DIRECTION_BLOCKED;
                } else if (room_data[i] > data->avt->n_rooms) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_ROOM,
                                      "A room direction points to an invalid "
                                      "room");
                        return false;
                } else {
                        room->movements[i] = room_data[i] - 1;
                }
        }

        return true;
}

static bool
load_rooms(struct load_data *data,
           struct pcx_error **error)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        bool ret = true;

        if (!seek_or_error(data, PCX_AVT_LOAD_ROOMS_OFFSET, error)) {
                ret = false;
                goto done;
        }

        if (!load_zero_terminated_data(data,
                                       &buf,
                                       PCX_AVT_LOAD_ROOM_SIZE,
                                       150, /* max_entries */
                                       error)) {
                ret = false;
                goto done;
        }

        data->avt->n_rooms = buf.length / PCX_AVT_LOAD_ROOM_SIZE;

        if (data->avt->n_rooms == 0) {
                pcx_set_error(error,
                              &pcx_avt_load_error,
                              PCX_AVT_LOAD_ERROR_INVALID_ROOM,
                              "No rooms are defined");
                ret = false;
                goto done;
        }

        data->avt->rooms = pcx_calloc(data->avt->n_rooms *
                                      sizeof (struct pcx_avt_room));

        for (size_t room_num = 0; room_num < data->avt->n_rooms; room_num++) {
                struct pcx_avt_room *room = data->avt->rooms + room_num;
                const uint8_t *room_data =
                        buf.data + room_num * PCX_AVT_LOAD_ROOM_SIZE;

                room->name = extract_string(room_data, 20, error);

                if (room->name == NULL) {
                        ret = false;
                        goto done;
                }

                room_data += 21;

                room->description = extract_string_num(data, room_data, error);

                if (room->description == NULL) {
                        ret = false;
                        goto done;
                }

                room_data += 2;

                room->points = *(room_data++);

                if (!extract_movements(data, room_data, room, error)) {
                        ret = false;
                        goto done;
                }
        }

done:
        pcx_buffer_destroy(&buf);
        return ret;
}

static bool
extract_directions(struct load_data *data,
                   struct pcx_avt_direction *directions,
                   const uint8_t *p,
                   size_t n_directions,
                   struct pcx_error **error)
{
        for (size_t i = 0; i < n_directions; i++) {
                directions[i].name = extract_string(p, 20, error);

                if (directions[i].name == NULL)
                        return false;

                normalize_ending(directions[i].name, 'o');

                const uint8_t *desc = p + PCX_AVT_LOAD_DIRECTION_DESCRIPTION;

                if (!extract_optional_string_num(data,
                                                 desc,
                                                 &directions[i].description,
                                                 error))
                        return false;

                uint8_t target = p[PCX_AVT_LOAD_DIRECTION_TARGET];

                if (target < 1 || target > data->avt->n_rooms) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_DIRECTION,
                                      "A direction has an invalid target");
                        return false;
                }

                directions[i].target = target - 1;

                p += PCX_AVT_LOAD_DIRECTION_SIZE;
        }

        return true;
}

static int
compare_direction_source(const void *ar, const void *br)
{
        const uint8_t *a = ar, *b = br;

        return ((int) a[PCX_AVT_LOAD_DIRECTION_SOURCE] -
                (int) b[PCX_AVT_LOAD_DIRECTION_SOURCE]);
}

static bool
load_directions(struct load_data *data,
                struct pcx_error **error)
{
        struct pcx_buffer buf = PCX_BUFFER_STATIC_INIT;
        bool ret = true;

        if (!seek_or_error(data, PCX_AVT_LOAD_DIRECTIONS_OFFSET, error)) {
                ret = false;
                goto done;
        }

        if (!load_zero_terminated_data(data,
                                       &buf,
                                       PCX_AVT_LOAD_DIRECTION_SIZE,
                                       500, /* max_entries */
                                       error)) {
                ret = false;
                goto done;
        }

        size_t n_directions = buf.length / PCX_AVT_LOAD_DIRECTION_SIZE;

        /* Sort the directions according to the source room so we can
         * easily count how many directions there are with the same
         * room.
         */
        qsort(buf.data,
              n_directions,
              PCX_AVT_LOAD_DIRECTION_SIZE,
              compare_direction_source);

        for (size_t direction_num = 0; direction_num < n_directions;) {
                uint8_t source_room =
                        buf.data[PCX_AVT_LOAD_DIRECTION_SIZE *
                                 direction_num +
                                 PCX_AVT_LOAD_DIRECTION_SOURCE];

                if (source_room < 1 || source_room > data->avt->n_rooms) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_DIRECTION,
                                      "A direction has an invalid source");
                        ret = false;
                        goto done;
                }

                int room_directions = 1;

                while (room_directions + direction_num < n_directions &&
                       buf.data[PCX_AVT_LOAD_DIRECTION_SIZE *
                                (room_directions + direction_num) +
                                PCX_AVT_LOAD_DIRECTION_SOURCE] ==
                       source_room)
                        room_directions++;

                struct pcx_avt_room *room = data->avt->rooms + source_room - 1;

                room->directions =
                        pcx_calloc(room_directions *
                                   sizeof (struct pcx_avt_direction));
                room->n_directions = room_directions;

                if (!extract_directions(data,
                                        room->directions,
                                        buf.data +
                                        direction_num *
                                        PCX_AVT_LOAD_DIRECTION_SIZE,
                                        room_directions,
                                        error)) {
                        ret = false;
                        goto done;
                }

                direction_num += room_directions;
        }

done:
        pcx_buffer_destroy(&buf);
        return ret;
}

static int
count_strings(struct load_data *data,
              struct pcx_error **error)
{
        if (!seek_or_error(data, PCX_AVT_LOAD_STRING_POINTERS_OFFSET, error))
                return -1;

        uint32_t pointer;
        int n_strings = 0;

        /* There is a zero-terminated list of DOS-style far pointers
         * in the file pointing to each string. This seems to be the
         * only way to determine how many strings are in the file.
         */

        while (true) {
                if (n_strings >= PCX_AVT_LOAD_MAX_N_STRINGS) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_STRING,
                                      "Too many strings");
                        return -1;
                }

                size_t got = data->source->read_source(data->source,
                                                       &pointer,
                                                       sizeof pointer);

                if (got < sizeof pointer) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_STRING,
                                      "Failed to load string pointer table");
                        return -1;
                }

                if (pointer == 0)
                        break;

                n_strings++;
        }

        return n_strings;
}

static bool
read_string(struct load_data *data,
            struct pcx_buffer *out_buf,
            struct pcx_error **error)
{
        uint8_t buf[PCX_AVT_LOAD_TEXT_CHUNK_SIZE];
        bool had_data = false;

        while (true) {
                size_t got = data->source->read_source(data->source,
                                                       buf,
                                                       sizeof buf);

                if (got == 0) {
                        break;
                } else if (got < PCX_AVT_LOAD_TEXT_CHUNK_SIZE) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_STRING,
                                      "Incomplete string chunk encountered");
                        return false;
                }

                had_data = true;

                uint8_t chunk_len = buf[0];

                if (chunk_len >= PCX_AVT_LOAD_TEXT_CHUNK_SIZE ||
                    chunk_len < 1) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_STRING,
                                      "Invalid string chunk encountered");
                        return false;
                }

                bool is_end = false;

                if (buf[chunk_len] == '@') {
                        is_end = true;
                        chunk_len--;

                        while (chunk_len > 0 && buf[chunk_len] == ' ')
                                chunk_len--;
                }

                if (!convert_string_chunk(out_buf, buf + 1, chunk_len, error))
                        return false;

                if (is_end)
                        break;
        }

        if (had_data)
                pcx_buffer_append_c(out_buf, '\0');

        return true;
}

static bool
load_strings(struct load_data *data,
             struct pcx_error **error)
{
        int n_strings = count_strings(data, error);

        if (n_strings < 0)
                return false;

        if (!seek_or_error(data, PCX_AVT_LOAD_TEXT_OFFSET, error))
                return false;

        struct pcx_buffer tmp = PCX_BUFFER_STATIC_INIT;
        bool ret = true;

        data->avt->n_strings = n_strings;
        data->avt->strings = pcx_calloc(n_strings * sizeof (char *));

        for (int i = 0; i < n_strings; i++) {
                pcx_buffer_set_length(&tmp, 0);

                if (!read_string(data, &tmp, error)) {
                        ret = false;
                        break;
                }

                if (tmp.length <= 0) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_STRING,
                                      "Not enough strings in the file");
                        ret = false;
                        break;
                }

                data->avt->strings[i] = pcx_strdup((char *) tmp.data);
        }

        if (ret) {
                pcx_buffer_set_length(&tmp, 0);

                /* If there is another string after the main strings
                 * then it is printed as the introductory text.
                 */
                if (!read_string(data, &tmp, error))
                        ret = false;
                else if (tmp.length > 0)
                        data->avt->introduction = pcx_strdup((char *) tmp.data);
        }

        pcx_buffer_destroy(&tmp);

        return ret;
}

static bool
validate_location(struct load_data *data,
                  struct pcx_avt_movable *movable,
                  struct pcx_error **error)
{
        switch (movable->location_type) {
        case PCX_AVT_LOCATION_TYPE_CARRYING:
        case PCX_AVT_LOCATION_TYPE_NOWHERE:
                return true;
        case PCX_AVT_LOCATION_TYPE_IN_ROOM:
                if (movable->location < 1 ||
                    movable->location > data->avt->n_rooms) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_ROOM,
                                      "An invalid room number was "
                                      "referenced");
                        return false;
                }
                movable->location--;
                return true;
        case PCX_AVT_LOCATION_TYPE_WITH_MONSTER:
                if (movable->location < 1 ||
                    movable->location > data->avt->n_monsters) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_MONSTER,
                                      "An invalid monster number was "
                                      "referenced");
                        return false;
                }
                movable->location--;
                return true;
        case PCX_AVT_LOCATION_TYPE_IN_OBJECT:
                if (movable->location < 1 ||
                    movable->location > data->avt->n_objects) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_OBJECT,
                                      "An invalid object number was "
                                      "referenced");
                        return false;
                }
                movable->location--;
                return true;
        }

        assert(false);
}

static bool
validate_locations(struct load_data *data,
                   struct pcx_error **error)
{
        for (size_t i = 0; i < data->avt->n_objects; i++) {
                if (!validate_location(data,
                                       &data->avt->objects[i].base,
                                       error))
                        return false;
        }

        for (size_t i = 0; i < data->avt->n_monsters; i++) {
                if (!validate_location(data,
                                       &data->avt->monsters[i].base,
                                       error))
                        return false;
        }

        return true;
}

static void
finalize_rule_verbs(struct load_data *data)
{
        for (size_t i = 0; i < data->avt->n_verbs; i++) {
                struct pcx_avt_verb *verb = data->avt->verbs + i;
                struct pcx_buffer *buf = data->rule_verbs + i;

                verb->n_rules = buf->length / sizeof (uint16_t);
                verb->rules = pcx_memdup(buf->data, buf->length);
                pcx_buffer_destroy(buf);
        }

        pcx_free(data->rule_verbs);
}

struct pcx_avt *
pcx_avt_load(struct pcx_source *source,
             struct pcx_error **error)
{
        struct load_data data = {
                .avt = NULL,
                .source = source,
        };
        bool ret = true;

        data.avt = pcx_calloc(sizeof (struct pcx_avt));

        if (!load_strings(&data, error)) {
                ret = false;
                goto done;
        }

        if (!load_rooms(&data, error)) {
                ret = false;
                goto done;
        }

        if (!load_directions(&data, error)) {
                ret = false;
                goto done;
        }

        if (!load_objects(&data, error)) {
                ret = false;
                goto done;
        }

        if (!load_monsters(&data, error)) {
                ret = false;
                goto done;
        }

        if (!load_aliases(&data, error)) {
                ret = false;
                goto done;
        }

        if (!validate_locations(&data, error)) {
                ret = false;
                goto done;
        }

        if (!load_verbs(&data, error)) {
                ret = false;
                goto done;
        }

        if (!load_rules(&data, error)) {
                ret = false;
                goto done;
        }

        if (!load_attributes(&data, error)) {
                ret = false;
                goto done;
        }

        if (!load_information(&data, error)) {
                ret = false;
                goto done;
        }

done:
        if (data.rule_verbs)
                finalize_rule_verbs(&data);

        if (ret) {
                return data.avt;
        } else {
                pcx_avt_free(data.avt);
                return NULL;
        }
}
