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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>

#include "pcx-util.h"
#include "pcx-buffer.h"
#include "pcx-file-error.h"

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

struct pcx_error_domain
pcx_avt_load_error;

struct load_data {
        struct pcx_avt *avt;
        FILE *file;
};

static bool
seek_or_error(struct load_data *data,
              size_t position,
              struct pcx_error **error)
{
        int r = fseek(data->file, position, SEEK_SET);

        if (r == -1) {
                pcx_file_error_set(error,
                                   errno,
                                   "%s",
                                   strerror(errno));
                return false;
        }

        return true;
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

                size_t got = fread(buf->data + buf->length,
                                   1, chunk_size,
                                   data->file);

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

                monster->name = extract_string(monster_data, 20, error);

                if (monster->name == NULL) {
                        ret = false;
                        goto done;
                }

                monster_data += 21;

                monster->adjective = extract_string(monster_data, 20, error);

                if (monster->name == NULL) {
                        ret = false;
                        goto done;
                }

                monster_data += 21;

                if (!extract_optional_string_num(data,
                                                 monster_data,
                                                 &monster->description,
                                                 error)) {
                        ret = false;
                        goto done;
                }

                monster_data += 2;

                if (!extract_pronoun(monster_data,
                                     &monster->pronoun,
                                     error)) {
                        ret = false;
                        goto done;
                }

                monster_data++;

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

                if (!extract_location_type(monster_data,
                                           &monster->location_type,
                                           error)) {
                        ret = false;
                        goto done;
                }

                monster_data++;

                monster->location = *(monster_data++);
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

                object->name = extract_string(object_data, 20, error);

                if (object->name == NULL) {
                        ret = false;
                        goto done;
                }

                object_data += 21;

                object->adjective = extract_string(object_data, 20, error);

                if (object->name == NULL) {
                        ret = false;
                        goto done;
                }

                object_data += 21;

                if (!extract_optional_string_num(data,
                                                 object_data,
                                                 &object->description,
                                                 error)) {
                        ret = false;
                        goto done;
                }

                object_data += 2;

                if (!extract_pronoun(object_data,
                                     &object->pronoun,
                                     error)) {
                        ret = false;
                        goto done;
                }

                object_data++;

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

                if (!extract_location_type(object_data,
                                           &object->location_type,
                                           error)) {
                        ret = false;
                        goto done;
                }

                object_data++;

                object->location = *(object_data++);

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

                /* Unknown byte */
                room_data++;

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

static bool
load_strings(struct load_data *data,
             struct pcx_error **error)
{
        if (!seek_or_error(data, PCX_AVT_LOAD_TEXT_OFFSET, error))
                return false;

        struct pcx_buffer tmp = PCX_BUFFER_STATIC_INIT;
        struct pcx_buffer strings = PCX_BUFFER_STATIC_INIT;
        bool ret = true;

        while (true) {
                uint8_t buf[PCX_AVT_LOAD_TEXT_CHUNK_SIZE];

                size_t got = fread(buf, 1, sizeof buf, data->file);

                if (got == 0) {
                        break;
                } else if (got <PCX_AVT_LOAD_TEXT_CHUNK_SIZE) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_STRING,
                                      "Incomplete string chunk encountered");
                        ret = false;
                        break;
                }

                uint8_t chunk_len = buf[0];

                if (chunk_len >= PCX_AVT_LOAD_TEXT_CHUNK_SIZE ||
                    chunk_len < 1) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_STRING,
                                      "Invalid string chunk encountered");
                        ret = false;
                        break;
                }

                bool is_end = false;

                if (buf[chunk_len] == '@') {
                        is_end = true;
                        chunk_len--;

                        while (chunk_len > 0 && buf[chunk_len] == ' ')
                                chunk_len--;
                }

                if (!convert_string_chunk(&tmp, buf + 1, chunk_len, error)) {
                        ret = false;
                        break;
                }

                if (is_end) {
                        pcx_buffer_append_c(&tmp, '\0');
                        char *s = pcx_memdup(tmp.data, tmp.length);
                        pcx_buffer_append(&strings, &s, sizeof s);

                        pcx_buffer_set_length(&tmp, 0);
                }
        }

        pcx_buffer_destroy(&tmp);

        data->avt->n_strings = strings.length / sizeof (char *);
        data->avt->strings = (char **) strings.data;

        return ret;
}

static bool
validate_location(struct load_data *data,
                  enum pcx_avt_location_type type,
                  uint8_t *location,
                  struct pcx_error **error)
{
        switch (type) {
        case PCX_AVT_LOCATION_TYPE_CARRYING:
        case PCX_AVT_LOCATION_TYPE_NOWHERE:
        case PCX_AVT_LOCATION_TYPE_IN_ROOM:
                return true;
        case PCX_AVT_LOCATION_TYPE_WITH_MONSTER:
                if (*location < 1 || *location > data->avt->n_monsters) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_ROOM,
                                      "An invalid monster number was "
                                      "referenced");
                        return false;
                }
                (*location)--;
                return true;
        case PCX_AVT_LOCATION_TYPE_IN_OBJECT:
                if (*location < 1 || *location > data->avt->n_objects) {
                        pcx_set_error(error,
                                      &pcx_avt_load_error,
                                      PCX_AVT_LOAD_ERROR_INVALID_OBJECT,
                                      "An invalid object number was "
                                      "referenced");
                        return false;
                }
                (*location)--;
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
                                       data->avt->objects[i].location_type,
                                       &data->avt->objects[i].location,
                                       error))
                        return false;
        }

        for (size_t i = 0; i < data->avt->n_monsters; i++) {
                if (!validate_location(data,
                                       data->avt->monsters[i].location_type,
                                       &data->avt->monsters[i].location,
                                       error))
                        return false;
        }

        return true;
}

struct pcx_avt *
pcx_avt_load(const char *filename,
             struct pcx_error **error)
{
        struct load_data data = {
                .avt = NULL,
        };
        bool ret = true;

        data.file = fopen(filename, "rb");
        if (data.file == NULL) {
                pcx_file_error_set(error,
                                   errno,
                                   "%s",
                                   strerror(errno));
                return NULL;
        }

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

        if (!validate_locations(&data, error)) {
                ret = false;
                goto done;
        }

done:
        fclose(data.file);

        if (ret) {
                return data.avt;
        } else {
                pcx_avt_free(data.avt);
                return NULL;
        }
}
