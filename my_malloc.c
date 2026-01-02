#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>

/*
  TODO
  - splitting free chunks into smaller chunks as per requirement
  - best fit for finding required free chunks
  - coalescing free chunks
  - alignment of data pointer returned by malloc
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

struct chunk {
    bool free;
    struct chunk* next;
    size_t size;
    // NOTE: Find out which function call is responsible for the chunk
    // TODO: Remove later
    uint32_t debug;
};

struct chunk* chunk_head = NULL;

internal
struct chunk* request_memory(size_t size)
{
    void* memory = (struct chunk *)sbrk(size + sizeof(struct chunk));
    struct chunk* chunk_info = (struct chunk *)memory;

    chunk_info->free = false;
    chunk_info->next = chunk_head;
    chunk_info->size = size;
    chunk_info->debug = 0x77777777;

    chunk_head = chunk_info;
    ++chunk_info;

    if (memory == (void *)-1)
        return NULL;

    return chunk_info;
}

void* my_malloc(size_t size)
{
    void* data_ptr = NULL;

    struct chunk* current = chunk_head;
    while (current != NULL
           && (size > current->size
               || !current->free))
        current = current->next;

    if (current == NULL) {
        data_ptr = (void *)request_memory(size);
    }
    else {
        current->size = size;
        current->debug = 0x55555555;
        current->free = false;
        data_ptr = (void *)(current + 1);
    }

    return data_ptr;
}

void free(void* ptr)
{
    if (ptr == NULL)
        return;

    struct chunk* chunk_info = (struct chunk *)ptr - 1;
    assert(!chunk_info->free);

    chunk_info->free = true;
    chunk_info->debug = 0x12345678;
}

int main()
{
    void* ptr = my_malloc(23);
    struct chunk* ptr_chunk = (struct chunk *)ptr - 1;
    DEBUG_PRINT(ptr, ptr_chunk);

    ptr = my_malloc(23);
    ptr_chunk = ((struct chunk *)ptr) - 1;
    DEBUG_PRINT(ptr, ptr_chunk);

    free(ptr);

    ptr = my_malloc(23);
    ptr_chunk = ((struct chunk *)ptr) - 1;
    DEBUG_PRINT(ptr, ptr_chunk);

    return 0;
}
