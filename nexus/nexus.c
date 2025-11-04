#include "nexus.h"

// TODO: Remove this CRT dependency
#include <string.h>

// =============================================================================
// LOGGER
// =============================================================================

#define MAX_LOGGER_CALLBACK_COUNT 16

typedef struct _logger_callback_t _logger_callback_t;
struct _logger_callback_t {
    nexus_logger_callback_func_t func;
    void* userdata;
};

static _logger_callback_t g_logger_callbacks[MAX_LOGGER_CALLBACK_COUNT] = {0};
static uint32_t g_logger_callback_count = 0;

void nexus_logger_register_callback(nexus_logger_callback_func_t func, void* userdata) {
    NEXUS_ASSERT_MSG(g_logger_callback_count < MAX_LOGGER_CALLBACK_COUNT, "Maximum amount of logger callbacks of %d has been reached.", MAX_LOGGER_CALLBACK_COUNT);
    g_logger_callbacks[g_logger_callback_count] = (_logger_callback_t) {
        .func = func,
        .userdata = userdata,
    };
    g_logger_callback_count++;
}

void _nexus_log(nexus_log_level_t level, const char* file, int32_t line, const char* message, ...) {
    for (uint32_t i = 0; i < g_logger_callback_count; i++) {
        nexus_log_event_t event = {
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

nexus_arena_t nexus_arena_create(nexus_allocator_t allocator, size_t capacity) {
    return (nexus_arena_t) {
        .allocator = allocator,
        .memory = NEXUS_ALLOC(allocator, capacity),
        .capacity = capacity,
        .position = 0,
        .last_position = 0,
    };
}

nexus_arena_t nexus_arena_create_from_buffer(uint8_t* buffer, size_t capacity) {
    return (nexus_arena_t) {
        .allocator = {0},
        .memory = buffer,
        .capacity = capacity,
        .position = 0,
        .last_position = 0,
    };
}

void nexus_arena_destroy(nexus_arena_t* arena) {
    NEXUS_FREE(arena->allocator, arena->memory, arena->capacity);
    *arena = (nexus_arena_t) {0};
}

void* _arena_alloc(size_t size, void* context) {
    nexus_arena_t* arena = context;
    return nexus_arena_push(arena, size);
}

void* _arena_realloc(void* ptr, size_t old_size, size_t new_size, void* context) {
    if (old_size >= new_size) {
        return ptr;
    }
    nexus_arena_t* arena = context;
    // Resize in place if we're reallocing the last allocation.
    if ((uintptr_t) arena->memory + arena->last_position == (uintptr_t) ptr) {
        arena->position = arena->last_position;
    }
    void* new_ptr = nexus_arena_push(arena, new_size);
    memcpy(new_ptr, ptr, old_size);
    return new_ptr;
}

void _arena_free(void* ptr, size_t size, void* context) {
    NEXUS_UNUSED(ptr);
    NEXUS_UNUSED(size);
    NEXUS_UNUSED(context);
}

nexus_allocator_t nexus_arena_allocator(nexus_arena_t* arena) {
    return (nexus_allocator_t) {
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
    NEXUS_ASSERT(_is_power_of_two(align));
    size_t mod = value & (align - 1);
    if (mod != 0) {
        value += align - mod;
    }
    return value;
}

void* nexus_arena_push_aligned(nexus_arena_t* arena, size_t size, size_t align) {
    uintptr_t current_ptr = (uintptr_t) arena->memory + arena->last_position;
    uintptr_t aligned_ptr = _align_up(current_ptr, align);
    uintptr_t position = aligned_ptr - (uintptr_t) arena->memory;
    if (position + size > arena->capacity) {
        return NULL;
    }
    arena->last_position = position;
    arena->position = position + size;
    return &arena->memory[position];
}

void* nexus_arena_push(nexus_arena_t* arena, size_t size) {
    return nexus_arena_push_aligned(arena, size, sizeof(void*));
}

void nexus_arena_reset(nexus_arena_t* arena) {
    arena->position = 0;
    arena->last_position = 0;
}

nexus_arena_temp_t nexus_arena_temp_begin(nexus_arena_t* arena) {
    return (nexus_arena_temp_t) {
        .arena = arena,
        .position = arena->position,
        .last_position = arena->last_position,
    };
}

void nexus_arena_temp_end(nexus_arena_temp_t* temp) {
    nexus_arena_t* arena = temp->arena;
    arena->position = temp->position;
    arena->last_position = temp->last_position;
    *temp = (nexus_arena_temp_t) {0};
}

// =============================================================================
// DYNAMIC ARRAY
// =============================================================================

#define _DYN_ARR_INITIAL_SIZE 8

typedef struct _dyn_arr_header_t _dyn_arr_header_t;
struct _dyn_arr_header_t {
    nexus_allocator_t allocator;
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

void* nexus_dyn_arr_create(nexus_allocator_t allocator, size_t element_size) {
    _dyn_arr_header_t* header = NEXUS_ALLOC(allocator, sizeof(_dyn_arr_header_t) + element_size * _DYN_ARR_INITIAL_SIZE);
    *header = (_dyn_arr_header_t) {
        .allocator = allocator,
        .element_size = element_size,
        .capacity = _DYN_ARR_INITIAL_SIZE,
        .length = 0,
    };
    return _header_to_dyn_arr(header);
}

void nexus_dyn_arr_destroy(void** dyn_arr) {
    NEXUS_ASSERT_MSG(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    NEXUS_FREE(header->allocator, header, sizeof(_dyn_arr_header_t) + header->element_size * _DYN_ARR_INITIAL_SIZE);
    *dyn_arr = NULL;
}

size_t nexus_dyn_arr_length(const void* dyn_arr) {
    return _dyn_arr_to_header(dyn_arr)->length;
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
    NEXUS_REALLOC(header->allocator,
            header,
            sizeof(_dyn_arr_header_t) + prev_capacity * header->element_size,
            sizeof(_dyn_arr_header_t) + new_capacity * header->element_size);
    *dyn_arr = _header_to_dyn_arr(header);
}

void nexus_dyn_arr_insert_arr(void** dyn_arr, size_t index, const void* arr, size_t arr_length) {
    NEXUS_ASSERT_MSG(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    NEXUS_ASSERT_MSG(index <= header->length, "Index out of bounds");
    _dyn_arr_ensure_capacity(dyn_arr, arr_length);

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

void nexus_dyn_arr_remove_arr(void** dyn_arr, size_t index, size_t count, void* output) {
    NEXUS_ASSERT_MSG(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    NEXUS_ASSERT_MSG(index < header->length, "Index out of bounds");
    NEXUS_ASSERT_MSG(index+count <= header->length, "Index out of bounds");

    void *end = (uint8_t*) (*dyn_arr) + (index+count)*header->element_size;
    void *dest = (uint8_t*) (*dyn_arr) + index*header->element_size;

    if (output != NULL) {
        memcpy(output, dest, count*header->element_size);
    }
    memmove(dest, end, (header->length - index - count)*header->element_size);

    header->length -= count;
}

void nexus_dyn_arr_insert(void** dyn_arr, size_t index, const void* value) {
    nexus_dyn_arr_insert_arr(dyn_arr, index, value, 1);
}

void nexus_dyn_arr_remove(void** dyn_arr, size_t index, void* output) {
    nexus_dyn_arr_remove_arr(dyn_arr, index, 1, output); \
}

void nexus_dyn_arr_insert_fast(void** dyn_arr, size_t index, const void* value) {
    NEXUS_ASSERT_MSG(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    NEXUS_ASSERT_MSG(index <= header->length, "Index out of bounds");
    _dyn_arr_ensure_capacity(dyn_arr, 1);

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

void nexus_dyn_arr_remove_fast(void** dyn_arr, size_t index, void* output) {
    NEXUS_ASSERT_MSG(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    NEXUS_ASSERT_MSG(index < header->length, "Index out of bounds");

    void *end = (uint8_t*) (*dyn_arr) + (header->length-1)*header->element_size;
    void *dest = (uint8_t*) (*dyn_arr) + index*header->element_size;

    if (output != NULL) {
        memcpy(output, dest, header->element_size);
    }
    memcpy(dest, end, header->element_size);

    header->length--;
}

void nexus_dyn_arr_push(void** dyn_arr, const void* value) {
    NEXUS_ASSERT_MSG(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    _dyn_arr_ensure_capacity(dyn_arr, 1);

    void *end = (uint8_t*) (*dyn_arr) + header->length*header->element_size;
    memcpy(end, value, header->element_size);
    header->length++;
}

void nexus_dyn_arr_pop(void** dyn_arr, void* output) {
    NEXUS_ASSERT_MSG(*dyn_arr != NULL, "Null pointer dereference");
    _dyn_arr_header_t* header = _dyn_arr_to_header(*dyn_arr);
    NEXUS_ASSERT_MSG(header->length > 0, "Index out of bounds");

    if (output != NULL) {
        void *end = (uint8_t*) (*dyn_arr) + header->length*header->element_size;
        memcpy(output, end, header->element_size);
    }
    header->length++;
}
