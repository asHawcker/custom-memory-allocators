#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include "../4. buddy-allocator/alloc.c"

typedef struct slab_t
{
    struct slab_t *next;
    void *page_start;
    int free_count;
    uint32_t bitmap;
} slab_t;

typedef struct kmem_cache_t
{
    slab_t *slabs_partial;
    slab_t *slabs_full;
    slab_t *slabs_free;

    size_t obj_size;
    int objects_per_slab;
    const char *name;
} kmem_cache_t;

kmem_cache_t *kmem_cache_create(const char *name, size_t size)
{
    kmem_cache_t *cache = (kmem_cache_t *)malloc(sizeof(kmem_cache_t));

    cache->name = name;
    cache->obj_size = size;

    cache->objects_per_slab = PAGE_SIZE / size;

    // Constraint: Our simple bitmap supports max 32 objects.
    if (cache->objects_per_slab > 32)
    {
        cache->objects_per_slab = 32;
    }

    cache->slabs_partial = NULL;
    cache->slabs_full = NULL;
    cache->slabs_free = NULL;

    return cache;
}

slab_t *slab_create(kmem_cache_t *cache)
{
    slab_t *slab = (slab_t *)malloc(sizeof(slab_t));

    slab->page_start = buddy_alloc(0);
    if (slab->page_start == NULL)
    {
        free(slab);
        return NULL;
    }

    slab->free_count = cache->objects_per_slab;
    slab->bitmap = 0;
    slab->next = NULL;

    return slab;
}

void *kmem_cache_alloc(kmem_cache_t *cache)
{
    slab_t *slab = NULL;

    if (cache->slabs_partial)
    {
        slab = cache->slabs_partial;
    }
    else if (cache->slabs_free)
    {
        slab = cache->slabs_free;
        cache->slabs_free = slab->next;
        slab->next = cache->slabs_partial;
        cache->slabs_partial = slab;
    }
    else
    {
        slab = slab_create(cache);
        if (!slab)
            return NULL;

        slab->next = cache->slabs_partial;
        cache->slabs_partial = slab;
    }

    int slot = -1;
    for (int i = 0; i < cache->objects_per_slab; i++)
    {
        if (!((slab->bitmap >> i) & 1))
        {
            slot = i;
            break;
        }
    }

    if (slot == -1)
        return NULL;

    slab->bitmap |= (1 << slot);
    slab->free_count--;

    void *obj_ptr = (char *)slab->page_start + (slot * cache->obj_size);

    if (slab->free_count == 0)
    {
        cache->slabs_partial = slab->next;

        slab->next = cache->slabs_full;
        cache->slabs_full = slab;
    }

    return obj_ptr;
}

slab_t *find_slab(void *ptr, slab_t *head, slab_t **prev_ptr)
{
    slab_t *curr = head;
    slab_t *prev = NULL;

    while (curr)
    {
        if (ptr >= curr->page_start &&
            ptr < (curr->page_start + PAGE_SIZE))
        {

            if (prev_ptr)
                *prev_ptr = prev;
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

void kmem_cache_free(kmem_cache_t *cache, void *ptr)
{
    if (!ptr)
        return;

    slab_t *slab = NULL;
    slab_t *prev = NULL;
    int from_full_list = 0;

    slab = find_slab(ptr, cache->slabs_partial, &prev);

    if (!slab)
    {
        slab = find_slab(ptr, cache->slabs_full, &prev);
        from_full_list = 1;
    }

    if (!slab)
    {
        return;
    }

    uintptr_t offset = (uintptr_t)ptr - (uintptr_t)slab->page_start;
    int slot = offset / cache->obj_size;

    slab->bitmap &= ~(1 << slot);
    slab->free_count++;

    // CASE A: Slab was FULL. Now it has 1 free slot.
    // Move from Full -> Partial
    if (from_full_list)
    {
        if (prev)
            prev->next = slab->next;
        else
            cache->slabs_full = slab->next;

        slab->next = cache->slabs_partial;
        cache->slabs_partial = slab;
    }

    // CASE B: Slab was PARTIAL. If all slots are free.
    // Move from Partial -> Free
    else if (slab->free_count == cache->objects_per_slab)
    {
        if (prev)
            prev->next = slab->next;
        else
            cache->slabs_partial = slab->next;

        slab->next = cache->slabs_free;
        cache->slabs_free = slab;
    }
}
void free_slab_list(slab_t *head)
{
    while (head)
    {
        slab_t *temp = head;
        head = head->next;

        buddy_free((block_t *)((slab_t *)temp->page_start));
        free(temp);
    }
}

void kmem_cache_destroy(kmem_cache_t *cache)
{

    free_slab_list(cache->slabs_full);
    free_slab_list(cache->slabs_partial);
    free_slab_list(cache->slabs_free);

    free(cache);
}