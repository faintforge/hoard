#ifndef CORE_H
#define CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

// =============================================================================
// ALLOCATOR INTERFACE
// =============================================================================

typedef void* (*alloc_func_t)(size_t size, void* context);
typedef void* (*realloc_func_t)(void* ptr, size_t old_size, size_t new_size, void* context);
typedef void (*free_func_t)(void* ptr, size_t size, void* context);

typedef struct allocator_t allocator_t;
struct allocator_t {
    alloc_func_t alloc;
    realloc_func_t realloc;
    free_func_t free;
    void* context;
};

#define core_alloc(allocator, size) (allocator).alloc((size), (allocator).context)
#define core_realloc(allocator, ptr, old_size, new_size) (allocator).realloc((ptr), (old_size), (new_size), (allocator).context)
#define core_free(allocator, ptr, size) (allocator).free((ptr), (size), (allocator).context)

// =============================================================================
// LOGGING
// =============================================================================

typedef enum log_level_t {
    LOG_LEVEL_FATAL,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE,

    _LOG_LEVEL_COUNT,
} log_level_t;

typedef struct log_event_t log_event_t;
struct log_event_t {
    log_level_t level;
    const char* file;
    int32_t line;
    const char* message;
    va_list args;
};

typedef void (*logger_callback_func_t)(log_event_t event, void* userdata);

extern void logger_register_callback(logger_callback_func_t func, void* userdata);

#define log_fatal(...) _log_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) _log_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) _log_log(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) _log_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) _log_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_trace(...) _log_log(LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)

extern void _log_log(log_level_t level, const char* file, int32_t line, const char* message, ...);

// =============================================================================
// UTILITY
// =============================================================================

#define unused(var) (void) var

#if defined(__GNUC__)
#define debug_break() __builtin_trap()
#elif defined(_MSC_VER)
#define debug_break() __debugbreak()
#else
#define debug_break() (*(volatile int32_t*)0)
#endif

#define core_assert(expr) \
    if (expr) { \
    } else { \
        log_fatal("Assertion Failed: %s", #expr); \
        debug_break(); \
    }

#define core_assert_msg(expr, ...) \
    if (expr) { \
    } else { \
        log_fatal("Assertion Failed: %s", #expr); \
        log_fatal(__VA_ARGS__); \
        debug_break(); \
    }

extern uint32_t fvn1a_hash32(uint32_t seed, const void* data, uint32_t size);

// =============================================================================
// ARENA ALLOCATOR
// =============================================================================

typedef struct arena_t arena_t;
struct arena_t {
    allocator_t allocator;
    uint8_t* memory;
    size_t capacity;
    size_t position;
    size_t last_position;
};

extern arena_t* arena_create(allocator_t allocator, size_t capacity);
extern arena_t* arena_create_from_buffer(uint8_t* buffer, size_t capacity);
extern void arena_destroy(arena_t** arena);
extern allocator_t arena_allocator(arena_t* arena);

extern void* arena_push(arena_t* arena, size_t size);
extern void* arena_push_aligned(arena_t* arena, size_t size, size_t align);
extern void* arena_push_zero(arena_t* arena, size_t size);
extern void* arena_push_zero_aligned(arena_t* arena, size_t size, size_t align);
extern void arena_reset(arena_t* arena);

typedef struct arena_scope_t arena_scope_t;
struct arena_scope_t {
    arena_t* arena;
    size_t position;
    size_t last_position;
};

extern arena_scope_t arena_scope_begin(arena_t* arena);
extern void arena_scope_end(arena_scope_t* scope);

// =============================================================================
// DYNAMIC ARRAY
// =============================================================================

#define dyn_arr_t(T) T*

extern void* dyn_arr_create(allocator_t allocator, size_t element_size);
extern void dyn_arr_destroy(void** dyn_arr);
extern size_t dyn_arr_length(const void* dyn_arr);
extern void dyn_arr_clear(void** dyn_arr);

extern void dyn_arr_insert_arr(void** dyn_arr, size_t index, const void* arr, size_t arr_length);
extern void dyn_arr_remove_arr(void** dyn_arr, size_t index, size_t count, void* output);

extern void dyn_arr_insert(void** dyn_arr, size_t index, const void* value);
extern void dyn_arr_remove(void** dyn_arr, size_t index, void* output);

extern void dyn_arr_insert_fast(void** dyn_arr, size_t index, const void* value);
extern void dyn_arr_remove_fast(void** dyn_arr, size_t index, void* output);

extern void dyn_arr_push(void** dyn_arr, const void* value);
extern void dyn_arr_pop(void** dyn_arr, void* output);

extern void dyn_arr_push_arr(void** dyn_arr, const void* arr, size_t arr_length);
extern void dyn_arr_pop_arr(void** dyn_arr, size_t count, void* output);

// =============================================================================
// HASH MAP
// =============================================================================

typedef struct hash_map_t hash_map_t;
struct hash_map_t {
    allocator_t allocator;

    uint32_t key_size;
    uint32_t value_size;

    // SoA
    void* key_array;
    void* value_array;
    uint32_t* hash_array;
    uint8_t* state_array;

    bool (*equal_func)(const void* lhs, const void* rhs, uint32_t size);
    uint32_t (*hash_func)(const void* key, uint32_t size);

    uint32_t capacity;
    uint32_t count;
    uint8_t load_factor;
    float grow_factor;
};

typedef struct hash_map_desc_t hash_map_desc_t;
struct hash_map_desc_t {
    allocator_t allocator;
    uint32_t key_size;
    uint32_t value_size;
    uint32_t initial_capacity;
    uint8_t load_factor;
    float grow_factor;
    bool (*equal_func)(const void* lhs, const void* rhs, uint32_t size);
    uint32_t (*hash_func)(const void* key, uint32_t size);
};

extern hash_map_t hash_map_create(hash_map_desc_t desc);
extern void hash_map_destroy(hash_map_t* map);

extern bool hash_map_insert(hash_map_t* map, const void* key, const void* value);
extern bool hash_map_set(hash_map_t* map, const void* key, const void* value, void* old_value);
extern bool hash_map_remove(hash_map_t* map, const void* key, void* result_value);
extern bool hash_map_contains(const hash_map_t* map, const void* key);
extern bool hash_map_get(const hash_map_t* map, const void* key, void* result_value);
extern void* hash_map_get_ptr(const hash_map_t* map, const void* key);

#define hash_map_desc_generic(_allocator, key_type, value_type) (hash_map_desc_t) { \
        .allocator = _allocator, \
        .key_size = sizeof(key_type), \
        .value_size = sizeof(value_type), \
        .hash_func = _hm_generic_hash, \
        .equal_func = _hm_generic_equal, \
    }

extern uint32_t _hm_generic_hash(const void* key, uint32_t size);
extern bool _hm_generic_equal(const void* lhs, const void* rhs, uint32_t size);

#ifdef CORE_IMPLEMENTATION

// TODO: Remove this CRT dependency
#include <string.h>

// =============================================================================
// LOGGER
// =============================================================================

#define MAX_LOGGER_CALLBACK_COUNT 16

typedef struct _logger_callback_t _logger_callback_t;
struct _logger_callback_t {
    logger_callback_func_t func;
    void* userdata;
};

static _logger_callback_t g_logger_callbacks[MAX_LOGGER_CALLBACK_COUNT] = {0};
static uint32_t g_logger_callback_count = 0;

void logger_register_callback(logger_callback_func_t func, void* userdata) {
    core_assert_msg(g_logger_callback_count < MAX_LOGGER_CALLBACK_COUNT, "Maximum amount of logger callbacks of %d has been reached.", MAX_LOGGER_CALLBACK_COUNT);
    g_logger_callbacks[g_logger_callback_count] = (_logger_callback_t) {
        .func = func,
        .userdata = userdata,
    };
    g_logger_callback_count++;
}

void _log_log(log_level_t level, const char* file, int32_t line, const char* message, ...) {
    for (uint32_t i = 0; i < g_logger_callback_count; i++) {
        log_event_t event = {
            .level = level,
            .file = file,
            .line = line,
            .message = message,
        };
        va_start(event.args, message);
        _logger_callback_t callback = g_logger_callbacks[i];
        callback.func(event, callback.userdata);
        va_end(event.args);
    }
}

// =============================================================================
// UTILITY
// =============================================================================

uint32_t fvn1a_hash32(uint32_t seed, const void* data, uint32_t size) {
    // https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
    uint32_t hash = 0x811c9dc5 ^ seed;
    for (size_t i = 0; i < size; i++) {
        hash ^= ((uint8_t*) data)[i];
        hash *= 0x01000193;
    }
    return hash;
}

// =============================================================================
// ARENA ALLOCATOR
// =============================================================================

arena_t* arena_create(allocator_t allocator, size_t capacity) {
    arena_t* arena = core_alloc(allocator, capacity);
    *arena = (arena_t) {
        .allocator = allocator,
        .memory = (uint8_t*) arena,
        .capacity = capacity,
        .position = sizeof(arena_t),
        .last_position = sizeof(arena_t),
    };
    return arena;
}

arena_t* arena_create_from_buffer(uint8_t* buffer, size_t capacity) {
    arena_t* arena = (arena_t*) buffer;
    *arena = (arena_t) {
        .allocator = {0},
        .memory = buffer,
        .capacity = capacity,
        .position = sizeof(arena_t),
        .last_position = sizeof(arena_t),
    };
    return arena;
}

void arena_destroy(arena_t** arena) {
    core_free((*arena)->allocator, (*arena)->memory, (*arena)->capacity);
    *arena = NULL;
}

void* _arena_alloc(size_t size, void* context) {
    arena_t* arena = context;
    return arena_push(arena, size);
}

void* _arena_realloc(void* ptr, size_t old_size, size_t new_size, void* context) {
    if (old_size >= new_size) {
        return ptr;
    }
    arena_t* arena = context;
    // Resize in place if we're reallocing the last allocation.
    if ((uintptr_t) arena->memory + arena->last_position == (uintptr_t) ptr) {
        arena->position = arena->last_position;
    }
    void* new_ptr = arena_push(arena, new_size);
    memcpy(new_ptr, ptr, old_size);
    return new_ptr;
}

void _arena_free(void* ptr, size_t size, void* context) {
    unused(ptr);
    unused(size);
    unused(context);
}

allocator_t arena_allocator(arena_t* arena) {
    return (allocator_t) {
        .alloc = _arena_alloc,
        .realloc = _arena_realloc,
        .free = _arena_free,
        .context = arena,
    };
}

static bool _is_power_of_two(uintptr_t value) {
    return (value & (value - 1)) == 0;
}

static uintptr_t _align_up(uintptr_t value, size_t align) {
    core_assert(_is_power_of_two(align));
    size_t mod = value & (align - 1);
    if (mod != 0) {
        value += align - mod;
    }
    return value;
}

void* arena_push_aligned(arena_t* arena, size_t size, size_t align) {
    uintptr_t current_ptr = (uintptr_t) arena->memory + arena->position;
    uintptr_t aligned_ptr = _align_up(current_ptr, align);
    uintptr_t position = aligned_ptr - (uintptr_t) arena->memory;
    if (position + size > arena->capacity) {
        log_warn("Arena OOM");
        return NULL;
    }
    arena->last_position = position;
    arena->position = position + size;
    return &arena->memory[position];
}

void* arena_push(arena_t* arena, size_t size) {
    return arena_push_aligned(arena, size, sizeof(void*));
}

void* arena_push_zero(arena_t* arena, size_t size) {
    return arena_push_zero_aligned(arena, size, sizeof(void*));
}

void* arena_push_zero_aligned(arena_t* arena, size_t size, size_t align) {
    void* mem = arena_push_aligned(arena, size, align);
    memset(mem, 0, size);
    return mem;
}

void arena_reset(arena_t* arena) {
    arena->position = sizeof(arena_t);
    arena->last_position = sizeof(arena_t);
}

arena_scope_t arena_scope_begin(arena_t* arena) {
    return (arena_scope_t) {
        .arena = arena,
        .position = arena->position,
        .last_position = arena->last_position,
    };
}

void arena_scope_end(arena_scope_t* scope) {
    arena_t* arena = scope->arena;
    arena->position = scope->position;
    arena->last_position = scope->last_position;
    *scope = (arena_scope_t) {0};
}

// =============================================================================
// DYNAMIC ARRAY
// =============================================================================

#define _DYN_ARR_INITIAL_SIZE 8

typedef struct _dyn_arr_header_t _dyn_arr_header_t;
struct _dyn_arr_header_t {
    allocator_t allocator;
    size_t element_size;
    size_t capacity;
    size_t length;
};

static inline void* _header_to_dyn_arr(_dyn_arr_header_t* header) {
    return &header[1];
}

static inline _dyn_arr_header_t* _dyn_arr_to_header(const void* array) {
    return &((_dyn_arr_header_t*) array)[-1];
}

void* dyn_arr_create(allocator_t allocator, size_t element_size) {
    _dyn_arr_header_t* header = core_alloc(allocator, sizeof(_dyn_arr_header_t) + element_size * _DYN_ARR_INITIAL_SIZE);
    *header = (_dyn_arr_header_t) {
        .allocator = allocator,
        .element_size = element_size,
        .capacity = _DYN_ARR_INITIAL_SIZE,
        .length = 0,
    };
    return _header_to_dyn_arr(header);
}

void dyn_arr_destroy(void** dyn_arr) {
    core_assert_msg(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    core_free(header->allocator, header, sizeof(_dyn_arr_header_t) + header->element_size * _DYN_ARR_INITIAL_SIZE);
    *dyn_arr = NULL;
}

size_t dyn_arr_length(const void* dyn_arr) {
    if (dyn_arr == NULL) {
        return 0;
    }
    return _dyn_arr_to_header(dyn_arr)->length;
}

void dyn_arr_clear(void** dyn_arr) {
    core_assert_msg(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    header->length = 0;
}

static void _dyn_arr_ensure_capacity(void** dyn_arr, size_t length) {
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    if (header->capacity >= header->length + length) {
        return;
    }
    size_t prev_capacity = header->capacity;
    size_t new_capacity = header->capacity;
    while (new_capacity < header->length + length) {
        new_capacity *= 2;
    }
    header = core_realloc(header->allocator,
            header,
            sizeof(_dyn_arr_header_t) + prev_capacity * header->element_size,
            sizeof(_dyn_arr_header_t) + new_capacity * header->element_size);
    *dyn_arr = _header_to_dyn_arr(header);
}

void dyn_arr_insert_arr(void** dyn_arr, size_t index, const void* arr, size_t arr_length) {
    core_assert_msg(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    core_assert_msg(index <= header->length, "Index out of bounds");
    _dyn_arr_ensure_capacity(dyn_arr, arr_length);
    header = _dyn_arr_to_header(*dyn_arr);

    void *end = (uint8_t*) (*dyn_arr) + (index+arr_length)*header->element_size;
    void *dest = (uint8_t*) (*dyn_arr) + index*header->element_size;

    memmove(end, dest, (header->length - index)*header->element_size);
    if (arr == NULL) {
        memset(dest, 0, arr_length*header->element_size);
    } else {
        memcpy(dest, arr, arr_length*header->element_size);
    }

    header->length += arr_length;
}

void dyn_arr_remove_arr(void** dyn_arr, size_t index, size_t count, void* output) {
    core_assert_msg(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    core_assert_msg(index < header->length, "Index out of bounds");
    core_assert_msg(index+count <= header->length, "Index out of bounds");

    void *end = (uint8_t*) (*dyn_arr) + (index+count)*header->element_size;
    void *dest = (uint8_t*) (*dyn_arr) + index*header->element_size;

    if (output != NULL) {
        memcpy(output, dest, count*header->element_size);
    }
    memmove(dest, end, (header->length - index - count)*header->element_size);

    header->length -= count;
}

void dyn_arr_insert(void** dyn_arr, size_t index, const void* value) {
    dyn_arr_insert_arr(dyn_arr, index, value, 1);
}

void dyn_arr_remove(void** dyn_arr, size_t index, void* output) {
    dyn_arr_remove_arr(dyn_arr, index, 1, output); \
}

void dyn_arr_insert_fast(void** dyn_arr, size_t index, const void* value) {
    core_assert_msg(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    core_assert_msg(index <= header->length, "Index out of bounds");
    _dyn_arr_ensure_capacity(dyn_arr, 1);
    header = _dyn_arr_to_header(*dyn_arr);

    void *end = (uint8_t*) (*dyn_arr) + header->length*header->element_size;
    void *dest = (uint8_t*) (*dyn_arr) + index*header->element_size;

    memcpy(end, dest, header->element_size);
    if (value == NULL) {
        memset(dest, 0, header->element_size);
    } else {
        memcpy(dest, value, header->element_size);
    }

    header->length++;
}

void dyn_arr_remove_fast(void** dyn_arr, size_t index, void* output) {
    core_assert_msg(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    core_assert_msg(index < header->length, "Index out of bounds");

    void *end = (uint8_t*) (*dyn_arr) + (header->length-1)*header->element_size;
    void *dest = (uint8_t*) (*dyn_arr) + index*header->element_size;

    if (output != NULL) {
        memcpy(output, dest, header->element_size);
    }
    memcpy(dest, end, header->element_size);

    header->length--;
}

void dyn_arr_push(void** dyn_arr, const void* value) {
    core_assert_msg(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    _dyn_arr_ensure_capacity(dyn_arr, 1);
    header = _dyn_arr_to_header(*dyn_arr);

    void *end = (uint8_t*) (*dyn_arr) + header->length*header->element_size;
    memcpy(end, value, header->element_size);
    header->length++;
}

void dyn_arr_pop(void** dyn_arr, void* output) {
    core_assert_msg(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    core_assert_msg(header->length > 0, "Index out of bounds");

    if (output != NULL) {
        void *end = (uint8_t*) (*dyn_arr) + (header->length - 1)*header->element_size;
        memcpy(output, end, header->element_size);
    }
    header->length--;
}

void dyn_arr_push_arr(void** dyn_arr, const void* arr, size_t arr_length) {
    dyn_arr_insert_arr(dyn_arr, dyn_arr_length(*dyn_arr), arr, arr_length);
}

void dyn_arr_pop_arr(void** dyn_arr, size_t count, void* output) {
    core_assert(count <= dyn_arr_length(*dyn_arr));
    size_t index = dyn_arr_length(*dyn_arr) - count;
    dyn_arr_remove_arr(dyn_arr, index, count, output);
}

// =============================================================================
// HASH MAP
// =============================================================================

enum {
    _HM_SLOT_EMPTY,
    _HM_SLOT_ALIVE,
    _HM_SLOT_DEAD,
};

static inline void* _hm_get_key_ptr(const hash_map_t* map, uint32_t index) {
    return (uint8_t*) map->key_array + map->key_size*index;
}

static inline void* _hm_get_value_ptr(const hash_map_t* map, uint32_t index) {
    return (uint8_t*) map->value_array + map->value_size*index;
}

// Call when inserting a new pair into the map.
static bool _hm_resize_if_needed(hash_map_t* map) {
    if (map->count < (uint32_t) (map->capacity * map->load_factor / 100.0f)) {
        return false;
    }

    allocator_t alloc = map->allocator;

    uint32_t new_capacity = map->capacity * map->grow_factor;
    uint32_t new_count = 0;
    void* new_key_array = core_alloc(alloc, map->key_size * new_capacity);
    void* new_value_array = core_alloc(alloc, map->value_size * new_capacity);;
    uint32_t* new_hash_array = core_alloc(alloc, sizeof(uint32_t) * new_capacity);;
    uint8_t* new_state_array = core_alloc(alloc, sizeof(uint8_t) * new_capacity);;

    memset(new_key_array, 0, map->key_size * new_capacity);
    memset(new_value_array, 0, map->value_size * new_capacity);
    memset(new_hash_array, 0, sizeof(uint32_t) * new_capacity);
    memset(new_state_array, _HM_SLOT_EMPTY, sizeof(uint8_t) * new_capacity);

    for (uint32_t i = 0; i < map->capacity; i++) {
        if (map->state_array[i] != _HM_SLOT_ALIVE) {
            continue;
        }

        uint32_t hash = map->hash_array[i];
        uint32_t new_index = hash % new_capacity;

        // Insert into new SoA
        for (uint32_t j = 0; j < new_capacity; j++) {
            if (new_state_array[new_index] == _HM_SLOT_EMPTY) {
                break;
            }

            // Don't check for duplicate keys since those *shouldn't* exist.

            new_index = (new_index + 1) % new_capacity;
        }

        core_assert(new_state_array[new_index] == _HM_SLOT_EMPTY);

        void* key_ptr = (uint8_t*) new_key_array + map->key_size * new_index;
        void* value_ptr = (uint8_t*) new_value_array + map->value_size * new_index;
        memcpy(key_ptr, _hm_get_key_ptr(map, i), map->key_size);
        memcpy(value_ptr, _hm_get_value_ptr(map, i), map->value_size);
        new_hash_array[new_index] = hash;
        new_state_array[new_index] = _HM_SLOT_ALIVE;
        new_count++;
    }

    core_free(alloc, map->key_array, map->key_size * map->capacity);
    core_free(alloc, map->value_array, map->value_size * map->capacity);
    core_free(alloc, map->hash_array, sizeof(uint32_t) * map->capacity);
    core_free(alloc, map->state_array, sizeof(uint8_t) * map->capacity);

    map->capacity = new_capacity;
    map->key_array = new_key_array;
    map->value_array = new_value_array;
    map->hash_array = new_hash_array;
    map->state_array = new_state_array;
    map->count = new_count;

    return true;
}

hash_map_t hash_map_create(hash_map_desc_t desc) {
    // Set sensilbe defaults
    if (desc.initial_capacity == 0) {
        desc.initial_capacity = 8;
    }
    if (desc.load_factor == 0) {
        desc.load_factor = 80;
    }
    if (desc.grow_factor == 0.0f) {
        desc.grow_factor = 2.0f;
    }

    core_assert(desc.allocator.alloc != NULL);
    core_assert(desc.key_size != 0);
    core_assert(desc.value_size != 0);
    core_assert(desc.equal_func != NULL);
    core_assert(desc.hash_func != NULL);
    core_assert((uint32_t) (desc.initial_capacity * desc.load_factor / 100.0f) > 0);
    core_assert((uint32_t) (desc.initial_capacity * desc.grow_factor) > desc.initial_capacity);

    allocator_t alloc = desc.allocator;
    hash_map_t map = {
        .allocator = alloc,
        .key_size = desc.key_size,
        .value_size = desc.value_size,
        .key_array = core_alloc(alloc, desc.key_size * desc.initial_capacity),
        .value_array = core_alloc(alloc, desc.value_size * desc.initial_capacity),
        .hash_array = core_alloc(alloc, sizeof(uint32_t) * desc.initial_capacity),
        .state_array = core_alloc(alloc, sizeof(uint8_t) * desc.initial_capacity),
        .equal_func = desc.equal_func,
        .hash_func = desc.hash_func,
        .capacity = desc.initial_capacity,
        .count = 0,
        .load_factor = desc.load_factor,
        .grow_factor = desc.grow_factor,
    };

    memset(map.key_array, 0, map.key_size * map.capacity);
    memset(map.value_array, 0, map.value_size * map.capacity);
    memset(map.hash_array, 0, sizeof(uint32_t) * map.capacity);
    memset(map.state_array, _HM_SLOT_EMPTY, sizeof(uint8_t) * map.capacity);

    return map;
}

void hash_map_destroy(hash_map_t* map) {
    allocator_t alloc = map->allocator;
    core_free(alloc, map->key_array, map->key_size * map->capacity);
    core_free(alloc, map->value_array, map->value_size * map->capacity);
    core_free(alloc, map->hash_array, sizeof(uint32_t) * map->capacity);
    core_free(alloc, map->state_array, sizeof(uint8_t) * map->capacity);
    *map = (hash_map_t) {0};
}

bool hash_map_insert(hash_map_t* map, const void* key, const void* value) {
    uint32_t hash = map->hash_func(key, map->key_size);
    uint32_t index = hash % map->capacity;
    uint32_t i = 0;
    while (true) {
        uint8_t state = map->state_array[index];
        if (state == _HM_SLOT_EMPTY) {
            break;
        }

        // Key already exists
        if (state == _HM_SLOT_ALIVE &&
            map->hash_array[index] == hash &&
            map->equal_func(key, _hm_get_key_ptr(map, index), map->key_size)) {
            return false;
        }

        index = (index + 1) % map->capacity;

        i++;
        core_assert(i <= map->capacity);
    }

    core_assert(map->state_array[index] == _HM_SLOT_EMPTY);

    memcpy(_hm_get_key_ptr(map, index), key, map->key_size);
    memcpy(_hm_get_value_ptr(map, index), value, map->value_size);
    map->hash_array[index] = hash;
    map->state_array[index] = _HM_SLOT_ALIVE;
    map->count++;

    _hm_resize_if_needed(map);

    return true;
}

bool hash_map_set(hash_map_t* map, const void* key, const void* value, void* old_value) {
    uint32_t hash = map->hash_func(key, map->key_size);
    uint32_t index = hash % map->capacity;
    uint32_t i = 0;
    while (true) {
        uint8_t state = map->state_array[index];
        if (state == _HM_SLOT_EMPTY) {
            break;
        }

        // Key already exists
        if (state == _HM_SLOT_ALIVE &&
            map->hash_array[index] == hash &&
            map->equal_func(key, _hm_get_key_ptr(map, index), map->key_size)) {
            break;
        }

        index = (index + 1) % map->capacity;

        i++;
        core_assert(i <= map->capacity);
    }

    bool is_unique = map->state_array[index] == _HM_SLOT_EMPTY;
    if (is_unique && old_value != NULL) {
        memcpy(old_value, _hm_get_value_ptr(map, index), sizeof(map->value_size));
    }

    memcpy(_hm_get_value_ptr(map, index), value, map->value_size);
    map->state_array[index] = _HM_SLOT_ALIVE;
    if (is_unique) {
        memcpy(_hm_get_key_ptr(map, index), key, map->key_size);
        map->hash_array[index] = hash;
        map->count++;
    }

    _hm_resize_if_needed(map);

    return is_unique;
}

bool hash_map_remove(hash_map_t* map, const void* key, void* result_value) {
    uint32_t hash = map->hash_func(key, map->key_size);
    uint32_t index = hash % map->capacity;
    uint32_t i = 0;
    while (true) {
        uint8_t state = map->state_array[index];
        if (state == _HM_SLOT_EMPTY) {
            return false;
        }

        if (state == _HM_SLOT_ALIVE &&
            map->hash_array[index] == hash &&
            map->equal_func(key, _hm_get_key_ptr(map, index), map->key_size)) {
            break;
        }

        index = (index + 1) % map->capacity;

        i++;
        core_assert(i <= map->capacity);
    }

    core_assert(map->state_array[index] == _HM_SLOT_ALIVE);

    if (result_value != NULL) {
        memcpy(result_value, _hm_get_value_ptr(map, index), sizeof(map->value_size));
    }

    map->state_array[index] = _HM_SLOT_DEAD;

    return true;
}

bool hash_map_contains(const hash_map_t* map, const void* key) {
    uint32_t hash = map->hash_func(key, map->key_size);
    uint32_t index = hash % map->capacity;
    uint32_t i = 0;
    while (true) {
        uint8_t state = map->state_array[index];
        if (state == _HM_SLOT_EMPTY) {
            return false;
        }

        // Key exists
        if (state == _HM_SLOT_ALIVE &&
            map->hash_array[index] == hash &&
            map->equal_func(key, _hm_get_key_ptr(map, index), map->key_size)) {
            return true;
        }

        index = (index + 1) % map->capacity;

        i++;
        core_assert(i <= map->capacity);
    }
    core_assert_msg(false, "Unreacable %s", __func__);
    return -1;
}

bool hash_map_get(const hash_map_t* map, const void* key, void* result_value) {
    uint32_t hash = map->hash_func(key, map->key_size);
    uint32_t index = hash % map->capacity;
    uint32_t i = 0;
    while (true) {
        uint8_t state = map->state_array[index];
        if (state == _HM_SLOT_EMPTY) {
            return false;
        }

        // Key exists
        if (state == _HM_SLOT_ALIVE &&
            map->hash_array[index] == hash &&
            map->equal_func(key, _hm_get_key_ptr(map, index), map->key_size)) {
            break;
        }

        index = (index + 1) % map->capacity;

        i++;
        core_assert(i <= map->capacity);
    }

    core_assert(map->state_array[index] == _HM_SLOT_ALIVE);

    if (result_value != NULL) {
        memcpy(result_value, _hm_get_value_ptr(map, index), sizeof(map->value_size));
    }

    return true;
}

void* hash_map_get_ptr(const hash_map_t* map, const void* key) {
    uint32_t hash = map->hash_func(key, map->key_size);
    uint32_t index = hash % map->capacity;
    uint32_t i = 0;
    while (true) {
        uint8_t state = map->state_array[index];
        if (state == _HM_SLOT_EMPTY) {
            return NULL;
        }

        // Key exists
        if (state == _HM_SLOT_ALIVE &&
            map->hash_array[index] == hash &&
            map->equal_func(key, _hm_get_key_ptr(map, index), map->key_size)) {
            return _hm_get_value_ptr(map, index);
        }

        index = (index + 1) % map->capacity;

        i++;
        core_assert(i <= map->capacity);
    }
    core_assert_msg(false, "Unreacable %s", __func__);
    return NULL;
}

uint32_t _hm_generic_hash(const void* key, uint32_t size) {
    return fvn1a_hash32(0, key, size);
}

bool _hm_generic_equal(const void* lhs, const void* rhs, uint32_t size) {
    return memcmp(lhs, rhs, size) == 0;
}

#endif // CORE_IMPLEMENTATION
#endif // CORE_H
