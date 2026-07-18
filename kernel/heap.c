#include "heap.h"
#include <stdint.h>
#include <stddef.h>

extern void *page_alloc(void);
extern void page_free(void *ptr);
extern void printk(const char *fmt, ...);

#define HEAP_START  0x1000000
#define HEAP_PAGES  32
#define HEAP_SIZE   (HEAP_PAGES * 4096)

typedef struct block {
    size_t size;
    uint8_t free;
    struct block *next;
} block_t;

static block_t *heap_head = 0;
static uint8_t heap_initialized = 0;

void heap_init(void) {
    uint8_t *base = 0;
    for (uint32_t i = 0; i < HEAP_PAGES; i++) {
        uint8_t *page = (uint8_t *)page_alloc();
        if (!page) {
            printk("heap: failed to allocate page %d\n", i);
            return;
        }
        if (i == 0) base = page;
    }

    heap_head = (block_t *)base;
    heap_head->size = HEAP_SIZE - sizeof(block_t);
    heap_head->free = 1;
    heap_head->next = 0;
    heap_initialized = 1;
    printk("heap: initialized at 0x%x, %d bytes\n", (uint64_t)base, HEAP_SIZE);
}

static block_t *find_free(size_t size) {
    block_t *cur = heap_head;
    while (cur) {
        if (cur->free && cur->size >= size) return cur;
        cur = cur->next;
    }
    return 0;
}

static block_t *split_block(block_t *block, size_t size) {
    if (block->size >= size + sizeof(block_t) + 16) {
        block_t *new_block = (block_t *)((uint8_t *)block + sizeof(block_t) + size);
        new_block->size = block->size - size - sizeof(block_t);
        new_block->free = 1;
        new_block->next = block->next;
        block->next = new_block;
        block->size = size;
    }
    return block;
}

void *malloc(size_t size) {
    if (!heap_initialized || size == 0) return 0;

    size = (size + 7) & ~7;

    block_t *block = find_free(size);
    if (!block) return 0;

    block->free = 0;
    split_block(block, size);
    return (void *)((uint8_t *)block + sizeof(block_t));
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) {
        uint8_t *p = (uint8_t *)ptr;
        for (size_t i = 0; i < total; i++) p[i] = 0;
    }
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return 0; }

    block_t *block = (block_t *)((uint8_t *)ptr - sizeof(block_t));
    if (block->size >= size) return ptr;

    void *new_ptr = malloc(size);
    if (new_ptr) {
        uint8_t *src = (uint8_t *)ptr;
        uint8_t *dst = (uint8_t *)new_ptr;
        for (size_t i = 0; i < block->size; i++) dst[i] = src[i];
        free(ptr);
    }
    return new_ptr;
}

void free(void *ptr) {
    if (!ptr || !heap_initialized) return;

    block_t *block = (block_t *)((uint8_t *)ptr - sizeof(block_t));
    block->free = 1;

    block_t *cur = heap_head;
    while (cur) {
        if (cur->free && cur->next && cur->next->free) {
            cur->size += sizeof(block_t) + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}
