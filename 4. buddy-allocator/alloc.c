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
