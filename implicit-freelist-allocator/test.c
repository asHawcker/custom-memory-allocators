#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "alloc.c"

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

// Validates the heap integrity by walking the blocks
int check_heap_integrity()
{
    char *bp = heap_list_p;

    // Check Prologue
    if (GET_SIZE(HDRP(heap_list_p)) != DWORD || !GET_ALLOC(HDRP(heap_list_p)))
    {
        printf("ERROR: Bad Prologue\n");
        return 0;
    }

    int free_blocks = 0;
    int prev_alloc = 1;

    // Walk the list
    for (bp = heap_list_p; GET_SIZE(HDRP(bp)) > 0; bp = NXT_BLOCK(bp))
    {

        // Check 1: Alignment (Must be divisible by 16)
        if ((uintptr_t)bp % DWORD != 0)
        {
            printf("ERROR: Block %p is not 16-byte aligned.\n", bp);
            return 0;
        }

        // Check 2: Boundary consistency (Header == Footer)
        if (GET(HDRP(bp)) != GET(FTRP(bp)))
        {
            printf("ERROR: Header/Footer mismatch at %p\n", bp);
            return 0;
        }

        // Check 3: Coalescing Invariant (No two free blocks in a row)
        int is_alloc = GET_ALLOC(HDRP(bp));
        if (!prev_alloc && !is_alloc)
        {
            printf("ERROR: Escaped Coalescing at %p. Two consecutive free blocks.\n", bp);
            return 0;
        }
        prev_alloc = is_alloc;
    }

    // Check Epilogue
    if (GET_SIZE(HDRP(bp)) != 0 || !GET_ALLOC(HDRP(bp)))
    {
        printf("ERROR: Bad Epilogue\n");
        return 0;
    }
    return 1;
}

/* --- Test Cases --- */

void test_initialization()
{
    printf("\n=== Test 1: Initialization ===\n");
    heap_list_p = 0; // Reset
    int res = mminit();
    TEST_ASSERT(res == 0, "mminit returns success");
    TEST_ASSERT(heap_list_p != NULL, "heap_list_p is initialized");
    TEST_ASSERT(check_heap_integrity(), "Heap consistent after init");
}

void test_basic_malloc()
{
    printf("\n=== Test 2: Basic Allocation & Alignment ===\n");

    // Alloc 1 byte (Should round up to 32 bytes total block size)
    char *p1 = my_malloc(1);
    TEST_ASSERT(p1 != NULL, "Malloc returned a pointer");
    TEST_ASSERT((uintptr_t)p1 % 16 == 0, "Pointer is 16-byte aligned");

    // White-box: Check internal block size
    // Payload 16 + Header 8 + Footer 8 = 32
    size_t block_size = GET_SIZE(HDRP(p1));
    TEST_ASSERT(block_size == 32, "Block size rounded up correctly (min block size)");

    // Write data to verify safety
    *p1 = 'A';
    TEST_ASSERT(*p1 == 'A', "Memory is writable");

    my_free(p1);
    TEST_ASSERT(check_heap_integrity(), "Heap consistent after free");
}

void test_coalescing()
{
    printf("\n=== Test 3: Coalescing (Merging Free Blocks) ===\n");

    // Allocate 3 blocks
    char *p1 = my_malloc(64);
    char *p2 = my_malloc(64);
    char *p3 = my_malloc(64); // Guard block

    TEST_ASSERT(p1 && p2 && p3, "Allocated 3 blocks");

    // Free P1 and P2 (Should merge into one big block)
    my_free(p1);
    my_free(p2);

    // Alloc a block larger than one of them, but smaller than the sum
    // P1 (80 bytes w/ overhead) + P2 (80 bytes w/ overhead) = 160 bytes total available
    // Request 100 bytes payload
    char *p4 = my_malloc(100);

    // If coalescing worked, p4 should start exactly where p1 was
    TEST_ASSERT(p4 == p1, "Coalescing successful: Reused merged space starting at P1");

    my_free(p3);
    my_free(p4);
}

void test_fragmentation_splitting()
{
    printf("\n=== Test 4: Block Splitting ===\n");

    char *p_large = my_malloc(200);
    size_t large_size = GET_SIZE(HDRP(p_large));

    my_free(p_large);

    char *p_small = my_malloc(10);

    TEST_ASSERT(p_small == p_large, "Splitting: Small alloc reused start of large free block");

    size_t small_size = GET_SIZE(HDRP(p_small));
    TEST_ASSERT(small_size < large_size, "Splitting: Block size was reduced (split happened)");

    my_free(p_small);
}

int main()
{
    printf("Starting Malloc Unit Tests...\n");

    test_initialization();
    test_basic_malloc();
    test_coalescing();
    test_fragmentation_splitting();

    printf("\n------------------------------------------------\n");
    printf("Summary: %d / %d Tests Passed.\n", tests_passed, tests_total);
    if (tests_passed == tests_total)
    {
        printf(ANSI_COLOR_GREEN "ALL TESTS PASSED! GOOD JOB!\n" ANSI_COLOR_RESET);
    }
    else
    {
        printf(ANSI_COLOR_RED "SOME TESTS FAILED.\n" ANSI_COLOR_RESET);
    }
    return 0;
}