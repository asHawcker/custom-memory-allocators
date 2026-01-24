#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* --- WHITE BOX TESTING --- */
// Include the source directly to access static variables (heap_list_p, free_list_p)
// and internal macros (HDRP, NXT_BLOCK, etc.)
#include "alloc.c"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

int tests_passed = 0;
int tests_total = 0;

/* --- TEST MACROS --- */
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

/* --- INTEGRITY CHECKER --- */
// Validates the Linked List Structure
int check_list_integrity()
{
    char *bp = free_list_p;
    char *prev = NULL;
    int limit = 0;

    // 1. Check Root
    if (free_list_p != NULL && (GET_ALLOC(HDRP(free_list_p)) == 1))
    {
        printf("ERROR: Root of free list is marked ALLOCATED!\n");
        return 0;
    }

    while (bp != NULL)
    {
        limit++;
        if (limit > 10000)
        {
            printf("ERROR: Infinite loop in free list.\n");
            return 0;
        }

        // 2. Check Allocation Status
        if (GET_ALLOC(HDRP(bp)) != 0)
        {
            printf("ERROR: Block %p in free list is ALLOCATED.\n", bp);
            return 0;
        }

        // 3. Check Back-Pointers (Doubly Linked)
        if (GET_PRV_PTR(bp) != prev)
        {
            printf("ERROR: Broken Back-Link at %p. Expected %p, Got %p\n", bp, prev, GET_PRV_PTR(bp));
            return 0;
        }

        prev = bp;
        bp = GET_NXT_PTR(bp);
    }
    return 1;
}

/* --- SECTION 1: BASIC FUNCTIONALITY --- */

void test_initialization()
{
    printf("\n=== Test 1: Initialization ===\n");
    heap_list_p = 0;
    free_list_p = 0;
    mminit();
    TEST_ASSERT(heap_list_p != NULL, "Heap initialized");
    TEST_ASSERT(free_list_p != NULL, "Free list created");
    TEST_ASSERT(check_list_integrity(), "List integrity check");
}

void test_basic_malloc()
{
    printf("\n=== Test 2: Basic Malloc & Alignment ===\n");
    mminit();

    // Alloc 1 byte -> Should verify alignment
    char *p1 = my_malloc(1);
    TEST_ASSERT(p1 != NULL, "Malloc returned pointer");
    TEST_ASSERT((uintptr_t)p1 % 16 == 0, "Pointer is 16-byte aligned");

    size_t size = GET_SIZE(HDRP(p1));
    TEST_ASSERT(size >= 32, "Block size meets minimum (32 bytes)");

    // Verify Write
    *p1 = 'X';
    TEST_ASSERT(*p1 == 'X', "Memory is writable");

    my_free(p1);
    TEST_ASSERT(GET_ALLOC(HDRP(p1)) == 0, "Block marked free after free()");
    TEST_ASSERT(check_list_integrity(), "List integrity check");
}

/* --- SECTION 2: EXPLICIT LIST BEHAVIOR --- */

void test_lifo_policy()
{
    printf("\n=== Test 3: LIFO Policy (Last-In, First-Out) ===\n");
    mminit();

    // Allocate to clear initial big chunk
    void *junk = my_malloc(CHUNKSIZE - 128);

    char *a = my_malloc(64);
    char *b = my_malloc(64);

    // Free A -> A is Root
    my_free(a);
    TEST_ASSERT(free_list_p == a, "Freed A -> A is root");

    TEST_ASSERT(check_list_integrity(), "List integrity check");
}

void test_complex_coalescing()
{
    printf("\n=== Test 4: Coalescing (Left-Middle-Right) ===\n");
    mminit();

    // Create 3 chunks
    char *left = my_malloc(64);
    char *middle = my_malloc(64);
    char *right = my_malloc(64);

    // Free Left and Right
    my_free(left);
    my_free(right);

    // Free Middle (Triggers Case 4: Merge both sides)
    my_free(middle);

    // Check Result: Should be one massive block starting at Left
    TEST_ASSERT(free_list_p == left, "Merged block starts at Left");

    // Size check: 3 blocks * (64 payload + 16 overhead = 80) = 240 bytes
    // (Note: actual size might vary slightly due to alignment of the first chunk)
    size_t size = GET_SIZE(HDRP(left));
    TEST_ASSERT(size >= 240, "Size is sum of all blocks");

    TEST_ASSERT(check_list_integrity(), "List integrity check");
}

/* --- SECTION 3: SMART REALLOC (OPTIMIZED) --- */

void test_realloc_shrink_split()
{
    printf("\n=== Test 5: Realloc Shrink (Splitting) ===\n");
    mminit();

    // Alloc large block
    char *p = my_malloc(200);
    size_t old_size = GET_SIZE(HDRP(p));

    // Shrink drastically
    char *new_p = my_realloc(p, 32);

    TEST_ASSERT(new_p == p, "Pointer unchanged (In-Place)");
    TEST_ASSERT(GET_SIZE(HDRP(new_p)) < old_size, "Block size reduced");

    // Check Remainder
    char *remainder = NXT_BLOCK(new_p);
    TEST_ASSERT(GET_ALLOC(HDRP(remainder)) == 0, "Remainder is free");
    TEST_ASSERT(free_list_p == remainder, "Remainder added to free list");

    TEST_ASSERT(check_list_integrity(), "List integrity check");
}

void test_realloc_expand_merge()
{
    printf("\n=== Test 6: Realloc Expand (Merge & Split) ===\n");
    mminit();

    // Setup: [ A (64) ] [ B (256, Free) ]
    char *a = my_malloc(64);
    char *b = my_malloc(256);
    my_free(b);

    size_t a_old_size = GET_SIZE(HDRP(a));

    // Expand A (should eat part of B)
    char *new_a = my_realloc(a, 100);

    TEST_ASSERT(new_a == a, "Pointer unchanged (Merged)");
    TEST_ASSERT(GET_SIZE(HDRP(new_a)) > a_old_size, "Block size increased");

    // Check Remainder of B
    char *remainder = NXT_BLOCK(new_a);
    TEST_ASSERT(GET_ALLOC(HDRP(remainder)) == 0, "Remainder of B is free");
    TEST_ASSERT(free_list_p == remainder, "Remainder at list root");

    TEST_ASSERT(check_list_integrity(), "List integrity check");
}

void test_realloc_fallback()
{
    printf("\n=== Test 7: Realloc Fallback (Copy) ===\n");
    mminit();

    // Setup: [ A ] [ B (Allocated) ] -> Cannot expand A
    char *a = my_malloc(64);
    char *b = my_malloc(64);

    strcpy(a, "Testing123");

    char *new_a = my_realloc(a, 128);

    TEST_ASSERT(new_a != a, "Pointer moved (Fallback)");
    TEST_ASSERT(strcmp(new_a, "Testing123") == 0, "Data preserved");
    TEST_ASSERT(GET_ALLOC(HDRP(a)) == 0, "Old block freed");

    TEST_ASSERT(check_list_integrity(), "List integrity check");
}

/* --- MAIN --- */
int main()
{
    printf("--- FINAL MASTER TEST SUITE ---\n");
    printf("Testing Optimized Explicit Free List Allocator\n");

    test_initialization();
    test_basic_malloc();
    test_lifo_policy();
    test_complex_coalescing();
    test_realloc_shrink_split();
    test_realloc_expand_merge();
    test_realloc_fallback();

    printf("\n------------------------------------------------\n");
    printf("Summary: %d / %d Tests Passed.\n", tests_passed, tests_total);

    if (tests_passed == tests_total)
    {
        printf(ANSI_COLOR_GREEN "PERFECT SCORE! ALL SYSTEMS GO.\n" ANSI_COLOR_RESET);
    }
    else
    {
        printf(ANSI_COLOR_RED "FAILURES DETECTED.\n" ANSI_COLOR_RESET);
    }
    return 0;
}