#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

typedef char ALIGN[16];
typedef union header{

    struct header_t{
        size_t size;
        unsigned free;
        union header *next;
    } s;
    ALIGN stb;
} header_t;

header_t *head, *tail;
pthread_mutex_t global_lock;

void *malloc(size_t){
    size_t total_size;
    header_t *header;
    void *block;
}