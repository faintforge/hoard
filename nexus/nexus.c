#include "nexus.h"

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
