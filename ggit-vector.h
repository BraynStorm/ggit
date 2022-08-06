#ifndef GGIT_VECTOR_H
#define GGIT_VECTOR_H

#include <stdbool.h>
#include <stdint.h>

struct ggit_vector
{
    int size;
    int capacity;
    int value_size;
    void* data;
};

// clang-format off
void  ggit_vector_init        (struct ggit_vector* vec, int value_size);
void  ggit_vector_destroy     (struct ggit_vector* vec);
void  ggit_vector_clear       (struct ggit_vector* vec);
void  ggit_vector_insert      (struct ggit_vector* vec, int index, void* value);
void  ggit_vector_push        (struct ggit_vector* vec, void* value);
void  ggit_vector_reserve     (struct ggit_vector* vec, int at_least);
void  ggit_vector_reserve_more(struct ggit_vector* vec, int more);
void* ggit_vector_get         (struct ggit_vector* vec, int index);
// clang-format on


#define GGIT_GENERATE_VECTOR_REF_GETTER(type, name)                         \
    inline type* ggit_vector_ref_##name(struct ggit_vector* vec, int index) \
    {                                                                       \
        return (type*)ggit_vector_get(vec, index);                          \
    }

#define GGIT_GENERATE_VECTOR_VALUE_GETTER(type, name)                      \
    inline type ggit_vector_get_##name(struct ggit_vector* vec, int index) \
    {                                                                      \
        return *(type*)ggit_vector_get(vec, index);                        \
    }

#define GGIT_GENERATE_VECTOR_GETTERS(type, name)  \
    GGIT_GENERATE_VECTOR_VALUE_GETTER(type, name) \
    GGIT_GENERATE_VECTOR_REF_GETTER(type, name)

GGIT_GENERATE_VECTOR_GETTERS(long long, longlong)
GGIT_GENERATE_VECTOR_GETTERS(long, long)
GGIT_GENERATE_VECTOR_GETTERS(int, int)
GGIT_GENERATE_VECTOR_GETTERS(short, short)
GGIT_GENERATE_VECTOR_GETTERS(char, char)

GGIT_GENERATE_VECTOR_GETTERS(int64_t, i64)
GGIT_GENERATE_VECTOR_GETTERS(int32_t, i32)
GGIT_GENERATE_VECTOR_GETTERS(int16_t, i16)
GGIT_GENERATE_VECTOR_GETTERS(int8_t, i8)
GGIT_GENERATE_VECTOR_GETTERS(uint64_t, u64)
GGIT_GENERATE_VECTOR_GETTERS(uint32_t, u32)
GGIT_GENERATE_VECTOR_GETTERS(uint16_t, u16)
GGIT_GENERATE_VECTOR_GETTERS(uint8_t, u8)
GGIT_GENERATE_VECTOR_GETTERS(double, f64)
GGIT_GENERATE_VECTOR_GETTERS(float, f32)

GGIT_GENERATE_VECTOR_GETTERS(char*, string)

GGIT_GENERATE_VECTOR_GETTERS(int*, intptr)

#endif
