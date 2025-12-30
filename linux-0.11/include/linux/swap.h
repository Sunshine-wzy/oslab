#ifndef _SWAP_H
#define _SWAP_H

/* Swap device configuration */
#define SWAP_DEV 0x0300         /* /dev/hd1 (slave drive, device 3:0) */
#define SWAP_BITMAP_SIZE 1024   /* 1024 bytes = 8192 slots */
#define MAX_SWAP_PAGES (SWAP_BITMAP_SIZE * 8)  /* 8192 pages = 32MB */

/* Debug flag */
#define SWAP_DEBUG 1

#if SWAP_DEBUG
#define SWAP_LOG(fmt, ...) printk("SWAP: " fmt, ##__VA_ARGS__)
#else
#define SWAP_LOG(fmt, ...)
#endif

/* Swap statistics */
struct swap_stat {
	unsigned long total_slots;      /* Total swap slots available */
	unsigned long free_slots;       /* Free swap slots */
	unsigned long swap_in_count;    /* Number of swap-ins */
	unsigned long swap_out_count;   /* Number of swap-outs */
	unsigned long clock_scan_count; /* Number of clock scans */
};

extern struct swap_stat swap_stat;
extern unsigned char *swap_bitmap;

/* Swap subsystem functions */
void swap_init(void);
int swap_out_page(unsigned long page_addr, unsigned long *pte);
int swap_in_page(unsigned long swap_slot, unsigned long address);
unsigned long alloc_swap_slot(void);
void free_swap_slot(unsigned long slot);
void inc_swap_slot(unsigned long slot);

/* Low-level page I/O */
void ll_rw_page(int rw, unsigned long page_addr, int dev, unsigned long block);

#endif
