/*
 * implicit-freelist allocator (simple malloc/free)
 *
 * This implementation uses an implicit free list with boundary tags.
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

#define WORD 8              // machine word size in bytes (8 on 64-bit, 4 on 32-bit)
#define DWORD 16            // double word size (alignment). For 32-bit machines use 8.
#define CHUNKSIZE (1 << 12) // initial heap extension size (4KB)

/* Read and write a word at address p (use uintptr_t for correct size) */
#define GET(p) (*(uintptr_t *)(p))
#define PUT(p, val) (*(uintptr_t *)(p) = (val))

/* Pack a size and allocated bit into a header/footer word */
#define PACK(size, alloc) ((size) | (alloc))

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Read the size and allocated fields from address p (header/footer) */
#define GET_SIZE(p) (GET(p) & ~(DWORD - 1)) /* mask out alloc bit(s) */
#define GET_ALLOC(p) (GET(p) & 0x1)         /* allocation bit is LSB */

/* Given block pointer bp (points to payload), compute addresses of header/footer */
#define HDRP(bp) ((char *)(bp) - WORD)
#define FTRP(bp) (((char *)(bp) + GET_SIZE(HDRP(bp))) - DWORD)

/* Given block pointer bp, compute pointer to next and previous blocks' payload */
#define NXT_BLOCK(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PRV_BLOCK(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DWORD))

/* Pointer to first block's payload (after prologue) */
static char *heap_list_p = 0;

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
        /* Case 1: both neighbors allocated — nothing to do */
    }
    else if (prev_alloc && !next_alloc)
    {
        /* Case 2: merge with next block */
        size += GET_SIZE(HDRP(NXT_BLOCK(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        /* Case 3: merge with previous block */
        size += GET_SIZE(FTRP(PRV_BLOCK(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PRV_BLOCK(bp)), PACK(size, 0));
        bp = PRV_BLOCK(bp); /* new payload pointer is at previous block */
    }
    else
    {
        /* Case 4: merge with both previous and next */
        size += GET_SIZE(FTRP(PRV_BLOCK(bp))) + GET_SIZE(HDRP(NXT_BLOCK(bp)));
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

    /* Ensure the block size is a multiple of DWORD for alignment */
    size = (words % 2) ? (words + 1) * WORD : words * WORD;

    if ((long)(bp = sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and new epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NXT_BLOCK(bp)), PACK(0, 1)); /* new epilogue header */

    return coalesce(bp);
}

/*
 * mminit - create the initial empty heap with prologue and epilogue
 * Returns 0 on success, -1 on error
 */
int mminit(void)
{
    /* Create initial empty heap: 4 words for alignment/prologue/epilogue */
    if ((heap_list_p = sbrk(4 * WORD)) == (void *)(-1))
        return -1;

    PUT(heap_list_p, 0);                           /* alignment padding */
    PUT(heap_list_p + WORD, PACK(DWORD, 1));       /* prologue header (allocated) */
    PUT(heap_list_p + (2 * WORD), PACK(DWORD, 1)); /* prologue footer (allocated) */
    PUT(heap_list_p + (3 * WORD), PACK(0, 1));     /* epilogue header (allocated) */
    heap_list_p += (2 * WORD);                     /* heap_list_p now points to prologue payload */

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
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

    for (bp = heap_list_p; GET_SIZE(HDRP(bp)) > 0; bp = NXT_BLOCK(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= size))
        {
            return bp; /* found a fit */
        }
    }
    return NULL; /* no fit found */
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
        /* Split: allocate front part and leave remainder as free block */
        PUT(HDRP(bp), PACK((size), 1));
        PUT(FTRP(bp), PACK((size), 1));

        /* Set header/footer for the remaining free block */
        PUT(HDRP(NXT_BLOCK(bp)), PACK((asize - size), 0));
        PUT(FTRP(NXT_BLOCK(bp)), PACK((asize - size), 0));
    }
    else
    {
        /* Do not split: mark whole block as allocated */
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

    if (heap_list_p == 0) /* lazy initialization of heap */
    {
        mminit();
    }

    if (size <= 0)
        return NULL;

    /* Adjust block size to include overhead and to satisfy alignment requirements */
    if (size <= DWORD)
        asize = (2 * DWORD); /* minimum block size (header+footer+minimum payload) */
    else
    {
        /* Round up to nearest multiple of DWORD and add header/footer */
        asize = DWORD * ((size + DWORD + (DWORD - 1)) / DWORD);
    }

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    size_t extension = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extension / WORD)) != NULL)
    {
        place(bp, asize);
        return bp;
    }
    return NULL; /* out of memory */
}

/*
 * my_free - free a previously allocated block and coalesce if possible
 */
void my_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0)); /* mark header as free */
    PUT(FTRP(bp), PACK(size, 0)); /* mark footer as free */
    coalesce(bp);                 /* merge with adjacent free blocks if any */
}
