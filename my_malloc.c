#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>

/*
  TODO
  - best fit for finding required free chunks
  - using mmap for requesting memory
  - calloc, realloc implementation
  - thread safety
*/

#define internal static
#define DEBUG_PRINT(p, p_chunk)                                         \
    do {                                                                \
        if (p && p_chunk) {                                             \
            printf("Addr: 0x%x\n", (p));                                \
            printf("Addr_chunk: 0x%x\n", (p_chunk));                    \
            printf("size: %zu\n", (p_chunk)->size);                     \
            printf("prev: 0x%x\n", (p_chunk)->prev);                    \
            printf("next: 0x%x\n", (p_chunk)->next);                    \
            printf("free: %s\n", (p_chunk)->free ? "true" : "false"); \
            printf("debug: 0x%x\n\n", (p_chunk)->debug);                \
        }                                                               \
        else {                                                          \
            printf("ERROR\n");                                          \
            printf("Addr: 0x%x\n", (p));                                \
            printf("Addr_chunk: 0x%x\n\n", (p_chunk));                  \
        }                                                               \
    } while(0)
#define ALIGNMENT alignof(max_align_t)

struct chunk {
    bool free;
    struct chunk* prev;
    struct chunk* next;
    size_t size;
    // NOTE: Find out which function call is responsible for the chunk
    // TODO: Remove later
    uint32_t debug;
};

internal struct chunk* chunk_head = NULL;

internal
void ll_insert_after(struct chunk* at, struct chunk* new_node)
{
    new_node->next = at->next;
    new_node->prev = at;
    at->next = new_node;
}

internal
void ll_insert_before(struct chunk* at, struct chunk* new_node)
{
    new_node->next = at;
    new_node->prev = at->prev;
    at->prev = new_node;
}


internal
void ll_delete(struct chunk* node)
{
    if (node->prev != NULL) node->prev->next = node->next;
    if (node->next != NULL) node->next->prev = node->prev;
}

internal
struct chunk* request_memory(size_t size)
{
    void* prog_break = sbrk(0);
    size_t align = (ALIGNMENT - (size_t)(prog_break + sizeof(struct chunk))) % ALIGNMENT;

    void* memory = sbrk(size + sizeof(struct chunk) + align);
    if (memory == (void *)-1)
        return NULL;
    struct chunk* chunk_info = (struct chunk *)(memory + align);

    chunk_info->free = false;
    chunk_info->size = size;
    chunk_info->prev = NULL;
    chunk_info->next = NULL;
    chunk_info->debug = 0x77777777;

    if (chunk_head != NULL)
        ll_insert_before(chunk_head, chunk_info);

    chunk_head = chunk_info;
    return chunk_info;
}

internal
struct chunk* find_free_chunk(size_t size)
{
    struct chunk* current = chunk_head;
    while (current != NULL
           && (size > current->size
               || !current->free))
        current = current->next;

    return current;
}

internal
void split_free_chunk(struct chunk* free_chunk, size_t size)
{
    size_t size_diff = free_chunk->size - size;
    if (size_diff <= 0) return;

    size_t min_size = sizeof(struct chunk);
    void* chunk_end = (void*)(free_chunk + 1) + size;
    size_t align = (ALIGNMENT - (size_t)(chunk_end + sizeof(struct chunk))) % ALIGNMENT;
    min_size += align;

    if (size_diff < min_size) return;

    struct chunk* new_free_chunk = (struct chunk *)(chunk_end + align);

    new_free_chunk->free = true;
    new_free_chunk->size = size_diff - min_size;
    new_free_chunk->debug = 0x23232323;

    free_chunk->size = size;
    free_chunk->debug = 0x12121212;

    ll_insert_after(free_chunk, new_free_chunk);
}

void* my_malloc(size_t size)
{
    void* data_ptr = NULL;

    struct chunk* free_chunk = find_free_chunk(size);

    if (free_chunk == NULL) {
        struct chunk* chunk_info = request_memory(size);
        if (chunk_info == NULL) return NULL;
        return (void *)(chunk_info + 1);
    }
    else {
        split_free_chunk(free_chunk, size);
        free_chunk->free = false;
        data_ptr = (void *)(free_chunk + 1);
    }

    return data_ptr;
}

void coalesce_free(struct chunk* prev, struct chunk* curr)
{
    if (prev != chunk_head) {
        size_t size = (size_t)(prev->prev) - (size_t)(curr + 1);
        curr->size = size;
        ll_delete(prev);
        return;
    }

    size_t size = (size_t)(prev + 1) + prev->size - (size_t)(curr + 1);
    curr->size = size;
    chunk_head = curr;
    ll_delete(prev);
}

void my_free(void* ptr)
{
    if (ptr == NULL) return;

    struct chunk* chunk_info = (struct chunk *)ptr - 1;
    assert(!chunk_info->free);

    if (chunk_info->prev != NULL
        && chunk_info->prev->free) {
        coalesce_free(chunk_info->prev, chunk_info);
    }

    if (chunk_info->next != NULL
        && chunk_info->next->free) {
        chunk_info = chunk_info->next;
        coalesce_free(chunk_info->prev, chunk_info);
    }

    chunk_info->free = true;
    chunk_info->debug = 0x12345678;
}

// TODO test coalescing free chunks
int main()
{
    void* ptr1 = my_malloc(1);
    void* ptr2 = my_malloc(2);
    void* ptr3 = my_malloc(3);
    void* ptr4 = my_malloc(4);
    void* ptr5 = my_malloc(5);

    my_free(ptr5);
    my_free(ptr4);

    my_free(ptr1);
    my_free(ptr2);

    struct chunk* current = chunk_head;
    while (current != NULL) {
        void* data_ptr = (void *)(current + 1);
        DEBUG_PRINT(data_ptr, current);
        current = current->next;
    }

    return 0;
}
