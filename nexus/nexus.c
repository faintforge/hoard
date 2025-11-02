#include "nexus.h"

nexus_arena_t nexus_arena_create(nexus_allocator_t allocator, size_t capacity) {
    return (nexus_arena_t) {
        .allocator = allocator,
        .memory = NEXUS_ALLOC(allocator, capacity),
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
    return nexus_arena_push(arena, new_size);
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

static size_t _is_power_of_two(size_t value) {
    return (value & (value - 1)) == 0;
}

static size_t _align_up(size_t value, size_t align) {
    NEXUS_ASSERT(_is_power_of_two(align));
    size_t mod = value % (align - 1);
    if (mod != 0) {
        value += align - mod;
    }
    return value;
}

void* nexus_arena_push_aligned(nexus_arena_t* arena, size_t size, size_t align) {
    size_t aligned_size = _align_up(size, align);
    if (arena->position + aligned_size > arena->capacity) {
        return NULL;
    }
    arena->last_position = arena->position;
    arena->position += aligned_size;
    return arena->memory + arena->last_position;
}

void* nexus_arena_push(nexus_arena_t* arena, size_t size) {
    return nexus_arena_push_aligned(arena, size, sizeof(void*));
}

void nexus_arena_reset(nexus_arena_t* arena) {
    arena->position = 0;
    arena->last_position = 0;
}
