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
