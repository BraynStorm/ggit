#include "ggit-vector.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static bool
ggit_realloc(void** data_block, int element_size, int old_elements, int new_elements)
{
    void* old_block = *data_block;
    void* new_block = realloc(old_block, new_elements * element_size);
    if (!new_block) {
        new_block = calloc(old_elements, element_size);
        if (!new_block) {
            return false;
        } else {
            memcpy(new_block, old_block, old_elements * element_size);
            free(old_block);
        }
    }
    printf("ReAlloc: %p %d->%d\n", *data_block, old_elements, new_elements);

    *data_block = new_block;
    return true;
}
static void
ggit_vector_grow(struct ggit_vector* vec, int more, bool exact)
{
    int old_capacity = vec->capacity;
    int new_capacity;
    if (more == 1 && old_capacity == 0)
        new_capacity = 8;
    else {
        /* TODO(boz):
            INEFFICIENT! Make two different versions of grow(), one for
            resize/reserve/reserve_more and one for insert/push.
        */
        if (exact) {
            new_capacity = old_capacity + more;
        } else {
            new_capacity = old_capacity + old_capacity;
            do {
                new_capacity += old_capacity;
            } while (new_capacity < old_capacity + more);
        }
    }

    if (!ggit_realloc(&vec->data, vec->value_size, vec->size, new_capacity)) {
        perror("[ggit_vector_grow] OOM.");
        abort();
    } else {
        vec->capacity = new_capacity;
    }
}

void
ggit_vector_init(struct ggit_vector* vec, int value_size)
{
    memset(vec, 0, sizeof(*vec));
    assert(vec->size == 0);
    vec->value_size = value_size;
}
void
ggit_vector_destroy(struct ggit_vector* vec)
{
    free(vec->data);
    memset(vec, 0, sizeof(*vec));
}
void
ggit_vector_clear(struct ggit_vector* vec)
{
    vec->size = 0;
}
void
ggit_vector_clear_and_free(struct ggit_vector* vec)
{
    assert(vec->value_size == sizeof(void*));

    for (int i = 0; i < vec->size; ++i)
        free(((void**)vec->data)[i]);

    ggit_vector_clear(vec);
}
void
ggit_vector_insert(struct ggit_vector* vec, int index, void const* value)
{
    int const size = vec->size;
    int const value_size = vec->value_size;

    assert(index >= 0);
    assert(index <= size);

    if (size + 1 > vec->capacity)
        ggit_vector_grow(vec, 1, false);

    int move_amount = size - index;
    if (move_amount) {
        memmove(
            ggit_vector_get(vec, index + 1),
            ggit_vector_get(vec, index),
            move_amount * value_size
        );
    }

    memcpy(ggit_vector_get(vec, index), value, value_size);
    vec->size = size + 1;
}
void
ggit_vector_push(struct ggit_vector* vec, void const* value)
{
    ggit_vector_insert(vec, vec->size, value);
}
void
ggit_vector_push_array(struct ggit_vector* vec, int n_values, void const* values)
{
    int const value_size = vec->value_size;
    int const vec_size = vec->size;

    assert(value_size > 0);
    assert(n_values >= 0);
    assert(values != NULL);

    ggit_vector_reserve_more(vec, n_values);

    memcpy((char*)vec->data + vec_size * value_size, values, n_values * value_size);
    vec->size = vec_size + n_values;
}
void
ggit_vector_push_string(struct ggit_vector* vec, char const* str)
{
    ggit_vector_push_array(vec, strlen(str), str);
}
void
ggit_vector_push_sprintf(struct ggit_vector* vec, char const* format, ...)
{
    va_list args;
    va_start(args, format);
    ggit_vector_push_vsprintf(vec, format, args);
    va_end(args);
}
void
ggit_vector_push_vsprintf(struct ggit_vector* vec, char const* format, va_list args)
{
    assert(vec->value_size == sizeof(char));

    int i = 0;
    int last_replacement = 0;

    for (; format[i]; ++i) {
        char c = format[i];

        switch (c) {
            case '%': {
                // Push the 'skipped' part of `format`.
                ggit_vector_push_array(
                    vec,
                    i - last_replacement,
                    format + last_replacement
                );

                // Get the next char.
                c = format[++i];

                switch (c) {
                    case '%': ggit_vector_push(vec, &c); break;
                    case 's':
                        ggit_vector_push_string(vec, va_arg(args, char const*));
                        break;
                }
                last_replacement = i + 1;
            } break;
        }
    }

    ggit_vector_push_string(vec, format + last_replacement);
}
void
ggit_vector_push_sprintf_terminated(struct ggit_vector* vec, char const* format, ...)
{
    va_list va;
    va_start(va, format);
    ggit_vector_push_vsprintf(vec, format, va);
    va_end(va);

    char null = 0;
    ggit_vector_push(vec, &null);
}
void
ggit_vector_reserve(struct ggit_vector* vec, int at_least)
{
    int more = at_least - vec->capacity;
    if (more > 0)
        ggit_vector_grow(vec, more, true);
}
void
ggit_vector_reserve_more(struct ggit_vector* vec, int more)
{
    more += vec->size;
    ggit_vector_reserve(vec, more);
}
void*
ggit_vector_get(struct ggit_vector* vec, int index)
{
    return (char*)vec->data + (vec->value_size * index);
}