#include <stdint.h>
#include <stddef.h>

extern void printk(const char *fmt, ...);

extern uint64_t pml4_table[];
extern uint64_t pdpt_table[];
extern uint64_t page_directory_low[];
extern uint64_t page_directory_high[];

#define PAGE_SIZE 4096
#define MAX_PAGES 32768
#define BITMAP_SIZE (MAX_PAGES / 64)

static uint64_t bitmap[BITMAP_SIZE];
static uint64_t total_pages = 0;
static uint64_t used_pages = 0;
static uint64_t memory_start = 0x200000;

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_RW       (1ULL << 1)
#define PAGE_HUGE     (1ULL << 7)

void paging_map_2mb(uint64_t phys_addr, uint64_t virt_addr) {
    uint64_t pdpt_idx = (virt_addr >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt_addr >> 21) & 0x1FF;

    uint64_t *pd;
    if (pdpt_idx == 0) pd = page_directory_low;
    else if (pdpt_idx == 3) pd = page_directory_high;
    else {
        printk("paging: can't map virt 0x%X (PDPT[%d] not wired)\n", virt_addr, pdpt_idx);
        return;
    }

    pd[pd_idx] = (phys_addr & ~0x1FFFFF) | PAGE_PRESENT | PAGE_RW | PAGE_HUGE;

    __asm__ volatile ("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

void mem_init(uint64_t total_mem) {
    total_pages = total_mem / PAGE_SIZE;
    if (total_pages > MAX_PAGES) total_pages = MAX_PAGES;
    
    for (int i = 0; i < BITMAP_SIZE; i++) {
        bitmap[i] = 0;
    }
    
    for (uint64_t i = 0; i < memory_start / PAGE_SIZE; i++) {
        uint64_t idx = i / 64;
        uint64_t bit = i % 64;
        bitmap[idx] |= (1ULL << bit);
        used_pages++;
    }

    for (uint64_t phys = 0x200000; phys < total_mem; phys += 0x200000) {
        paging_map_2mb(phys, phys);
    }
    
    printk("Memory: %d pages (%d MB) total, %d used\n", 
           total_pages, total_pages * 4 / 1024, used_pages);
}

void *page_alloc() {
    for (uint64_t i = 0; i < total_pages; i++) {
        uint64_t idx = i / 64;
        uint64_t bit = i % 64;
        
        if (!(bitmap[idx] & (1ULL << bit))) {
            bitmap[idx] |= (1ULL << bit);
            used_pages++;
            return (void*)(memory_start + i * PAGE_SIZE);
        }
    }
    return NULL;
}

void page_free(void *ptr) {
    uint64_t addr = (uint64_t)ptr;
    if (addr < memory_start) return;
    
    uint64_t page_idx = (addr - memory_start) / PAGE_SIZE;
    uint64_t idx = page_idx / 64;
    uint64_t bit = page_idx % 64;
    
    bitmap[idx] &= ~(1ULL << bit);
    used_pages--;
}
