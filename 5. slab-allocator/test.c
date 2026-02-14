#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "alloc.h"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

int tests_passed = 0;
int tests_total = 0;

#define TEST_ASSERT(cond, msg)                                           \
    do                                                                   \
    {                                                                    \
        tests_total++;                                                   \
        if (!(cond))                                                     \
        {                                                                \
            printf(ANSI_COLOR_RED "FAIL: %s\n" ANSI_COLOR_RESET, msg);   \
            return;                                                      \
        }                                                                \
        else                                                             \
        {                                                                \
            printf(ANSI_COLOR_GREEN "PASS: %s\n" ANSI_COLOR_RESET, msg); \
            tests_passed++;                                              \
        }                                                                \
    } while (0)

/* --- Helper: Count Slabs in a List --- */
int count_slabs(slab_t *head)
{
    int count = 0;
    while (head)
    {
        count++;
        head = head->next;
    }
    return count;
}

/* --- Helper: Visualizer --- */
void print_cache_state(kmem_cache_t *c)
{
    printf("  [Cache '%s'] Full: %d | Partial: %d | Free: %d\n",
           c->name,
           count_slabs(c->slabs_full),
           count_slabs(c->slabs_partial),
           count_slabs(c->slabs_free));
}

/* --- TEST CASES --- */

void test_initialization()
{
    printf("\n=== Test 1: Initialization ===\n");
    buddy_init(); // Initialize the underlying memory first!

    kmem_cache_t *cache = kmem_cache_create("int_cache", sizeof(int)); // 4 bytes

    TEST_ASSERT(cache != NULL, "Cache created");
    TEST_ASSERT(cache->obj_size == sizeof(int), "Object size correct");
    TEST_ASSERT(cache->slabs_partial == NULL, "Starts with 0 partial slabs");
    TEST_ASSERT(cache->slabs_full == NULL, "Starts with 0 full slabs");

    // Cleanup
    // (We leak the cache struct here for simplicity, or add a destroy later)
}

void test_single_alloc()
{
    printf("\n=== Test 2: Single Allocation ===\n");
    buddy_init();
    kmem_cache_t *cache = kmem_cache_create("node_cache", 32); // 32 bytes

    void *p = kmem_cache_alloc(cache);

    TEST_ASSERT(p != NULL, "Allocated object");
    TEST_ASSERT(count_slabs(cache->slabs_partial) == 1, "Slab created in Partial list");

    // Check internal state of that slab
    slab_t *slab = cache->slabs_partial;
    // 4096 / 32 = 128 objects per slab.
    // We used 1. Should have 127 free.
    TEST_ASSERT(slab->free_count == (cache->objects_per_slab - 1), "Free count decremented");

    // Verify Bitmap: Bit 0 should be set (1)
    TEST_ASSERT((slab->bitmap & 1) == 1, "Bitmap bit 0 set");
}

void test_slab_full_transition()
{
    printf("\n=== Test 3: Fill Slab (Partial -> Full) ===\n");
    buddy_init();
    kmem_cache_t *cache = kmem_cache_create("fill_test", 32);

    int limit = cache->objects_per_slab; // Should be 128
    void *ptrs[limit];

    // Fill it up!
    for (int i = 0; i < limit; i++)
    {
        ptrs[i] = kmem_cache_alloc(cache);
    }

    TEST_ASSERT(count_slabs(cache->slabs_partial) == 0, "Partial list empty");
    TEST_ASSERT(count_slabs(cache->slabs_full) == 1, "Slab moved to Full list");

    slab_t *slab = cache->slabs_full;
    TEST_ASSERT(slab->free_count == 0, "Slab is completely full");

    print_cache_state(cache);
}

void test_slab_growth()
{
    printf("\n=== Test 4: Cache Growth (New Page Request) ===\n");
    // Continue from previous state (Full list has 1 slab)
    // We assume test 3 ran, but let's re-init to be safe/standalone
    buddy_init();
    kmem_cache_t *cache = kmem_cache_create("growth_test", 64); // 64 bytes -> 64 objs/page

    int limit = cache->objects_per_slab;

    // Fill first slab
    for (int i = 0; i < limit; i++)
        kmem_cache_alloc(cache);

    // Request ONE more
    void *overflow = kmem_cache_alloc(cache);

    TEST_ASSERT(overflow != NULL, "Allocated overflow object");
    TEST_ASSERT(count_slabs(cache->slabs_full) == 1, "Old slab still full");
    TEST_ASSERT(count_slabs(cache->slabs_partial) == 1, "New slab created in Partial");

    print_cache_state(cache);
}

void test_free_and_reuse()
{
    printf("\n=== Test 5: Free & Reuse (Bitmap Logic) ===\n");
    buddy_init();
    kmem_cache_t *cache = kmem_cache_create("reuse_test", 128); // 128 bytes -> 32 objs/page

    void *p1 = kmem_cache_alloc(cache); // Slot 0
    void *p2 = kmem_cache_alloc(cache); // Slot 1
    void *p3 = kmem_cache_alloc(cache); // Slot 2

    // Free p2 (Slot 1)
    kmem_cache_free(cache, p2);

    slab_t *slab = cache->slabs_partial;
    TEST_ASSERT(slab->free_count == (cache->objects_per_slab - 2), "Free count correct (used 2)");
    // Bitmap check: Slot 0 (1), Slot 1 (0), Slot 2 (1) -> ...00000101 -> 5
    // Note: Depends on endianness/implementation, but bitwise check is safer:
    TEST_ASSERT(!((slab->bitmap >> 1) & 1), "Slot 1 bit cleared");
    TEST_ASSERT(((slab->bitmap >> 0) & 1), "Slot 0 bit still set");
    TEST_ASSERT(((slab->bitmap >> 2) & 1), "Slot 2 bit still set");

    // Alloc again - Should REUSE Slot 1 (First Free Bit)
    void *p4 = kmem_cache_alloc(cache);
    TEST_ASSERT(p4 == p2, "Pointer reused (LIFO/Bitmap priority)");
}

int main()
{
    printf("--- Slab Allocator Unit Tests ---\n");

    test_initialization();
    test_single_alloc();
    test_slab_full_transition();
    test_slab_growth();
    test_free_and_reuse();

    printf("\n------------------------------------------------\n");
    printf("Summary: %d / %d Tests Passed.\n", tests_passed, tests_total);
    if (tests_passed == tests_total)
    {
        printf(ANSI_COLOR_GREEN "ALL TESTS PASSED. SYSTEM STABLE.\n" ANSI_COLOR_RESET);
    }
    else
    {
        printf(ANSI_COLOR_RED "FAILURES DETECTED.\n" ANSI_COLOR_RESET);
    }
    return 0;
}