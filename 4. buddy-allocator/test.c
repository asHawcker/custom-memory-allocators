#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

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

int count_free_blocks(int order)
{
    int count = 0;
    block_t *curr = free_list[order];
    while (curr != NULL)
    {
        count++;
        curr = curr->next;
    }
    return count;
}

/* --- Helper: Visualizer --- */
void print_heap_state()
{
    printf("  [Heap State] ");
    int empty = 1;
    for (int i = 0; i <= MAX_ORDER; i++)
    {
        int cnt = count_free_blocks(i);
        if (cnt > 0)
        {
            printf("Ord%d:%d ", i, cnt);
            empty = 0;
        }
    }
    if (empty)
        printf("Empty (Full Leak?)");
    printf("\n");
}

/* --- TEST CASES --- */

void test_initialization()
{
    printf("\n=== Test 1: Initialization ===\n");
    buddy_init();

    // We expect exactly one block at MAX_ORDER
    TEST_ASSERT(count_free_blocks(MAX_ORDER) == 1, "One block at Max Order");

    // All other lists should be empty
    int others_empty = 1;
    for (int i = 0; i < MAX_ORDER; i++)
    {
        if (count_free_blocks(i) != 0)
            others_empty = 0;
    }
    TEST_ASSERT(others_empty, "All lower orders empty");
}

void test_recursive_split()
{
    printf("\n=== Test 2: Recursive Splitting ===\n");
    buddy_init(); // Reset

    // Alloc smallest block (Order 0)
    // This forces 1MB to split all the way down.
    void *p = buddy_alloc(0);

    TEST_ASSERT(p != NULL, "Allocation returned pointer");

    // Verification:
    // We should now have ONE free block in EVERY order from 0 to MAX-1
    // Why? Splitting Order 8 -> One O7 free, One O7 split.
    // Splitting O7 -> One O6 free, One O6 split...

    int split_correct = 1;
    for (int i = 0; i < MAX_ORDER; i++)
    {
        if (count_free_blocks(i) != 1)
        {
            printf("    Error at Order %d: Expected 1, got %d\n", i, count_free_blocks(i));
            split_correct = 0;
        }
    }
    TEST_ASSERT(split_correct, "Cascade split left 1 buddy at each level");
    TEST_ASSERT(count_free_blocks(MAX_ORDER) == 0, "Max Order list is empty");

    print_heap_state();
}

void test_buddies_coalesce()
{
    printf("\n=== Test 3: Buddy Coalescing ===\n");
    buddy_init();

    // Alloc two Order 0 blocks.
    // Since we just init, the first one splits everything.
    // The second one should take the "Right Buddy" sitting in Order 0 list.
    void *a = buddy_alloc(0);
    void *b = buddy_alloc(0);

    TEST_ASSERT(a != NULL && b != NULL, "Allocated buddies A and B");
    TEST_ASSERT(count_free_blocks(0) == 0, "Order 0 list empty (consumed)");

    // Calculate expected buddy address math manually
    // If A is first, B should be A + 4096.
    // OR if list inserts at head, allocation order might swap.
    // Let's just check they are distinct.
    TEST_ASSERT(a != b, "Pointers are distinct");

    // Free B. It enters Order 0 list.
    buddy_free(b);
    TEST_ASSERT(count_free_blocks(0) == 1, "Freed B sits in Order 0");

    // Free A. It should MERGE with B.
    // Order 0 becomes empty. Order 1 gains a block...
    // which merges with the waiting Order 1 buddy...
    // cascading all the way up.
    buddy_free(a);

    TEST_ASSERT(count_free_blocks(MAX_ORDER) == 1, "Fully coalesced back to Max Order");
    print_heap_state();
}

void test_fragmentation_holes()
{
    printf("\n=== Test 4: Fragmentation Pattern ===\n");
    buddy_init();

    // Alloc A (Order 0)
    void *a = buddy_alloc(0);

    // Alloc B (Order 1) - Skips the O0 buddy, takes O1 buddy
    void *b = buddy_alloc(1);

    // Alloc C (Order 0) - Should take the O0 buddy left behind by A
    void *c = buddy_alloc(0);

    print_heap_state();

    TEST_ASSERT(a && b && c, "Allocated A, B, C");
    TEST_ASSERT(count_free_blocks(0) == 0, "Order 0 list exhausted");

    // Free B (Order 1). It cannot merge because its buddies are split or in use.
    buddy_free(b);
    TEST_ASSERT(count_free_blocks(1) >= 1, "B sits in Order 1 waiting");

    // Free A and C. They should merge into an Order 1 block.
    // That Order 1 block should merge with B (if addresses align) or sit next to it.
    buddy_free(a);
    buddy_free(c);

    TEST_ASSERT(count_free_blocks(MAX_ORDER) == 1, "Heap eventually fully restored");
}

int main()
{
    printf("--- Buddy Allocator Unit Tests ---\n");

    test_initialization();
    test_recursive_split();
    test_buddies_coalesce();
    test_fragmentation_holes();

    printf("\n------------------------------------------------\n");
    printf("Summary: %d / %d Tests Passed.\n", tests_passed, tests_total);
    if (tests_passed == tests_total)
    {
        printf(ANSI_COLOR_GREEN "ALL TESTS PASSED.\n" ANSI_COLOR_RESET);
    }
    else
    {
        printf(ANSI_COLOR_RED "FAILURES DETECTED.\n" ANSI_COLOR_RESET);
    }
    return 0;
}