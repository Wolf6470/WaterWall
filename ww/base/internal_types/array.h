#ifndef WW_ARRAY_H_
#define WW_ARRAY_H_

/*
 * array
 * at: random access by pos
 * @effective
 * push_back,pop_back,del_nomove,swap
 * @ineffective
 * add,del
 */

#include <assert.h> // for assert
#include <stddef.h> // for NULL
#include <stdlib.h> // for malloc,realloc,free
#include <string.h> // for memorySet,memoryMove

#include "ev_memory.h"  // for EVENTLOOP_ALLOC, EVENTLOOP_FREE

#define ARRAY_INIT_SIZE     16

// #include <vector>
// typedef std::vector<type> atype;
#define ARRAY_DECL(type, atype) \
struct atype {      \
    type*   ptr;    \
    size_t  size;   \
    size_t  maxsize;\
};                  \
typedef struct atype atype;\
\
static inline type* atype##_data(atype* p) {\
    return p->ptr;\
}\
\
static inline int atype##_size(atype* p) {\
    return (int) p->size;\
}\
\
static inline int atype##_maxsize(atype* p) {\
    return (int) p->maxsize;\
}\
\
static inline int atype##_empty(atype* p) {\
    return p->size == 0;\
}\
\
static inline type* atype##_at(atype* p, int pos) {\
    if (pos < 0) {\
        pos += (int) p->size;\
    }\
    assert(pos >= 0 && (size_t) pos < p->size);\
    return p->ptr + pos;\
}\
\
static inline type* atype##_front(atype* p) {\
    return p->size == 0 ? NULL : p->ptr;\
}\
\
static inline type* atype##_back(atype* p) {\
    return p->size == 0 ? NULL : p->ptr + p->size - 1;\
}\
\
static inline void atype##_init(atype* p, int maxsize) {\
    p->size = 0;\
    p->maxsize = (size_t) maxsize;\
    EVENTLOOP_ALLOC(p->ptr, sizeof(type) * (size_t)maxsize);\
}\
\
static inline void atype##_clear(atype* p) {\
    p->size = 0;\
    memorySet(p->ptr, 0, sizeof(type) * p->maxsize);\
}\
\
static inline void atype##_cleanup(atype* p) {\
    EVENTLOOP_FREE(p->ptr);\
    p->size = p->maxsize = 0;\
}\
\
static inline void atype##_resize(atype* p, int maxsize) {\
    if (maxsize == 0) maxsize = ARRAY_INIT_SIZE;\
    p->ptr = (type*)eventloopRealloc(p->ptr, sizeof(type) * (size_t) maxsize, sizeof(type) * p->maxsize);\
    p->maxsize = (size_t) maxsize;\
}\
\
static inline void atype##_double_resize(atype* p) {\
    atype##_resize(p, (int) (p->maxsize * 2));\
}\
\
static inline void atype##_push_back(atype* p, type* elem) {\
    if (p->size == p->maxsize) {\
        atype##_double_resize(p);\
    }\
    p->ptr[p->size] = *elem;\
    p->size++;\
}\
\
static inline void atype##_pop_back(atype* p) {\
    assert(p->size > 0);\
    p->size--;\
}\
\
static inline void atype##_add(atype* p, type* elem, int pos) {\
    if (pos < 0) {\
        pos += (int) p->size;\
    }\
    assert(pos >= 0 && (size_t)pos <= p->size);\
    if (p->size == p->maxsize) {\
        atype##_double_resize(p);\
    }\
    if ((size_t)pos < p->size) {\
        memoryMove(p->ptr + pos+1, p->ptr + pos, sizeof(type) * (p->size - (size_t) pos));\
    }\
    p->ptr[pos] = *elem;\
    p->size++;\
}\
\
static inline void atype##_del(atype* p, int pos) {\
    if (pos < 0) {\
        pos += (int) p->size;\
    }\
    assert(pos >= 0 && (size_t)pos < p->size);\
    p->size--;\
    if ((size_t)pos < p->size) {\
        memoryMove(p->ptr + pos, p->ptr + pos+1, sizeof(type) * (p->size - (size_t) pos));\
    }\
}\
\
static inline void atype##_del_nomove(atype* p, int pos) {\
    if (pos < 0) {\
        pos += (int) p->size;\
    }\
    assert(pos >= 0 && (size_t) pos < p->size);\
    p->size--;\
    if ((size_t)pos < p->size) {\
        p->ptr[pos] = p->ptr[p->size];\
    }\
}\
\
static inline void atype##_swap(atype* p, int pos1, int pos2) {\
    if (pos1 < 0) {\
        pos1 += (int) p->size;\
    }\
    if (pos2 < 0) {\
        pos2 += (int) p->size;\
    }\
    type tmp = p->ptr[pos1];\
    p->ptr[pos1] = p->ptr[pos2];\
    p->ptr[pos2] = tmp;\
}\

#endif // WW_ARRAY_H_
