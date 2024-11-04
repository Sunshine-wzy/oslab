/*
 *  linux/mm/memory.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * demand-loading started 01.12.91 - seems it is high on the list of
 * things wanted, and it should be easy to implement. - Linus
 */

/*
 * Ok, demand-loading was easy, shared pages a little bit tricker. Shared
 * pages started 02.12.91, seems to work. - Linus.
 *
 * Tested sharing by executing about 30 /bin/sh: under the old kernel it
 * would have taken more than the 6M I have free, but it worked well as
 * far as I could see.
 *
 * Also corrected some "invalidate()"s - I wasn't doing enough of them.
 */

#include <signal.h>

#include <asm/system.h>

#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>

volatile void do_exit(long code);

static inline volatile void oom(void)
{
	printk("out of memory\n\r");
	do_exit(SIGSEGV);
}

#define invalidate() \
	__asm__("movl %%eax,%%cr3" ::"a"(0))

/* these are not to be changed without changing head.s etc */
#define LOW_MEM 0x100000
#define PAGING_MEMORY (15 * 1024 * 1024)
#define PAGING_PAGES (PAGING_MEMORY >> 12)
#define MAP_NR(addr) (((addr) - LOW_MEM) >> 12)
#define USED 100

#define CODE_SPACE(addr) ((((addr) + 4095) & ~4095) < \
						  current->start_code + current->end_code)

static long HIGH_MEMORY = 0;

#define copy_page(from, to) \
	__asm__("cld ; rep ; movsl" ::"S"(from), "D"(to), "c"(1024))

static unsigned char mem_map[PAGING_PAGES] = {
	0,
};

#define VM_DEV 0x305
#define VM_PAGES (PAGING_PAGES * 2)
#define LOW_VM HIGH_MEMORY
#define HIGH_VM (LOW_VM + (VM_PAGES << 12))
#define MAP_VM(addr) (((addr) - LOW_VM) >> 12)
static unsigned char vm_map[VM_PAGES] = {
	0,
};

// 3 2 1 0: Pending, User, Dirty, Accessed
static unsigned char mem_puda[PAGING_PAGES] = {
	0,
};
static unsigned long clock_ptr = PAGING_PAGES - 1;

unsigned long get_free_vm_blk(int count)
{
	int i;

	if (count < 1)
		return 0;

	for (i = 0; i < VM_PAGES; i++) {
		if (!vm_map[i]) {
			vm_map[i] = count;
			return LOW_VM + (i << 12);
		}
	}

	return 0;
}
void free_vm_blk(unsigned long addr)
{
	if (addr < LOW_VM)
		return;
	if (addr >= HIGH_VM)
		return;
	addr = MAP_VM(addr);
	if (vm_map[addr] == 0)
		panic("trying to free free vm block");
	vm_map[addr]--;
}
#define PRT_FIND_ON_VM 0
#define PRT_SWAP_OUT_PAGE 1
#define PRT_SWAP_IN_PAGE 2
#define PRT_CHOOSE_PAGE 3
static int prt_rec[10] = {0};
int prt_handler(int type)
{
	return prt_rec[type]++;
}
int with_handler(int handler)
{
	return handler < 2;
}
static int persist = 256;

unsigned long swap_out_page(unsigned long page)
{
	int nr[4];
	unsigned long *pg_table;
	unsigned long blk;
	int i, j, count, h;
	count = mem_map[MAP_NR(page)];
	if (count == 0)
		panic("trying to swap out free page");

	blk = get_free_vm_blk(count);
	if (!blk)
		oom();

	mem_puda[MAP_NR(page)] |= 0x8;

	h = prt_handler(PRT_SWAP_OUT_PAGE);
	if (with_handler(h)) {
		printk("[kernel] swap_out_page: page=%x, blk=%x\n", page, blk);
	}
	for (i = 0; i < 4; i++) {
		nr[i] = (blk >> 10) + i;
	}
	i = persist > 8 ? 8 : persist;
	persist -= i;
	bwrite_page(page, VM_DEV, nr);
	sync_dev(VM_DEV);
	persist += i;

	// Walk through all page directory entries
	for (i = 0; i < 1024; i++) {
		if (!(pg_dir[i] & 1)) {
			continue;
		}

		pg_table = (unsigned long *)(0xfffff000 & pg_dir[i]);

		for (j = 0; j < 1024; j++) {
			// 31-12: block number, 11-0: flags
			// 11 10 9: VM ? ?
			// 8 7 6 5 4 3 2 1 0: 0 0 D A 0 0 U/S R/W P
			if ((pg_table[j] & 0xfffff001) == (page | 1)) {
				pg_table[j] = blk | 0x800 | (pg_table[j] & 0xffe);
			}
		}
	}
	mem_map[MAP_NR(page)] = 0;
	mem_puda[MAP_NR(page)] = 0;
	invalidate();
	return blk;
}

void swap_in_page(unsigned long page, unsigned long hd_block)
{
	int nr[4];
	unsigned long *pg_table;
	int i, j, count, h;

	count = vm_map[MAP_VM(hd_block)];
	if (count == 0)
		panic("trying to swap in free vm block");
	mem_map[MAP_NR(page)] = count;
	mem_puda[MAP_NR(page)] = 0x8;

	h = prt_handler(PRT_SWAP_IN_PAGE);
	if (with_handler(h)) {
		printk("[kernel] swap_in_page: page=%x, hd_block=%x\n", page, hd_block);
	}
	for (i = 0; i < 4; i++) {
		nr[i] = (hd_block >> 10) + i;
	}
	i = persist > 8 ? 8 : persist;
	persist -= i;
	bread_page(page, VM_DEV, nr);
	sync_dev(VM_DEV);
	persist += i;
	// Walk through all page directory entries
	for (i = 0; i < 1024; i++) {
		if (!(pg_dir[i] & 1))
			continue;

		pg_table = (unsigned long *)(0xfffff000 & pg_dir[i]);

		for (j = 0; j < 1024; j++) {
			// 31-12: block number, 11-0: flags
			// 11 10 9: VM ? ?
			// 8 7 6 5 4 3 2 1 0: 0 0 D A 0 0 U/S R/W P
			if ((pg_table[j] & 0xfffff800) == (hd_block | 0x800)) {
				// Clear Dirty and Accessed bits
				pg_table[j] = page | 1 | (pg_table[j] & 0xf);
			}
		}
	}
	mem_puda[MAP_NR(page)] = 0x1;
	invalidate();
}
/**
 * Choose a page to swap out
 */
unsigned long clock_algo()
{
	unsigned long *pg_table, *dir;
	unsigned long da, page;
	int i, j, turn, h;

	dir = (unsigned long *)current->tss.cr3;

	// clear user bit
	for (i = 0; i < PAGING_PAGES; i++) {
		mem_puda[i] &= ~0x4;
	}

	// Walk through all page directory entries
	for (i = 0; i < 1024; i++) {
		if (!(dir[i] & 1))
			continue;
		page = (dir[i] & 0xfffff000);
		pg_table = (unsigned long *)(0xfffff000 & dir[i]);
		for (j = 0; j < 1024; j++) {
			if (!(pg_table[j] & 1))
				continue;
			da = (pg_table[j] & 0x60) >> 5; // Get dirty and accessed bits
			pg_table[j] &= ~0x20;			// Clear accessed bit
			page = (pg_table[j] & 0xfffff000);
			if ((i << 22) + (j << 12) >= current->start_code && (i << 22) + (j << 12) < current->start_code + current->brk && current->executable) {
				// Set Used bit
				mem_puda[MAP_NR(page)] |= 0x4;
			}

			mem_puda[MAP_NR(page)] |= da; // Set  DA bits
		}
	}
	j = persist;
	for (i = 0; i < PAGING_PAGES; i++) {
		page = (clock_ptr + PAGING_PAGES - i) % PAGING_PAGES;

		if (mem_map[page] == 0) { // Free page
			if (j-- > 0)
				continue;
			clock_ptr = (page + PAGING_PAGES - 1) % PAGING_PAGES;
			return (page << 12) + LOW_MEM;
		}
	}
	h = prt_handler(PRT_CHOOSE_PAGE);

	turn = 2;
	while (turn--) {
		// First turn: find a page with Used bit and D=0, A=0
		for (i = 0; i < PAGING_PAGES; i++) {
			page = (clock_ptr + PAGING_PAGES - i) % PAGING_PAGES;
			if (mem_puda[page] & 0x8) // pending
				continue;
			if (!(mem_puda[page] & 0x4)) // not user
				continue;

			if ((mem_puda[page] & 0x3) == 0) {
				clock_ptr = (page + PAGING_PAGES - 1) % PAGING_PAGES;
				if (with_handler(h)) {
					printk("[kernel] clock_algo[%d]: page=%x, DA=%x\n", (!turn) * 2 + 1, (page << 12) + LOW_MEM, (mem_puda[page] & 0x3));
				}
				return (page << 12) + LOW_MEM;
			}
		}
		// Second turn: find a page with Used bit and D=1, A=0
		// Clear Accessed bit when accessed
		for (i = 0; i < PAGING_PAGES; i++) {
			page = (clock_ptr + PAGING_PAGES - i) % PAGING_PAGES;
			if (mem_puda[page] & 0x8) // pending
				continue;
			if (!(mem_puda[page] & 0x4)) // not user
				continue;

			if ((mem_puda[page] & 0x3) == 2) {
				clock_ptr = (page + PAGING_PAGES - 1) % PAGING_PAGES;
				if (with_handler(h)) {
					printk("[kernel] clock_algo[%d]: page=%x, DA=%x\n", (!turn) * 2 + 2, (page << 12) + LOW_MEM, (mem_puda[page] & 0x3));
				}
				return (page << 12) + LOW_MEM;
			} else {
				mem_puda[page] &= ~1;
			}
		}
	}
	if (with_handler(h)) {
		for (i = 0; i < 1024; i++) {
			if (!(dir[i] & 1))
				continue;
			page = (dir[i] & 0xfffff000);
			pg_table = (unsigned long *)(0xfffff000 & dir[i]);
			for (j = 0; j < 1024; j++) {
				if (!(pg_table[j] & 1))
					continue;
				page = (pg_table[j] & 0xfffff000);
				if ((i << 22) + (j << 12) >= current->start_code && (i << 22) + (j << 12) < current->start_code + current->brk && current->executable) {
					printk("[kernel] clock_algo: page=%x, addr=%x, uda=%x\n", page, (i << 22) + (j << 12), (mem_puda[MAP_NR(page)]));
				}
			}
		}
		printk("[kernel] clock_algo: no page found\n");
	}
	return 0;
}
/**
 * Find a page in the virtual memory block
 *
 * `addr` is the virtual address of the page
 */
int find_on_vm(unsigned long v_addr)
{
	unsigned long *pg_table;
	unsigned long page, blk;
	int h;

	page = (v_addr >> 22) & 0x3ff;
	if (!(pg_dir[page] & 1))
		return 0;
	pg_table = (unsigned long *)(0xfffff000 & pg_dir[page]);

	page = (v_addr >> 12) & 0x3ff;
	if (!(pg_table[page] & 0x800))
		return 0;

	blk = pg_table[page] & 0xfffff000;

	h = prt_handler(PRT_FIND_ON_VM);
	if (with_handler(h)) {
		printk("[kernel] find_on_vm: v_addr=%x, blk=%x\n", v_addr, blk);
	}
	page = clock_algo();
	if (!page)
		oom();
	if (mem_map[MAP_NR(page)])
		swap_out_page(page);

	swap_in_page(page, blk);
	return 1;
}

/*
 * Get physical address of first (actually last :-) free page, and mark it
 * used. If no free pages left, return 0.
 */
unsigned long get_free_page(void)
{
	// register unsigned long __res asm("ax");

	// __asm__("std ; repne ; scasb\n\t"
	// 	"jne 1f\n\t"
	// 	"movb $1,1(%%edi)\n\t"
	// 	"sall $12,%%ecx\n\t"
	// 	"addl %2,%%ecx\n\t"
	// 	"movl %%ecx,%%edx\n\t"
	// 	"movl $1024,%%ecx\n\t"
	// 	"leal 4092(%%edx),%%edi\n\t"
	// 	"rep ; stosl\n\t"
	// 	"movl %%edx,%%eax\n"
	// 	"1:"
	// 	:"=a" (__res)
	// 	:"0" (0),"i" (LOW_MEM),"c" (PAGING_PAGES),
	// 	"D" (mem_map+PAGING_PAGES-1)
	// 	);
	// return __res;

	unsigned long page;
	int i;

	page = clock_algo();
	if (!page)
		return 0;
	if (mem_map[MAP_NR(page)])
		swap_out_page(page);

	mem_map[MAP_NR(page)] = 1;
	mem_puda[MAP_NR(page)] = 0x1;

	// Clear page
	for (i = 0; i < (1 << 12); i++) {
		((char *)page)[i] = 0;
	}

	return page;
}

/*
 * Free a page of memory at physical address 'addr'. Used by
 * 'free_page_tables()'
 */
void free_page(unsigned long addr)
{
	if (addr < LOW_MEM) return;
	if (addr >= HIGH_MEMORY)
		panic("trying to free nonexistent page");
	addr -= LOW_MEM;
	addr >>= 12;
	if (mem_map[addr]--) return;
	mem_map[addr]=0;
	panic("trying to free free page");
}

/*
 * This function frees a continuos block of page tables, as needed
 * by 'exit()'. As does copy_page_tables(), this handles only 4Mb blocks.
 */
int free_page_tables(unsigned long from,unsigned long size)
{
	unsigned long *pg_table;
	unsigned long * dir, nr;
	int i, last;

	if (from & 0x3fffff)
		panic("free_page_tables called with wrong alignment");
	if (!from)
		panic("Trying to free up swapper memory space");
	size = (size + 0x3fffff) >> 22;
	dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */
	for ( ; size-->0 ; dir++) {
		if (!(1 & *dir))
			continue;
		pg_table = (unsigned long *) (0xfffff000 & *dir);
		for (nr=0 ; nr<1024 ; nr++) {
			if (1 & *pg_table)
				free_page(0xfffff000 & *pg_table);
			else if (0x800 & *pg_table)
				free_vm_blk(0xfffff000 & *pg_table);
			*pg_table = 0;
			pg_table++;
		}
		free_page(0xfffff000 & *dir);
		*dir = 0;
	}
	invalidate();
	return 0;
}

/*
 *  Well, here is one of the most complicated functions in mm. It
 * copies a range of linerar addresses by copying only the pages.
 * Let's hope this is bug-free, 'cause this one I don't want to debug :-)
 *
 * Note! We don't copy just any chunks of memory - addresses have to
 * be divisible by 4Mb (one page-directory entry), as this makes the
 * function easier. It's used only by fork anyway.
 *
 * NOTE 2!! When from==0 we are copying kernel space for the first
 * fork(). Then we DONT want to copy a full page-directory entry, as
 * that would lead to some serious memory waste - we just copy the
 * first 160 pages - 640kB. Even that is more than we need, but it
 * doesn't take any more memory - we don't copy-on-write in the low
 * 1 Mb-range, so the pages can be shared with the kernel. Thus the
 * special case for nr=xxxx.
 */
int copy_page_tables(unsigned long from,unsigned long to,long size)
{
	unsigned long * from_page_table;
	unsigned long * to_page_table;
	unsigned long this_page;
	unsigned long * from_dir, * to_dir;
	unsigned long nr;

	if ((from&0x3fffff) || (to&0x3fffff))
		panic("copy_page_tables called with wrong alignment");
	from_dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */
	to_dir = (unsigned long *) ((to>>20) & 0xffc);
	size = ((unsigned) (size+0x3fffff)) >> 22;
	for( ; size-->0 ; from_dir++,to_dir++) {
		if (1 & *to_dir)
			panic("copy_page_tables: already exist");
		if (!(1 & *from_dir))
			continue;
		from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
		if (!(to_page_table = (unsigned long *) get_free_page()))
			return -1;	/* Out of memory, see freeing */
		*to_dir = ((unsigned long) to_page_table) | 7;
		nr = (from==0)?0xA0:1024;
		for ( ; nr-- > 0 ; from_page_table++,to_page_table++) {
			this_page = *from_page_table;
			if (!(1 & this_page))
				continue;
			this_page &= ~2;
			*to_page_table = this_page;
			if (this_page > LOW_MEM) {
				*from_page_table = this_page;
				this_page -= LOW_MEM;
				this_page >>= 12;
				mem_map[this_page]++;
			}
		}
	}
	invalidate();
	return 0;
}

/*
 * This function puts a page in memory at the wanted address.
 * It returns the physical address of the page gotten, 0 if
 * out of memory (either when trying to access page-table or
 * page.)
 */
unsigned long put_page(unsigned long page,unsigned long address)
{
	unsigned long tmp, *page_table;

/* NOTE !!! This uses the fact that _pg_dir=0 */

	if (page < LOW_MEM || page >= HIGH_MEMORY)
		printk("Trying to put page %p at %p\n",page,address);
	if (mem_map[(page-LOW_MEM)>>12] != 1)
		printk("mem_map disagrees with %p at %p\n",page,address);
	page_table = (unsigned long *) ((address>>20) & 0xffc);
	if ((*page_table)&1)
		page_table = (unsigned long *) (0xfffff000 & *page_table);
	else {
		if (!(tmp=get_free_page()))
			return 0;
		*page_table = tmp|7;
		page_table = (unsigned long *) tmp;
	}
	page_table[(address>>12) & 0x3ff] = page | 7;
/* no need for invalidate */
	return page;
}

void un_wp_page(unsigned long * table_entry)
{
	unsigned long old_page,new_page;

	old_page = 0xfffff000 & *table_entry;
	if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)]==1) {
		*table_entry |= 2;
		invalidate();
		return;
	}
	if (!(new_page=get_free_page()))
		oom();
	if (old_page >= LOW_MEM)
		mem_map[MAP_NR(old_page)]--;
	*table_entry = new_page | 7;
	invalidate();
	copy_page(old_page,new_page);
}	

/*
 * This routine handles present pages, when users try to write
 * to a shared page. It is done by copying the page to a new address
 * and decrementing the shared-page counter for the old page.
 *
 * If it's in code space we exit with a segment error.
 */
void do_wp_page(unsigned long error_code,unsigned long address)
{
#if 0
/* we cannot do this yet: the estdio library writes to code space */
/* stupid, stupid. I really want the libc.a from GNU */
	if (CODE_SPACE(address))
		do_exit(SIGSEGV);
#endif
	un_wp_page((unsigned long *)
		(((address>>10) & 0xffc) + (0xfffff000 &
		*((unsigned long *) ((address>>20) &0xffc)))));

}

void write_verify(unsigned long address)
{
	unsigned long page;

	if (!( (page = *((unsigned long *) ((address>>20) & 0xffc)) )&1))
		return;
	page &= 0xfffff000;
	page += ((address>>10) & 0xffc);
	if ((3 & *(unsigned long *) page) == 1)  /* non-writeable, present */
		un_wp_page((unsigned long *) page);
	return;
}

void get_empty_page(unsigned long address)
{
	unsigned long tmp;

	if (!(tmp = get_free_page()) || !put_page(tmp, address)) {
		free_page(tmp);		/* 0 is ok - ignored */
		oom();
	}
}

/*
 * try_to_share() checks the page at address "address" in the task "p",
 * to see if it exists, and if it is clean. If so, share it with the current
 * task.
 *
 * NOTE! This assumes we have checked that p != current, and that they
 * share the same executable.
 */
static int try_to_share(unsigned long address, struct task_struct * p)
{
	unsigned long from;
	unsigned long to;
	unsigned long from_page;
	unsigned long to_page;
	unsigned long phys_addr;

	from_page = to_page = ((address>>20) & 0xffc);
	from_page += ((p->start_code>>20) & 0xffc);
	to_page += ((current->start_code>>20) & 0xffc);
/* is there a page-directory at from? */
	from = *(unsigned long *) from_page;
	if (!(from & 1))
		return 0;
	from &= 0xfffff000;
	from_page = from + ((address>>10) & 0xffc);
	phys_addr = *(unsigned long *) from_page;
/* is the page clean and present? */
	if ((phys_addr & 0x41) != 0x01)
		return 0;
	phys_addr &= 0xfffff000;
	if (phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM)
		return 0;
	to = *(unsigned long *) to_page;
	if (!(to & 1)) {
		if ((to = get_free_page()))
			*(unsigned long *) to_page = to | 7;
		else
			oom();
	}
	to &= 0xfffff000;
	to_page = to + ((address>>10) & 0xffc);
	if (1 & *(unsigned long *) to_page)
		panic("try_to_share: to_page already exists");
/* share them: write-protect */
	*(unsigned long *) from_page &= ~2;
	*(unsigned long *) to_page = *(unsigned long *) from_page;
	invalidate();
	phys_addr -= LOW_MEM;
	phys_addr >>= 12;
	mem_map[phys_addr]++;
	return 1;
}

/*
 * share_page() tries to find a process that could share a page with
 * the current one. Address is the address of the wanted page relative
 * to the current data space.
 *
 * We first check if it is at all feasible by checking executable->i_count.
 * It should be >1 if there are other tasks sharing this inode.
 */
static int share_page(unsigned long address)
{
	struct task_struct ** p;

	if (!current->executable)
		return 0;
	if (current->executable->i_count < 2)
		return 0;
	for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
		if (!*p)
			continue;
		if (current == *p)
			continue;
		if ((*p)->executable != current->executable)
			continue;
		if (try_to_share(address,*p))
			return 1;
	}
	return 0;
}

void do_no_page(unsigned long error_code,unsigned long address)
{
	int nr[4];
	unsigned long tmp;
	unsigned long page;
	int block,i;

	address &= 0xfffff000;
	if (find_on_vm(address))
		return;
	tmp = address - current->start_code;
	if (!current->executable || tmp >= current->end_data) {
		get_empty_page(address);
		return;
	}
	if (share_page(tmp))
		return;
	if (!(page = get_free_page()))
		oom();
/* remember that 1 block is used for header */
	block = 1 + tmp/BLOCK_SIZE;
	for (i=0 ; i<4 ; block++,i++)
		nr[i] = bmap(current->executable,block);
	bread_page(page,current->executable->i_dev,nr);
	i = tmp + 4096 - current->end_data;
	tmp = page + 4096;
	while (i-- > 0) {
		tmp--;
		*(char *)tmp = 0;
	}
	if (put_page(page,address))
		return;
	free_page(page);
	oom();
}

void mem_init(long start_mem, long end_mem)
{
	int i;

	HIGH_MEMORY = end_mem;
	for (i=0 ; i<PAGING_PAGES ; i++)
		mem_map[i] = USED;
	i = MAP_NR(start_mem);
	end_mem -= start_mem;
	end_mem >>= 12;
	while (end_mem-->0)
		mem_map[i++]=0;
}

void calc_mem(void)
{
	int i,j,k,free=0;
	long * pg_tbl;

	for(i=0 ; i<PAGING_PAGES ; i++)
		if (!mem_map[i]) free++;
	printk("%d pages free (of %d)\n\r",free,PAGING_PAGES);
	for(i=2 ; i<1024 ; i++) {
		if (1&pg_dir[i]) {
			pg_tbl=(long *) (0xfffff000 & pg_dir[i]);
			for(j=k=0 ; j<1024 ; j++)
				if (pg_tbl[j]&1)
					k++;
			printk("Pg-dir[%d] uses %d pages\n",i,k);
		}
	}
}
