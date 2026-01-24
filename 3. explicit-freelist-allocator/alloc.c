/*
 * explicit-freelist allocator (simple malloc/free)
 *
 * LEARNING PATH:
 * - Started with understanding memory layout: header | payload | footer
 * - Learned boundary tags allow coalescing in both directions
 * - Discovered explicit freelists are faster than implicit (no full heap scan)
 * - Implemented doubly-linked list to track free blocks efficiently
 *
 * This implementation uses an explicit free list with boundary tags.
 * Assumes a 64-bit machine by default (WORD = 8). Change WORD and DWORD
 * for a 32-bit machine (WORD = 4, DWORD = 8).
 *
 * Layout of a block:
 *    [ header | payload... | footer ]
 *
 * Prologue block: allocated block of size DWORD to make edge conditions simpler.
 * Epilogue header: zero-size allocated block at the end of the heap.
 */

#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define WORD 8
#define DWORD 16
#define CHUNKSIZE (1 << 12)

#define GET(p) (*(uintptr_t *)(p))
#define PUT(p, val) (*(uintptr_t *)(p) = (val))

#define PACK(size, alloc) ((size) | (alloc))

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define GET_SIZE(p) (GET(p) & ~(DWORD - 1))
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WORD)
#define FTRP(bp) (((char *)(bp) + GET_SIZE(HDRP(bp))) - DWORD)

#define NXT_BLOCK(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PRV_BLOCK(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DWORD))

/* Free list node stores pointers at bp (prev) and bp+WORD (next) */
#define GET_NXT_PTR(bp) (*(char **)(bp + WORD))
#define GET_PRV_PTR(bp) (*(char **)(bp))

static char *heap_list_p = 0;
static char *free_list_p = 0;

/* Insert new free block at front of explicit free list (LIFO policy) */
void insert_node(void *bp)
{
    GET_NXT_PTR(bp) = free_list_p;
    GET_PRV_PTR(bp) = NULL;

    if (free_list_p != NULL)
    {
        GET_PRV_PTR(free_list_p) = bp;
    }
    free_list_p = bp;
}

/* Remove block from doubly-linked free list */
void delete_node(void *bp)
{
    if (GET_NXT_PTR(bp))
    {
        GET_PRV_PTR(GET_NXT_PTR(bp)) = GET_PRV_PTR(bp);
    }
    if (GET_PRV_PTR(bp))
    {
        GET_NXT_PTR(GET_PRV_PTR(bp)) = GET_NXT_PTR(bp);
    }
    else
    {
        /* bp was the head of free list */
        free_list_p = GET_NXT_PTR(bp);
    }
}

/*
 * coalesce - boundary-tag coalescing. Return pointer to coalesced block.
 * Four cases:
 * 1) prev_alloc && next_alloc : no coalescing
 * 2) prev_alloc && !next_alloc : merge with next
 * 3) !prev_alloc && next_alloc : merge with previous
 * 4) !prev_alloc && !next_alloc : merge with both
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PRV_BLOCK(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NXT_BLOCK(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
    {
        insert_node(bp);
    }
    else if (prev_alloc && !next_alloc)
    {
        /* Merge current block with free next block */
        size += GET_SIZE(HDRP(NXT_BLOCK(bp)));
        delete_node(NXT_BLOCK(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        insert_node(bp);
    }
    else if (!prev_alloc && next_alloc)
    {
        /* Merge current block with free previous block, update headers/footers */
        size += GET_SIZE(FTRP(PRV_BLOCK(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PRV_BLOCK(bp)), PACK(size, 0));
        bp = PRV_BLOCK(bp);
    }
    else
    {
        /* Merge with both neighbors; prev is already in free list, so don't re-insert */
        size += GET_SIZE(FTRP(PRV_BLOCK(bp))) + GET_SIZE(HDRP(NXT_BLOCK(bp)));
        delete_node(NXT_BLOCK(bp));
        PUT(HDRP(PRV_BLOCK(bp)), PACK(size, 0));
        PUT(FTRP(NXT_BLOCK(bp)), PACK(size, 0));
        bp = PRV_BLOCK(bp);
    }
    return bp;
}

/*
 * extend_heap - extend heap by 'words' words, return pointer to new free block's payload
 * We ensure alignment by making the size an even number of WORDs (so result is multiple of DWORD).
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Round up to maintain alignment: new block size must be multiple of DWORD */
    size = (words % 2) ? (words + 1) * WORD : words * WORD;

    if ((long)(bp = sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    /* New epilogue: zero-size allocated block marks heap end */
    PUT(HDRP(NXT_BLOCK(bp)), PACK(0, 1));

    return coalesce(bp);
}

/*
 * mminit - create the initial empty heap with prologue and epilogue
 * Returns 0 on success, -1 on error
 */
int mminit(void)
{
    if ((heap_list_p = sbrk(4 * WORD)) == (void *)(-1))
        return -1;
    free_list_p = NULL;
    /* Prologue: padding (unused), header, footer, and epilogue header */
    PUT(heap_list_p, 0);
    PUT(heap_list_p + WORD, PACK(DWORD, 1));
    PUT(heap_list_p + (2 * WORD), PACK(DWORD, 1));
    PUT(heap_list_p + (3 * WORD), PACK(0, 1));
    /* Point to prologue footer (start of usable heap) */
    heap_list_p += (2 * WORD);

    if (extend_heap(CHUNKSIZE / WORD) == NULL)
        return -1;
    return 0;
}

/*
 * find_fit - first-fit search for a free block with at least 'size' bytes (including header/footer)
 * Returns payload pointer (bp) or NULL if no fit found.
 */
static void *find_fit(size_t size)
{
    char *bp;
    /* Traverse explicit free list from head; O(number of free blocks) vs O(heap size) */
    for (bp = free_list_p; bp != NULL; bp = GET_NXT_PTR(bp))
    {
        if (size <= GET_SIZE(HDRP(bp)))
        {
            return bp;
        }
    }
    return NULL;
}

/*
 * place - place a block of 'size' bytes at start of free block bp
 * If the remainder would be at least the minimum block size (2*DWORD), split the block.
 */
static void place(void *bp, size_t size)
{
    size_t asize = GET_SIZE(HDRP(bp));

    if ((asize - size) >= (2 * DWORD))
    {
        /* Fragment is large enough to be a separate block: split */
        delete_node(bp);
        PUT(HDRP(bp), PACK((size), 1));
        PUT(FTRP(bp), PACK((size), 1));

        PUT(HDRP(NXT_BLOCK(bp)), PACK((asize - size), 0));
        PUT(FTRP(NXT_BLOCK(bp)), PACK((asize - size), 0));
        insert_node(NXT_BLOCK(bp));
    }
    else
    {
        /* Fragment too small; allocate entire block to avoid excessive fragmentation */
        delete_node(bp);
        PUT(HDRP(bp), PACK((asize), 1));
        PUT(FTRP(bp), PACK((asize), 1));
    }
}

/*
 * my_malloc - allocate a block with at least 'size' bytes of payload
 * Returns pointer to payload, or NULL on failure
 */
void *my_malloc(size_t size)
{
    char *bp;
    size_t asize;

    if (heap_list_p == 0)
    {
        mminit();
    }

    if (size <= 0)
        return NULL;

    /* Minimum allocation: 2*DWORD (header + 2 pointers for free list + footer) */
    if (size <= DWORD)
        asize = (2 * DWORD);
    else
    {
        /* Round up to nearest multiple of DWORD for alignment */
        asize = DWORD * ((size + DWORD + (DWORD - 1)) / DWORD);
    }

    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found; extend heap by max(requested, CHUNKSIZE) */
    size_t extension = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extension / WORD)) != NULL)
    {
        place(bp, asize);
        return bp;
    }
    return NULL;
}

/*
 * my_free - free a previously allocated block and coalesce if possible
 */
void my_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

void *my_realloc(void *ptr, size_t size)
{
    if (size == 0)
    {
        my_free(ptr);
        return NULL;
    }

    if (ptr == NULL)
    {
        return my_malloc(size);
    }

    size_t asize;
    if (size <= DWORD)
        asize = 2 * DWORD;
    else
        asize = DWORD * ((size + DWORD + (DWORD - 1)) / DWORD);

    size_t old_size = GET_SIZE(HDRP(ptr));

    if (asize <= old_size)
    {
        /* Shrink or no change: split if fragment is large enough */
        if ((old_size - asize) >= (2 * DWORD))
        {
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));

            void *next_ptr = NXT_BLOCK(ptr);
            PUT(HDRP(next_ptr), PACK(old_size - asize, 0));
            PUT(FTRP(next_ptr), PACK(old_size - asize, 0));

            coalesce(next_ptr);
        }
        return ptr;
    }

    /* Need to grow: try to use free adjacent block without moving data */
    size_t next_alloc = GET_ALLOC(HDRP(NXT_BLOCK(ptr)));
    size_t next_size = GET_SIZE(HDRP(NXT_BLOCK(ptr)));
    size_t total_avail = old_size + next_size;

    if (!next_alloc && (total_avail >= asize))
    {
        /* Merge with free next block in-place */
        delete_node(NXT_BLOCK(ptr));

        if ((total_avail - asize) >= (2 * DWORD))
        {
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));

            void *remainder_ptr = NXT_BLOCK(ptr);
            PUT(HDRP(remainder_ptr), PACK(total_avail - asize, 0));
            PUT(FTRP(remainder_ptr), PACK(total_avail - asize, 0));

            insert_node(remainder_ptr);
        }
        else
        {
            PUT(HDRP(ptr), PACK(total_avail, 1));
            PUT(FTRP(ptr), PACK(total_avail, 1));
        }

        return ptr;
    }

    /* Can't realloc in-place; allocate new block and copy data */
    void *new_ptr = my_malloc(size);
    if (new_ptr == NULL)
        return NULL;

    /* Copy old_size minus header; cap at requested size to avoid overflow */
    size_t copy_size = old_size - DWORD;

    if (size < copy_size)
        copy_size = size;

    memcpy(new_ptr, ptr, copy_size);
    my_free(ptr);

    return new_ptr;
}