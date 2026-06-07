/*
 * Copyright (c) 2025 Rizki <rizkirr.xyz@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LT_ARENA_H
#define LT_ARENA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(__cplusplus) ||                                                    \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)
#define LT_ARENA_NULLPTR nullptr
#else
#define LT_ARENA_NULLPTR NULL
#endif

#ifdef _WIN32
#include <windows.h>

#define LT_ARENA_MALLOC(size) HeapAlloc(GetProcessHeap(), 0, (size))
#define LT_ARENA_CALLOC(count, size)                                           \
  HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (count) * (size))
#define LT_ARENA_REALLOC(ptr, size)                                            \
  ((ptr) ? HeapReAlloc(GetProcessHeap(), 0, (ptr), (size))                     \
         : HeapAlloc(GetProcessHeap(), 0, (size)))
#define LT_ARENA_FREE(ptr)                                                     \
  do {                                                                         \
    if (ptr)                                                                   \
      HeapFree(GetProcessHeap(), 0, (ptr));                                    \
  } while (0)

#else /* Linux / macOS / POSIX */
#include <stdlib.h>
#define LT_ARENA_MALLOC(size) malloc(size)
#define LT_ARENA_CALLOC(count, size) calloc(count, size)
#define LT_ARENA_REALLOC(ptr, size) realloc(ptr, size)
#define LT_ARENA_FREE(ptr) free(ptr)

#endif

/**
 * @brief Get alignment of a type in a portable way.
 *
 * This macro expands to `alignof(type)` when compiling under C11 or newer, and
 * otherwise computes alignment using struct offset hack. This ensures the arena
 * allocator can correctly align memory on all compilers.
 *
 * @param type  Any C type whose alignment is needed.
 */
#if __STDC_VERSION__ >= 201112L
#include <stdalign.h>
#define LT_ARENA_ALIGNOF(type) alignof(type)
#else
#define LT_ARENA_ALIGNOF(type)                                                 \
  offsetof(                                                                    \
      struct {                                                                 \
        char c;                                                                \
        type d;                                                                \
      },                                                                       \
      d)
#endif

// -----------------------------------------------------------------------------
// PUBLIC API (opaque handles)
// -----------------------------------------------------------------------------

/**
 * @brief Opaque handle for an arena allocator.
 *
 * The internal structure is hidden from users unless
 * `LT_ARENA_IMPLEMENTATION` is defined. The arena manages memory using
 * fixed-size blocks and fast bump-pointer allocation.
 */
struct lt_arena;

/**
 * @brief Checkpoint structure for saving/restoring arena state.
 *
 * Represents a specific point in the arena's allocation history.
 * Can be used to restore the arena to a previous state, effectively
 * freeing all allocations made after the checkpoint while keeping
 * allocations made before it.
 */
struct lt_arena_checkpoint {
  struct lt_arena_block *block; // Block pointer at checkpoint
  size_t index;                 // Index within block at checkpoint
};

/**
 * @brief Create a new arena allocator.
 *
 * This allocates an `lt_arena` structure but does **not** allocate any memory
 * blocks yet. Blocks are lazily allocated on the first call to
 * `lt_arena_alloc()`.
 *
 * @param default_block_size  The size (in bytes) of each allocated block.
 *                            Larger allocations will allocate a block sized
 *                            exactly large enough for the request.
 *
 * @return Pointer to a newly initialized arena, or nullptr if allocation
 * fails.
 */
struct lt_arena *lt_arena_create(size_t default_block_size);

/**
 * @brief Allocate memory from the arena with a specific alignment.
 *
 * The arena grows by allocating new blocks when needed. Allocations never
 * return memory to the system until `lt_arena_free()` is called.
 *
 * @param arena      Pointer to a valid arena instance.
 * @param size       Number of bytes to allocate.
 * @param alignment  Alignment requirement (must be power of two).
 *
 * @return Pointer to allocated memory, or nullptr on failure.
 */
void *lt_arena_alloc(struct lt_arena *arena, size_t size, size_t alignment);

/**
 * @brief Reset the arena state for reuse.
 *
 * All blocks remain allocated, but their internal `index` pointers are reset
 * to zero. This effectively frees all previously allocated memory but retains
 * the capacity.
 *
 * Behavior note:
 * - The implementation resets **all blocks**.
 * - A different design may free all but the first block.
 *
 * @param arena  Pointer to an arena instance.
 */
void lt_arena_reset(struct lt_arena *arena);

/**
 * @brief Release all memory owned by the arena.
 *
 * This frees all blocks and the arena structure itself. After this call,
 * the arena pointer must not be used.
 *
 * @param arena  Pointer to an arena instance.
 */
void lt_arena_free(struct lt_arena *arena);

/**
 * @brief Save current arena state as a checkpoint.
 *
 * Returns a checkpoint representing the current allocation position.
 * Allocations made after this point can be freed by restoring to this
 * checkpoint using lt_arena_restore(), while allocations made before remain
 * intact.
 *
 * Supports nested checkpoints - multiple checkpoints can be saved and
 * restored independently.
 *
 * @param arena Pointer to arena instance
 * @return Checkpoint representing current state
 *
 * @example Basic usage:
 *   struct lt_arena *arena = lt_arena_create(4096);
 *   void *persistent = lt_arena_alloc(arena, 1024, 8);
 *
 *   struct lt_arena_checkpoint cp = lt_arena_checkpoint(arena);
 *
 *   for (int i = 0; i < 1000; i++) {
 *       void *temp = lt_arena_alloc(arena, 512, 8);
 *       // Use temp...
 *       lt_arena_restore(arena, cp);  // Free temp, keep persistent
 *   }
 */
struct lt_arena_checkpoint lt_arena_checkpoint(struct lt_arena *arena);

/**
 * @brief Restore arena to a previous checkpoint.
 *
 * Resets the arena's allocation position to the saved checkpoint state.
 * All allocations made after the checkpoint are effectively freed
 * (their memory becomes available for reuse).
 *
 * IMPORTANT:
 * - The checkpoint must be valid (from the same arena)
 * - Using a checkpoint after lt_arena_reset() or lt_arena_free() is undefined
 * behavior
 * - Debug builds include validation checks via assertions
 *
 * @param arena Pointer to arena instance
 * @param checkpoint Previously saved checkpoint from lt_arena_checkpoint()
 */
void lt_arena_restore(struct lt_arena *arena,
                      struct lt_arena_checkpoint checkpoint);

// -----------------------------------------------------------------------------
// IMPLEMENTATION
// -----------------------------------------------------------------------------
#ifdef LT_ARENA_IMPLEMENTATION

/**
 * @brief Internal structure representing a memory block.
 *
 * Each block contains:
 *   - `next` pointer (linked list)
 *   - `capacity` total size of the block
 *   - `index` current write position
 *   - `data[]` flexible array member (actual memory region)
 */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200) /* C99 flexible array member */
#endif
struct lt_arena_block {
  struct lt_arena_block *next;
  size_t capacity;
  size_t index;
  uint8_t data[];
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

/**
 * @brief Internal arena structure.
 *
 * Fields:
 *   - `head`    → first allocated block
 *   - `current` → block currently accepting allocations
 *   - `default_block_size` → minimum block size
 */
struct lt_arena {
  struct lt_arena_block *head;
  struct lt_arena_block *current;
  size_t default_block_size;
};
/**
 * @brief Compute padding needed to align a pointer.
 *
 * This uses a bitmask trick (requires alignment to be power of two):
 *
 *   padding = (-ptr) & (alignment - 1)
 *
 * This ensures:
 *   - If pointer is already aligned → padding = 0
 *   - Otherwise → padding = minimal offset to align
 *
 * @param ptr        Pointer value as integer.
 * @param alignment  Required alignment (must be power of two).
 *
 * @return Number of bytes of padding needed.
 */
static size_t lt__arena_align_up(uintptr_t ptr, size_t alignment) {
  return ((size_t)0 - (size_t)ptr) & (alignment - 1);
}

static bool lt__arena_add_overflow(size_t a, size_t b, size_t *out) {
  if (a > SIZE_MAX - b)
    return true;
  *out = a + b;
  return false;
}

static bool lt__arena_mul_overflow(size_t a, size_t b, size_t *out) {
  if (a != 0 && b > SIZE_MAX / a)
    return true;
  *out = a * b;
  return false;
}

static bool lt__arena_min_needed(size_t size, size_t alignment, size_t *out) {
  return !lt__arena_add_overflow(size, alignment - 1, out);
}

static struct lt_arena_block *lt__arena_block_create(size_t capacity) {
  size_t total_size = 0;
  if (lt__arena_add_overflow(sizeof(struct lt_arena_block), capacity,
                             &total_size))
    return LT_ARENA_NULLPTR;

  struct lt_arena_block *block =
      (struct lt_arena_block *)LT_ARENA_MALLOC(total_size);
  if (!block)
    return LT_ARENA_NULLPTR;

  block->next = LT_ARENA_NULLPTR;
  block->capacity = capacity;
  block->index = 0;
  return block;
}

struct lt_arena *lt_arena_create(size_t default_block_size) {
  if (default_block_size == 0)
    return LT_ARENA_NULLPTR;

  struct lt_arena *arena =
      (struct lt_arena *)LT_ARENA_CALLOC(1, sizeof(struct lt_arena));
  if (!arena)
    return LT_ARENA_NULLPTR;

  arena->default_block_size = default_block_size;
  return arena;
}

void *lt_arena_alloc(struct lt_arena *arena, size_t size, size_t alignment) {
  if (!arena || size == 0 || alignment == 0)
    return LT_ARENA_NULLPTR;

  // Ensure alignment is power of two.
  if (alignment & (alignment - 1))
    return LT_ARENA_NULLPTR;

  size_t min_needed = 0;
  if (!lt__arena_min_needed(size, alignment, &min_needed))
    return LT_ARENA_NULLPTR;

  // Lazily allocate first block.
  if (!arena->current) {
    size_t block_size = (min_needed > arena->default_block_size)
                            ? min_needed
                            : arena->default_block_size;
    struct lt_arena_block *block = lt__arena_block_create(block_size);
    if (!block)
      return LT_ARENA_NULLPTR;
    arena->head = arena->current = block;
  }

  for (;;) {
    // Compute padding for alignment in the current block.
    uintptr_t current_ptr =
        (uintptr_t)(arena->current->data + arena->current->index);
    size_t padding = lt__arena_align_up(current_ptr, alignment);

    size_t used = 0;
    if (lt__arena_add_overflow(arena->current->index, padding, &used) ||
        lt__arena_add_overflow(used, size, &used))
      return LT_ARENA_NULLPTR;

    if (used <= arena->current->capacity) {
      arena->current->index += padding;
      void *ptr = arena->current->data + arena->current->index;
      arena->current->index += size;
      return ptr;
    }

    // Reuse existing next block (important after lt_arena_reset()).
    if (arena->current->next) {
      arena->current = arena->current->next;
      continue;
    }

    size_t next_capacity = arena->current->capacity;
    if (next_capacity < arena->default_block_size)
      next_capacity = arena->default_block_size;
    size_t doubled = 0;
    if (!lt__arena_mul_overflow(next_capacity, (size_t)2, &doubled) &&
        doubled > next_capacity)
      next_capacity = doubled;
    if (next_capacity < min_needed)
      next_capacity = min_needed;

    struct lt_arena_block *new_block = lt__arena_block_create(next_capacity);
    if (!new_block)
      return LT_ARENA_NULLPTR;

    arena->current->next = new_block;
    arena->current = new_block;
  }
}

void lt_arena_reset(struct lt_arena *arena) {
  if (!arena)
    return;

  struct lt_arena_block *block = arena->head;
  while (block) {
    block->index = 0;
    block = block->next;
  }
  arena->current = arena->head;
}

void lt_arena_free(struct lt_arena *arena) {
  if (!arena)
    return;

  struct lt_arena_block *block = arena->head;
  while (block) {
    struct lt_arena_block *next = block->next;
    LT_ARENA_FREE(block);
    block = next;
  }
  LT_ARENA_FREE(arena);
}

struct lt_arena_checkpoint lt_arena_checkpoint(struct lt_arena *arena) {
  assert(arena != LT_ARENA_NULLPTR && "lt_arena_checkpoint: arena is NULL");

  struct lt_arena_checkpoint cp = {0};
  if (!arena->current) {
    // Arena not yet allocated - return zero checkpoint (valid for initial
    // state)
    return cp;
  }

  cp.block = arena->current;
  cp.index = arena->current->index;
  return cp;
}

void lt_arena_restore(struct lt_arena *arena,
                      struct lt_arena_checkpoint checkpoint) {
  assert(arena != LT_ARENA_NULLPTR && "lt_arena_restore: arena is NULL");

  // Restore to initial empty state (checkpoint taken before first allocation).
  if (checkpoint.block == LT_ARENA_NULLPTR) {
    assert(checkpoint.index == 0 &&
           "lt_arena_restore: invalid empty-state checkpoint index");
    struct lt_arena_block *block = arena->head;
    while (block) {
      struct lt_arena_block *next = block->next;
      LT_ARENA_FREE(block);
      block = next;
    }
    arena->head = LT_ARENA_NULLPTR;
    arena->current = LT_ARENA_NULLPTR;
    return;
  }

// Debug validation: ensure checkpoint belongs to this arena
#ifndef NDEBUG
  struct lt_arena_block *block = arena->head;
  bool found = false;
  while (block) {
    if (block == checkpoint.block) {
      found = true;
      break;
    }
    block = block->next;
  }
  assert(found && "lt_arena_restore: checkpoint does not belong to this arena");
  assert(checkpoint.index <= checkpoint.block->capacity &&
         "lt_arena_restore: checkpoint index is invalid");
#endif

  // Free blocks allocated after checkpoint to avoid memory leak
  struct lt_arena_block *orphan = checkpoint.block->next;
  while (orphan) {
    struct lt_arena_block *next = orphan->next;
    LT_ARENA_FREE(orphan);
    orphan = next;
  }
  checkpoint.block->next = LT_ARENA_NULLPTR;

  // Reset current block to checkpoint position
  checkpoint.block->index = checkpoint.index;
  arena->current = checkpoint.block;
}

#endif // LT_ARENA_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // LT_ARENA_H
