#include <stdint.h>
#include <unistd.h>

#define MAX_ORDER 8
#define PAGE_SIZE 4096
#define RAM_SIZE (PAGE_SIZE * (1 << MAX_ORDER))

static uint8_t *heap_start;

typedef struct block_t
{
    struct block_t *next;
    struct block_t *prev;
    int order;
    int is_free;
} block_t;

static block_t *free_list[MAX_ORDER + 1];

#define GET_BLOCK(ptr) ((block_t *)ptr)

void list_add(block_t *block, int order)
{
    block->order = order;
    block->is_free = 1;
    block->next = free_list[order];
    block->prev = NULL;

    if (free_list[order] != NULL)
    {
        free_list[order]->prev = block;
    }
    free_list[order] = block;
}

void list_remove(block_t *block)
{
    if (block->prev)
    {
        block->prev->next = block->next;
    }
    else
    {
        free_list[block->order] = block->next;
    }
    if (block->next)
    {
        block->next->prev = block->prev;
    }

    block->next = NULL;
    block->prev = NULL;
    block->is_free = 0;
}

void buddy_init()
{
    heap_start = (uint8_t *)malloc(RAM_SIZE);
    if (heap_start == NULL)
    {
        perror("Failed to allocate RAM");
        exit(1);
    }

    for (int i = 0; i < MAX_ORDER + 1; i++)
    {
        free_list[i] = NULL;
    }

    block_t *root_block = (block_t *)heap_start;
    root_block->order = MAX_ORDER;
    root_block->is_free = 1;
    root_block->next = NULL;
    root_block->prev = NULL;

    free_list[MAX_ORDER] = root_block;
}

void *buddy_alloc(int8_t req_order)
{
    uint8_t curr_order;
    for (curr_order = req_order; curr_order <= MAX_ORDER; curr_order++)
    {
        if (free_list[curr_order] != NULL)
        {
            block_t *block = free_list[curr_order];
            list_remove(block);

            while (curr_order > req_order)
            {
                curr_order--;
                block_t *buddy = (block_t *)((uint8_t *)block + (PAGE_SIZE << curr_order));
                list_add(buddy, curr_order);
            }
            block->is_free = 0;
            block->order = curr_order;
            return (void *)block;
        }
    }
    return NULL;
}

void buddy_free(block_t *ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    block_t *block = (block_t *)ptr;
    int curr_order = block->order;
    while (curr_order < MAX_ORDER)
    {
        size_t block_size = PAGE_SIZE << curr_order;
        size_t offset = (uint8_t *)block - heap_start;
        size_t buddy_offset = offset ^ block_size;

        block_t *buddy = (block_t *)(heap_start + buddy_offset);

        if (!buddy->is_free || buddy->order != curr_order)
        {
            break; // Cannot merge
        }

        // merge
        printf("  Merging %p with buddy %p (Order %d -> %d)\n", block, buddy, curr_order, curr_order + 1);

        list_remove(buddy);

        if (buddy < block)
        {
            block = buddy;
        }

        curr_order++;
        block->order = curr_order;
    }

    list_add(block, curr_order);
}