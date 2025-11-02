#ifndef NEXUS_H
#define NEXUS_H

#include <stddef.h>
#include <stdint.h>

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
// UTILITY
// =============================================================================

#define NEXUS_UNUSED(var) (void) var

// TODO: Implement assert
#define NEXUS_ASSERT(cond)

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
extern void nexus_arena_destroy(nexus_arena_t* arena);
extern nexus_allocator_t nexus_arena_allocator(nexus_arena_t* arena);

extern void* nexus_arena_push(nexus_arena_t* arena, size_t size);
extern void* nexus_arena_push_aligned(nexus_arena_t* arena, size_t size, size_t align);
extern void nexus_arena_reset(nexus_arena_t* arena);

#endif // NEXUS_H
