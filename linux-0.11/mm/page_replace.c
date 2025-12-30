/*
 *  linux/mm/page_replace.c
 *
 *  Clock page replacement algorithm implementation
 */

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/swap.h>
#include <asm/system.h>

/* Memory thresholds */
#define FREE_PAGE_CRITICAL  32   /* Emergency: block allocations */
#define FREE_PAGE_LOW_WATER 128  /* Start page-out */
#define FREE_PAGE_HIGH_WATER 256 /* Stop page-out */

/* Clock algorithm parameters */
#define PAGES_PER_SCAN 64  /* Pages to scan per clock_hand1 invocation */

/* External declarations */
extern struct task_struct *task[];
extern unsigned long nr_free_pages;
extern struct task_struct *wait_for_free_page;

/* Clock algorithm state */
struct clock_state {
	unsigned long hand1_position;  /* Access bit clearer */
	unsigned long hand2_position;  /* Victim selector */
	unsigned long scan_rounds;
	unsigned long pages_scanned;
} clock_state = {0, 0, 0, 0};

/*
 * find_pte_for_page - Find PTE mapping a physical page
 * page_idx: Index in mem_map[] (0-based)
 * Returns: Pointer to PTE, or NULL if not found
 *
 * This performs a reverse page table lookup by scanning all processes
 */
static unsigned long *find_pte_for_page(unsigned long page_idx)
{
	unsigned long phys_addr = (page_idx << 12) + LOW_MEM;
	struct task_struct *p;
	int i, pde_idx, pte_idx;
	unsigned long *pg_dir, *pg_table;
	unsigned long pde, pte;

	/* Scan all processes */
	for (i = 0; i < NR_TASKS; i++) {
		p = task[i];
		if (!p)
			continue;

		/* Get page directory for this process */
		pg_dir = (unsigned long *)((p->tss.cr3) & 0xfffff000);

		/* Scan all page directory entries */
		for (pde_idx = 0; pde_idx < 1024; pde_idx++) {
			pde = pg_dir[pde_idx];
			if (!(pde & PAGE_PRESENT))
				continue;

			pg_table = (unsigned long *)(pde & 0xfffff000);

			/* Scan all page table entries */
			for (pte_idx = 0; pte_idx < 1024; pte_idx++) {
				pte = pg_table[pte_idx];
				if (!(pte & PAGE_PRESENT))
					continue;

				if ((pte & 0xfffff000) == phys_addr) {
					/* Found it! */
					return &pg_table[pte_idx];
				}
			}
		}
	}

	return NULL;  /* Not found */
}

/*
 * clock_hand1_scan - Periodically clear accessed bits
 * Called every 100ms from timer interrupt
 */
void clock_hand1_scan(void)
{
	int scanned = 0;
	unsigned long page_idx;
	unsigned long *pte;

	/* Scan PAGES_PER_SCAN pages */
	while (scanned < PAGES_PER_SCAN) {
		page_idx = clock_state.hand1_position;

		/* Skip free or reserved pages */
		if (mem_map[page_idx] == 0 || mem_map[page_idx] == USED) {
			clock_state.hand1_position = (clock_state.hand1_position + 1) % PAGING_PAGES;
			scanned++;
			continue;
		}

		/* Find PTE mapping this page */
		pte = find_pte_for_page(page_idx);
		if (pte && (*pte & PAGE_ACCESSED)) {
			/* Clear accessed bit */
			*pte &= ~PAGE_ACCESSED;
			invalidate();
		}

		/* Advance hand1 (circular) */
		clock_state.hand1_position = (clock_state.hand1_position + 1) % PAGING_PAGES;
		scanned++;
	}

	clock_state.pages_scanned += scanned;
}

/*
 * clock_select_victim - Select a page to evict
 * Returns: Physical address of victim page, or 0 if none found
 */
unsigned long clock_select_victim(void)
{
	int pass = 0;
	int scanned = 0;
	int max_scan = PAGING_PAGES * 2;
	unsigned long page_idx;
	unsigned long *pte;
	int accessed, dirty;

	while (scanned < max_scan) {
		page_idx = clock_state.hand2_position;

		/* Skip free or reserved pages */
		if (mem_map[page_idx] == 0 || mem_map[page_idx] == USED) {
			clock_state.hand2_position = (clock_state.hand2_position + 1) % PAGING_PAGES;
			scanned++;
			continue;
		}

		/* Find PTE mapping this page */
		pte = find_pte_for_page(page_idx);
		if (!pte || !(*pte & PAGE_PRESENT)) {
			clock_state.hand2_position = (clock_state.hand2_position + 1) % PAGING_PAGES;
			scanned++;
			continue;
		}

		accessed = (*pte & PAGE_ACCESSED) ? 1 : 0;
		dirty = (*pte & PAGE_DIRTY) ? 1 : 0;

		/* Pass 0: Look for clean, unaccessed pages (best victim) */
		if (pass == 0 && !accessed && !dirty) {
			unsigned long victim = (page_idx << 12) + LOW_MEM;
			clock_state.hand2_position = (clock_state.hand2_position + 1) % PAGING_PAGES;
			return victim;
		}

		/* Pass 1: Look for dirty, unaccessed pages (good victim) */
		if (pass == 1 && !accessed) {
			unsigned long victim = (page_idx << 12) + LOW_MEM;
			clock_state.hand2_position = (clock_state.hand2_position + 1) % PAGING_PAGES;
			return victim;
		}

		/* Clear accessed bit and give second chance */
		if (accessed) {
			*pte &= ~PAGE_ACCESSED;
			invalidate();
		}

		clock_state.hand2_position = (clock_state.hand2_position + 1) % PAGING_PAGES;
		scanned++;

		/* After full scan, increase pass number */
		if (scanned >= PAGING_PAGES && pass < 2) {
			pass++;
			scanned = 0;
		}
	}

	/* Fallback: take current position (shouldn't happen) */
	page_idx = clock_state.hand2_position;
	if (mem_map[page_idx] != 0 && mem_map[page_idx] != USED) {
		unsigned long victim = (page_idx << 12) + LOW_MEM;
		clock_state.hand2_position = (clock_state.hand2_position + 1) % PAGING_PAGES;
		return victim;
	}

	return 0;  /* No victim found */
}

/*
 * try_free_pages - Attempt to free pages by swapping out
 * count: Target number of pages to free
 * Returns: Number of pages actually freed
 */
int try_free_pages(int count)
{
	int freed = 0;
	int attempts = 0;
	int max_attempts = count * 4;
	unsigned long victim_page;
	unsigned long *pte;
	int dirty;

	while (freed < count && attempts < max_attempts) {
		attempts++;

		/* Select victim page */
		victim_page = clock_select_victim();
		if (!victim_page) {
			SWAP_LOG("No victim page found after %d attempts\n", attempts);
			break;
		}

		/* Find PTE for this page */
		pte = find_pte_for_page(MAP_NR(victim_page));
		if (!pte) {
			SWAP_LOG("No PTE found for victim page %p\n", victim_page);
			continue;
		}

		/* Check if dirty */
		dirty = (*pte & PAGE_DIRTY) ? 1 : 0;

		if (dirty) {
			/* Dirty page - must swap out */
			if (swap_out_page(victim_page, pte) == 0) {
				freed++;
				SWAP_LOG("Swapped out dirty page %p\n", victim_page);
			} else {
				SWAP_LOG("Failed to swap out page %p\n", victim_page);
			}
		} else {
			/* Clean page - just discard */
			*pte = 0;  /* Mark as not present */
			invalidate();
			free_page(victim_page);
			freed++;
			SWAP_LOG("Discarded clean page %p\n", victim_page);
		}
	}

	if (freed > 0) {
		clock_state.scan_rounds++;
	}

	return freed;
}

/*
 * count_free_pages - Count free pages in mem_map
 * Returns: Number of free pages
 */
unsigned long count_free_pages(void)
{
	int i;
	unsigned long free = 0;

	for (i = 0; i < PAGING_PAGES; i++) {
		if (mem_map[i] == 0)
			free++;
	}

	return free;
}

/*
 * check_memory_pressure - Check memory status and trigger page-out if needed
 * Called from get_free_page() when memory is low
 */
void check_memory_pressure(void)
{
	unsigned long free;
	int freed;

	/* Count free pages */
	free = count_free_pages();
	nr_free_pages = free;

	if (free < FREE_PAGE_CRITICAL) {
		/* Critical pressure: aggressive page-out */
		printk("CRITICAL: Only %lu free pages, aggressive page-out\n", free);
		freed = try_free_pages(64);
		printk("Freed %d pages (aggressive)\n", freed);
	} else if (free < FREE_PAGE_LOW_WATER) {
		/* Low pressure: moderate page-out */
		SWAP_LOG("Low memory: %lu free pages, moderate page-out\n", free);
		freed = try_free_pages(32);
		SWAP_LOG("Freed %d pages (moderate)\n", freed);
	}

	/* Update cached count */
	nr_free_pages = count_free_pages();

	/* Wake up processes waiting for memory */
	if (nr_free_pages > FREE_PAGE_CRITICAL) {
		wake_up(&wait_for_free_page);
	}
}
