#ifndef NEXUS_H
#define NEXUS_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

// =============================================================================
// ALLOCATOR INTERFACE
// =============================================================================

typedef void* (*nexus_alloc_func_t)(size_t size, void* context);
typedef void* (*nexus_realloc_func_t)(void* ptr, size_t old_size, size_t new_size, void* context);
typedef void (*nexus_free_func_t)(void* ptr, size_t size, void* context);

typedef struct nexus_allocator_t nexus_allocator_t;
struct nexus_allocator_t {
    nexus_alloc_func_t alloc;
    nexus_realloc_func_t realloc;
    nexus_free_func_t free;
    void* context;
};

#define NEXUS_ALLOC(allocator, size) (allocator).alloc((size), (allocator).context)
#define NEXUS_REALLOC(allocator, ptr, old_size, new_size) (allocator).realloc((ptr), (old_size), (new_size), (allocator).context)
#define NEXUS_FREE(allocator, ptr, size) (allocator).free((ptr), (size), (allocator).context)

// =============================================================================
// LOGGING
// =============================================================================

typedef enum nexus_log_level_t {
    NEXUS_LOG_LEVEL_FATAL,
    NEXUS_LOG_LEVEL_ERROR,
    NEXUS_LOG_LEVEL_WARN,
    NEXUS_LOG_LEVEL_INFO,
    NEXUS_LOG_LEVEL_DEBUG,
    NEXUS_LOG_LEVEL_TRACE,

    _NEXUS_LOG_LEVEL_COUNT,
} nexus_log_level_t;

typedef struct nexus_log_event_t nexus_log_event_t;
struct nexus_log_event_t {
    nexus_log_level_t level;
    const char* file;
    int32_t line;
    const char* message;
    va_list args;
};

typedef void (*nexus_logger_callback_func_t)(nexus_log_event_t event, void* userdata);

extern void nexus_logger_register_callback(nexus_logger_callback_func_t func, void* userdata);

#define NEXUS_FATAL(...) _nexus_log(NEXUS_LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#define NEXUS_ERROR(...) _nexus_log(NEXUS_LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define NEXUS_WARN(...) _nexus_log(NEXUS_LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define NEXUS_INFO(...) _nexus_log(NEXUS_LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define NEXUS_DEBUG(...) _nexus_log(NEXUS_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define NEXUS_TRACE(...) _nexus_log(NEXUS_LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)

extern void _nexus_log(nexus_log_level_t level, const char* file, int32_t line, const char* message, ...);

// =============================================================================
// UTILITY
// =============================================================================

#define NEXUS_UNUSED(var) (void) var

#if defined(__GNUC__)
#define NEXUS_DEBUG_BREAK() __builtin_trap()
#elif defined(_MSC_VER)
#define NEXUS_DEBUG_BREAK() __debugbreak()
#else
#define NEXUS_DEBUG_BREAK() (*(volatile int32_t*)0)
#endif

#define NEXUS_ASSERT(expr) \
    if (expr) { \
    } else { \
        NEXUS_FATAL("Assertion Failed: %s", #expr); \
        NEXUS_DEBUG_BREAK(); \
    }

#define NEXUS_ASSERT_MSG(expr, ...) \
    if (expr) { \
    } else { \
        NEXUS_FATAL("Assertion Failed: %s", #expr); \
        NEXUS_FATAL(__VA_ARGS__); \
        NEXUS_DEBUG_BREAK(); \
    }

// =============================================================================
// ARENA ALLOCATOR
// =============================================================================

typedef struct nexus_arena_t nexus_arena_t;
struct nexus_arena_t {
    nexus_allocator_t allocator;
    uint8_t* memory;
    size_t capacity;
    size_t position;
    size_t last_position;
};

extern nexus_arena_t nexus_arena_create(nexus_allocator_t allocator, size_t capacity);
extern nexus_arena_t nexus_arena_create_from_buffer(uint8_t* buffer, size_t capacity);
extern void nexus_arena_destroy(nexus_arena_t* arena);
extern nexus_allocator_t nexus_arena_allocator(nexus_arena_t* arena);

extern void* nexus_arena_push(nexus_arena_t* arena, size_t size);
extern void* nexus_arena_push_aligned(nexus_arena_t* arena, size_t size, size_t align);
extern void nexus_arena_reset(nexus_arena_t* arena);

#endif // NEXUS_H
