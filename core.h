#ifndef CORE_H
#define CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

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
        return NULL;
    }
    arena->last_position = position;
    arena->position = position + size;
    return &arena->memory[position];
}

void* arena_push(arena_t* arena, size_t size) {
    return arena_push_aligned(arena, size, sizeof(void*));
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

#endif // CORE_IMPLEMENTATION
#endif // CORE_H
