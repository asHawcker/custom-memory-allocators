# Systems Programming: Custom Memory Allocator Lab

This repository documents the step-by-step implementation of a custom dynamic memory allocator (`malloc`, `free`, `realloc`) in C. The project evolves from a naive implicit list design to a production-grade explicit free list with smart coalescing and in-place reallocation.

**Goal:** To understand heap layout, fragmentation, alignment, and the mechanics of low-level systems programming.

---

## Architecture 1: The Implicit Free List (Naive)

The starting point. We view the heap as a contiguous sequence of blocks. We do not track free blocks separately; to find space, we must scan the entire heap.

### Key Concepts

- **Block Format:** `[ Header (Size/Alloc) | Payload ]`
- **Header/Footer:** 4 bytes each. Stores block size and allocated bit (packed).
- **Search Algorithm:** First Fit (Linear Scan). We iterate from the start of the heap until we find a block `size >= requested_size`.

### Pros & Cons

- **Simple:** Easy to implement and debug.
- **Low Overhead:** Only 8 bytes overhead per block.

- **Slow Allocation:** $O(N)$ where $N$ is total blocks. As heap fills, malloc becomes incredibly slow.
- **No Splitting (Naive):** The naive implementation wastes memory by returning the entire free block even if the user asked for a tiny slice.

---

## Architecture 2: The Explicit Free List

The performance upgrade. We treat free blocks as nodes in a Doubly Linked List. Allocated blocks remain untouched (no pointers inside them).

The production-grade refinement. Focuses on minimizing fragmentation and maximizing speed through clever reuse of memory.

### Key Concepts

- **Payload Overlay:** Free blocks store `next` and `prev` pointers _inside_ the empty payload area.
  - `[ Header | PREV_PTR | NEXT_PTR | ... | Footer ]`
- **List Policy:** **LIFO (Last-In, First-Out)**. Newly freed blocks are inserted at the root of the list.
- **Search Algorithm:** First Fit on the _Free List_. We only scan free blocks.

### Pros & Cons

- **Fast Allocation:** $O(F)$ where $F$ is number of free blocks. Much faster than implicit.
- **Splitting:** Implemented block splitting to reduce internal fragmentation.

- **Complexity:** Managing list pointers during splitting and coalescing is error-prone.
- **Min Block Size:** Blocks must be at least 32 bytes (16 overhead + 16 pointers) to be freeable.

---

### Features

1.  **Smart Coalescing:**
    - Uses boundary tags to merge physically adjacent free blocks in $O(1)$.
    - Optimized list updates: When merging, we often update the neighbor's size in-place rather than deleting and re-inserting, saving pointer operations.

2.  **In-Place Reallocation (`realloc`):**
    - Standard `realloc` copies data (slow).
    - **Smart Logic:** If `realloc` asks to expand, we check the _next physical block_. If it is free and large enough, we "eat" it, merge the space, and return the _same pointer_. Zero copy.

3.  **Aggressive Splitting:**
    - **On Malloc:** If we find a 1MB block for a 100B request, we split it and return the remainder to the free list.
    - **On Realloc:** If shrinking a block, we slice off the unused tail and free it immediately.

---

## API Reference

| Function                | Description                                                          | complexity       |
| :---------------------- | :------------------------------------------------------------------- | :--------------- |
| `mminit()`              | Initializes the heap (Prologue/Epilogue/Alignment). Must call first. | $O(1)$           |
| `my_malloc(size)`       | Allocates `size` bytes. Returns 16-byte aligned pointer.             | $O(F)$           |
| `my_free(ptr)`          | Frees memory and coalesces with neighbors.                           | $O(1)$           |
| `my_realloc(ptr, size)` | Resizes block. Tries to expand in-place or shrink-split.             | $O(1)$ or $O(F)$ |

---

## Testing & Verification

The project includes a robust test driver `test.c`.
