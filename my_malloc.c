#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>

/*
  TODO
  - best fit for finding required free chunks
  - using mmap for requesting memory
  - thread safety
*/

#define internal static
#define global_variable static

#define DEBUG_PRINT(p, p_chunk)                                         \
    do {                                                                \
        if (p && p_chunk) {                                             \
            printf("Addr: %p\n", (p));                                  \
            printf("Addr_chunk: %p\n", (p_chunk));                      \
            printf("size: %zu\n", (p_chunk)->size);                     \
            printf("prev: %p\n", (p_chunk)->prev);                      \
            printf("next: %p\n", (p_chunk)->next);                      \
            printf("free: %s\n", (p_chunk)->free ? "true" : "false");   \
            printf("debug: 0x%x\n\n", (p_chunk)->debug);                \
        }                                                               \
        else {                                                          \
            printf("ERROR\n");                                          \
            printf("Addr: %p\n", (p));                                  \
            printf("Addr_chunk: %p\n\n", (p_chunk));                    \
        }                                                               \
    } while(0)
#define ALIGNMENT alignof(max_align_t)

#define ll_insert_after(at, new_node)                           \
    do {                                                        \
        (new_node)->next = (at)->next;                          \
        (new_node)->prev = at;                                  \
        if ((at)->next != NULL) (at)->next->prev = new_node;    \
        (at)->next = new_node;                                  \
    } while(0)

#define ll_delete(node)                                                 \
    do {                                                                \
        if ((node)->prev != NULL) (node)->prev->next = (node)->next;    \
        if ((node)->next != NULL) (node)->next->prev = (node)->prev;    \
    } while(0)

struct chunk {
    bool free;
    struct chunk* prev;
    struct chunk* next;
    size_t size;
    // NOTE: Find out which function call is responsible for the chunk
    // TODO: Remove later
    uint32_t debug;
};

global_variable struct chunk* chunk_list_tail = NULL;

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

    if (chunk_list_tail != NULL)
        ll_insert_after(chunk_list_tail, chunk_info);

    chunk_list_tail = chunk_info;
    return chunk_info;
}

internal
struct chunk* find_free_chunk(size_t size)
{
    struct chunk* current = chunk_list_tail;
    while (current != NULL
           && (size > current->size
               || !current->free))
        current = current->prev;

    return current;
}

internal
void split_free_chunk(struct chunk* free_chunk, size_t size)
{
    size_t size_diff = free_chunk->size - size;

    if (size_diff <= 0) return;

    free_chunk->size = size;
    free_chunk->debug = 0x12121212;

    size_t min_size = sizeof(struct chunk);
    void* chunk_end = (void*)(free_chunk + 1) + size;
    size_t align = (ALIGNMENT - (size_t)(chunk_end + sizeof(struct chunk))) % ALIGNMENT;
    min_size += align;

    if (size_diff < min_size) return;

    struct chunk* new_free_chunk = (struct chunk *)(chunk_end + align);

    new_free_chunk->free = true;
    new_free_chunk->size = size_diff - min_size;
    new_free_chunk->debug = 0x23232323;

    ll_insert_after(free_chunk, new_free_chunk);

    if (free_chunk == chunk_list_tail) chunk_list_tail = new_free_chunk;
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

void* my_calloc(size_t n, size_t elem_size)
{
    size_t size = n*elem_size;
    void* ptr = my_malloc(size);

    unsigned char* u8_ptr = (unsigned char *)ptr;
    for (;
         size > 0;
         --size)
        *u8_ptr++ = 0;

    ((struct chunk *)ptr - 1)->debug = 0x56785678;

    return ptr;
}

void coalesce_free(struct chunk* next, struct chunk* curr)
{
    if (next != chunk_list_tail) {
        size_t size = (size_t)(next->next) - (size_t)(curr + 1);
        curr->size = size;
        ll_delete(next);
        return;
    }

    size_t size = (size_t)(next + 1) + next->size - (size_t)(curr + 1);
    curr->size = size;
    chunk_list_tail = curr;
    ll_delete(next);
}

void my_free(void* ptr)
{
    if (ptr == NULL) return;

    struct chunk* chunk_info = (struct chunk *)ptr - 1;
    assert(!chunk_info->free && "Double free detected");

    if (chunk_info->next != NULL
        && chunk_info->next->free) {
        coalesce_free(chunk_info->next, chunk_info);
    }

    if (chunk_info->prev != NULL
        && chunk_info->prev->free) {
        chunk_info = chunk_info->prev;
        coalesce_free(chunk_info->next, chunk_info);
    }

    chunk_info->free = true;
    chunk_info->debug = 0x12345678;
}

void* my_realloc(void* ptr, size_t size)
{
    struct chunk* old_chunk = (struct chunk *)ptr - 1;
    assert(!old_chunk->free && "Tried to reallocate a free pointer");

    void* new_ptr = my_malloc(size);

    unsigned char* u8_ptr = (unsigned char *)ptr;
    unsigned char* u8_new_ptr = new_ptr;
    for (size_t prev_size = old_chunk->size;
         prev_size > 0;
         --prev_size)
        *u8_new_ptr++ = *u8_ptr++;

    my_free(ptr);
    return new_ptr;
}

int main()
{
    int n = 20;
    my_malloc(sizeof(int)*n);
    int* ptr1 = (int *)my_malloc(sizeof(int)*n);
    my_malloc(sizeof(int)*n);
    my_malloc(sizeof(int)*n);

    for (size_t i=0; i<n; i++)
        ptr1[i] = 0x12341234;

    for (size_t i=0; i<n; i++)
        printf("ptr1 %zu: 0x%x\n", i, ptr1[i]);

    ++n;
    ptr1 = (int *)my_realloc(ptr1, n*sizeof(int));
    printf("\n");
    for (size_t i=0; i<n; i++)
        printf("ptr1 %zu: 0x%x\n", i, ptr1[i]);

    struct chunk* current = chunk_list_tail;
    while (current != NULL) {
        void* data_ptr = (void *)(current + 1);
        DEBUG_PRINT(data_ptr, current);
        current = current->prev;
    }

    return 0;
}
