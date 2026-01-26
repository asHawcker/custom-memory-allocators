#include <stdint.h>

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
