#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

typedef char ALIGN[24]; // 24 for a 64-bit machine; would be 16 for a 32-bit machine
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

header_t *get_free_block(size_t size){
    header_t *curr = head;
    while(curr){
        if(curr->s.free && curr->s.size >= size){
            return curr;
        }
        curr = curr->s.next;
    }
    return NULL;
}

void *malloc(size_t size){
    size_t total_size;
    header_t *header;
    void *block;
    if(!size) return NULL;
    pthread_mutex_lock(&global_lock);
    header = get_free_block(size);
    if (header) {
        header->s.free = 0;
        pthread_mutex_unlock(&global_lock);
        return (void *)(header + 1);
    }
    total_size = size + sizeof(header_t);
    block = sbrk(total_size);
    if ( block == (void*)-1){
        pthread_mutex_unlock(&global_lock);
        return NULL;
    }
    header = block;
    header->s.free = 0;
    header->s.next = NULL;
    header->s.size = size;
    if(!head){
        head = header;
    }
    if(tail){
        tail->s.next = header;
    }
    tail = header;
    pthread_mutex_unlock(&global_lock);
    return (void*)(header+1);
}

void free(void *block){
    header_t *header, *tmp;
    void* programbreak;
    if(!block) return;
    pthread_mutex_lock(&global_lock);
    header = (header_t*)block - 1;
    programbreak = sbrk(0);
    if((char*)block + header->s.size == programbreak){
        if(head == tail){
            head = tail = NULL;
        }else{
            tmp = head;
            while(tmp){
                if(tmp->s.next == tail){
                    tmp->s.next = NULL;
                    tail = tmp;
                }
                tmp = tmp->s.next;
            }
        }
        sbrk(0-sizeof(header_t) + header->s.size);
        pthread_mutex_unlock(&global_lock);
        return;
    }
    header->s.free = 1;
    pthread_mutex_unlock(&global_lock);
}