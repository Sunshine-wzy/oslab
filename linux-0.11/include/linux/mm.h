#ifndef _MM_H
#define _MM_H

#define PAGE_SIZE 4096

/* Memory management constants */
#define LOW_MEM 0x100000
#define PAGING_MEMORY (15*1024*1024)
#define PAGING_PAGES (PAGING_MEMORY>>12)
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)
#define USED 100

/* TLB invalidation macro */
#define invalidate() \
__asm__("movl %%eax,%%cr3"::"a" (0))

/* Page table entry bits */
#define PAGE_PRESENT    0x001
#define PAGE_RW         0x002
#define PAGE_USER       0x004
#define PAGE_ACCESSED   0x020
#define PAGE_DIRTY      0x040
#define PAGE_SWAPPED    0x200  /* Bit 9: page is in swap */

/* Page table entry manipulation for swapped pages */
#define PTE_IS_PRESENT(pte)     ((pte) & PAGE_PRESENT)
#define PTE_IS_SWAPPED(pte)     (((pte) & (PAGE_PRESENT | PAGE_SWAPPED)) == PAGE_SWAPPED)
#define PTE_GET_SWAP_SLOT(pte)  ((pte) >> 12)
#define PTE_MAKE_SWAPPED(slot)  (((slot) << 12) | PAGE_SWAPPED)

/* External variables */
extern unsigned char mem_map[];

/* Function prototypes */
extern unsigned long get_free_page(void);
extern unsigned long put_page(unsigned long page,unsigned long address);
extern void free_page(unsigned long addr);
extern int swap_in_page(unsigned long swap_slot, unsigned long address);
extern void check_memory_pressure(void);

#endif
