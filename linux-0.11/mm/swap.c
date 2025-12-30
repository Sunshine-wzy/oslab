/*
 *  linux/mm/swap.c
 *
 *  Swap device management and page I/O operations
 */

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/swap.h>
#include <asm/system.h>

/* Swap statistics */
struct swap_stat swap_stat = {0, 0, 0, 0, 0};

/* Swap bitmap - reference counted byte array */
unsigned char *swap_bitmap = NULL;

/*
 * swap_init - Initialize swap subsystem
 * Called during kernel initialization
 */
void swap_init(void)
{
	unsigned long test_page;
	int i;

	printk("Initializing swap device %04x...\n", SWAP_DEV);

	/* Allocate bitmap page */
	swap_bitmap = (unsigned char *)get_free_page();
	if (!swap_bitmap) {
		printk("WARNING: Cannot allocate swap bitmap, no swap available\n");
		return;
	}

	/* Initialize all slots as free (0 = free, >0 = reference count) */
	for (i = 0; i < SWAP_BITMAP_SIZE; i++) {
		swap_bitmap[i] = 0;
	}

	/* Initialize statistics */
	swap_stat.total_slots = MAX_SWAP_PAGES;
	swap_stat.free_slots = MAX_SWAP_PAGES;
	swap_stat.swap_in_count = 0;
	swap_stat.swap_out_count = 0;
	swap_stat.clock_scan_count = 0;

	printk("Swap device ready: %d KB available (%ld slots)\n",
		(MAX_SWAP_PAGES * 4), swap_stat.total_slots);
}

/*
 * alloc_swap_slot - Allocate a swap slot
 * Returns: slot number (>0) or 0 if no free slots
 */
unsigned long alloc_swap_slot(void)
{
	unsigned long i;

	if (!swap_bitmap)
		return 0;

	/* Find first free slot */
	for (i = 0; i < MAX_SWAP_PAGES; i++) {
		if (swap_bitmap[i] == 0) {
			swap_bitmap[i] = 1;  /* Mark as used */
			swap_stat.free_slots--;
			return i + 1;  /* Return 1-based slot number */
		}
	}

	printk("SWAP: No free swap slots available\n");
	return 0;  /* No free slots */
}

/*
 * free_swap_slot - Free a swap slot
 * slot: 1-based slot number
 */
void free_swap_slot(unsigned long slot)
{
	if (!swap_bitmap)
		return;

	if (slot == 0 || slot > MAX_SWAP_PAGES) {
		printk("SWAP: Invalid slot number %lu\n", slot);
		return;
	}

	slot--;  /* Convert to 0-based index */

	if (swap_bitmap[slot] > 0) {
		swap_bitmap[slot]--;
		if (swap_bitmap[slot] == 0) {
			swap_stat.free_slots++;
		}
	} else {
		printk("SWAP: Trying to free already free slot %lu\n", slot + 1);
	}
}

/*
 * inc_swap_slot - Increment reference count for shared pages
 * slot: 1-based slot number
 */
void inc_swap_slot(unsigned long slot)
{
	if (!swap_bitmap)
		return;

	if (slot == 0 || slot > MAX_SWAP_PAGES)
		return;

	slot--;  /* Convert to 0-based index */

	if (swap_bitmap[slot] < 255)
		swap_bitmap[slot]++;
}

/*
 * swap_out_page - Write a page to swap device
 * page_addr: physical address of page
 * pte: pointer to page table entry
 * Returns: 0 on success, -1 on failure
 */
int swap_out_page(unsigned long page_addr, unsigned long *pte)
{
	unsigned long slot;
	unsigned long block;

	if (!swap_bitmap) {
		printk("SWAP: Swap not initialized\n");
		return -1;
	}

	/* Allocate swap slot */
	slot = alloc_swap_slot();
	if (!slot) {
		printk("SWAP: No free slots for swap-out\n");
		return -1;
	}

	/* Calculate block number on swap device (slot-1 because slot is 1-based) */
	/* Each page = 8 sectors (512 bytes each = 4096 total) */
	block = (slot - 1) * 8;

	SWAP_LOG("Swapping out page %p to slot %lu (block %lu)\n",
		page_addr, slot, block);

	/* Write page to swap device */
	ll_rw_page(WRITE, page_addr, SWAP_DEV, block);

	/* Update PTE to mark as swapped */
	*pte = PTE_MAKE_SWAPPED(slot);
	invalidate();  /* Flush TLB */

	/* Free physical page */
	free_page(page_addr);

	/* Update statistics */
	swap_stat.swap_out_count++;

	return 0;
}

/*
 * swap_in_page - Read a page from swap device
 * swap_slot: 1-based slot number from PTE
 * address: virtual address to map page to
 * Returns: 0 on success, -1 on failure
 */
int swap_in_page(unsigned long swap_slot, unsigned long address)
{
	unsigned long page;
	unsigned long block;

	if (!swap_bitmap) {
		printk("SWAP: Swap not initialized\n");
		return -1;
	}

	if (swap_slot == 0 || swap_slot > MAX_SWAP_PAGES) {
		printk("SWAP: Invalid swap slot %lu\n", swap_slot);
		return -1;
	}

	SWAP_LOG("Swapping in slot %lu to address %p\n", swap_slot, address);

	/* Allocate physical page */
	page = get_free_page();
	if (!page) {
		printk("SWAP: Cannot allocate page for swap-in\n");
		return -1;
	}

	/* Calculate block number (slot is 1-based) */
	block = (swap_slot - 1) * 8;

	/* Read from swap device */
	ll_rw_page(READ, page, SWAP_DEV, block);

	/* Install page in page table */
	if (!put_page(page, address)) {
		printk("SWAP: put_page failed for address %p\n", address);
		free_page(page);
		return -1;
	}

	/* Free swap slot */
	free_swap_slot(swap_slot);

	/* Update statistics */
	swap_stat.swap_in_count++;

	return 0;
}
