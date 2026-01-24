#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include "alloc.c"

#define NUM_OPS 100000
#define MAX_ALLOC_SIZE 1024
#define HEAP_SIZE_LIMIT (1024 * 1024 * 50) // 50 MB simulated

void *pointers[NUM_OPS];
int ptr_status[NUM_OPS]; // 0 = free, 1 = allocated

int main()
{
    printf("Starting Benchmark...\n");
    printf("Total Operations: %d\n", NUM_OPS);

    // Initialize
    if (mminit() == -1)
    {
        printf("Heap init failed\n");
        return 1;
    }

    // Seed randomness for consistency between runs
    srand(42);

    clock_t start = clock();

    int successful_allocs = 0;

    for (int i = 0; i < NUM_OPS; i++)
    {
        // Randomly choose to Alloc (60%) or Free (40%)
        // We bias towards Alloc to fill the heap and stress the search algorithm
        int action = rand() % 10;

        if (action < 6)
        {
            // --- ALLOCATE ---
            size_t size = (rand() % MAX_ALLOC_SIZE) + 1;
            void *p = my_malloc(size);

            if (p != NULL)
            {
                // Find a slot to store this pointer
                // (In a real app, this logic wouldn't be part of the benchmark cost,
                // but here it's negligible compared to heap search time).
                pointers[i] = p;
                ptr_status[i] = 1;

                // Optional: Write to it to ensure it's valid memory
                *(int *)p = 12345;
                successful_allocs++;
            }
            else
            {
                pointers[i] = NULL;
                ptr_status[i] = 0;
            }
        }
        else
        {
            // --- FREE ---
            // Pick a random previous index to free
            if (i > 0)
            {
                int victim_idx = rand() % i;
                if (ptr_status[victim_idx] == 1)
                {
                    my_free(pointers[victim_idx]);
                    ptr_status[victim_idx] = 0;
                }
            }
            pointers[i] = NULL; // Current slot unused
            ptr_status[i] = 0;
        }
    }

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

    printf("--------------------------------------------\n");
    printf("Benchmark Complete.\n");
    printf("Successful Allocations: %d\n", successful_allocs);
    printf("Time Taken: %f seconds\n", time_spent);
    printf("Throughput: %.0f ops/sec\n", NUM_OPS / time_spent);
    printf("--------------------------------------------\n");

    return 0;
}