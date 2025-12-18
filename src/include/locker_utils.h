#ifndef LOCKER_UTILS_H
#define LOCKER_UTILS_H

#include <stddef.h>

#define DEFAULT_LOCKER_ARRAY_T_CAPACITY 16

#define DEFINE_LOCKER_ARRAY_T(type, name) \
    typedef struct { \
        type *values; \
        size_t count; \
        size_t capacity; \
    } array_##name##_t


#define init_item_array(array) \
    do { \
        array->values=NULL; \
        array->count=0; \
        array->capacity=0; \
    } while(0) \


#define locker_array_t_append(array, value) \
    do { \
        if((array)->count >= (array)->capacity) { \
            (array)->capacity = (array)->capacity ? (array)->capacity*2 : DEFAULT_LOCKER_ARRAY_T_CAPACITY; \
            (array)->values = realloc((array)->values, sizeof(value)*(array)->capacity); \
            if(!(array)->values) { \
                perror("realloc"); \
                exit(EXIT_FAILURE); \
            } \
        } \
        (array)->values[(array)->count++] = (value); \
    } while(0)

#define locker_array_t_free(array, value_free_func) \
    do { \
        for(size_t i = 0; i<(array)->count; i++) { \
            (value_free_func)((array)->values[i]); \
        }                           \
        free((array)->values); \
        (array)->count = 0; \
        (array)->capacity = 0; \
    } while(0)\


DEFINE_LOCKER_ARRAY_T(char *, str);

#endif
