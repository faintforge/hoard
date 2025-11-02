#ifndef NEXUS_H
#define NEXUS_H

#include <stddef.h>

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

#endif // NEXUS_H
