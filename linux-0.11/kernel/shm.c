#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/segment.h>

#define MAX_ARG_PAGES 32

int sys_CreateSharedMemory(int size, unsigned long *p_addr)
{
    unsigned long page, address, data_base, data_limit = 0x4000000;
    int i;

    if (size > PAGE_SIZE)
        return -EINVAL;

    /* 计算线性地址和逻辑地址 */
    data_base = get_base(current->ldt[2]);
    data_base += data_limit;
    address = data_limit; /* address存放逻辑地址，即数据段内的偏移 */
    for (i = MAX_ARG_PAGES; i >= 0; i--)
    {
        data_base -= PAGE_SIZE;
        address -= PAGE_SIZE;
    }
    /* 减去了MAX_ARG_PAGES后，就找到了一个空闲的虚页，显然data_base为该虚页的
    线性地址，而address为虚页的逻辑地址，p_addr就用来返回该值。*/

    /* 物理内存分配 */
    page = get_free_page();
    if (!page) return -ENOMEM;

    /* 记录到 PCB */
    current->sharepage = page;
    /* 建立映射 */
    put_page(page, data_base);
    /* 返回逻辑地址 */
    put_fs_long(address, p_addr);
    
    /* 唤醒等待该共享页的进程 */
    if (current->waitpage)
    {
        wake_up(&current->waitpage);
    }
    return 0;
}

int sys_ShareMemoryWith(int pid, unsigned long *p_addr)
{
    struct task_struct *p = NULL;
    unsigned long address, data_base, data_limit = 0x4000000;
    int i;

    /* 查找目标进程 */
    for (i = 0; i < NR_TASKS; i++)
    {
        if (task[i] && task[i]->pid == pid)
        {
            p = task[i];
            break;
        }
    }
    if (!p) return -ESRCH;

    /* 等待目标进程创建共享内存 */
    while (p->sharepage == 0)
    {
        sleep_on(&p->waitpage);
    }
    
    /* 在当前进程的数据段中划出虚页 */
    data_base = get_base(current->ldt[2]);
    data_base += data_limit;
    address = data_limit; /* address存放逻辑地址，即数据段内的偏移 */
    for (i = MAX_ARG_PAGES; i >= 0; i--)
    {
        data_base -= PAGE_SIZE;
        address -= PAGE_SIZE;
    }
    /* 减去了MAX_ARG_PAGES后，就找到了一个空闲的虚页，显然data_base为该虚页的
    线性地址，而address为虚页的逻辑地址，p_addr就用来返回该值。*/

    /* 使用目标进程的物理页，映射到当前进程的线性地址 */
    put_page(p->sharepage, data_base);
    /* 返回逻辑地址 */
    put_fs_long(address, p_addr);
    return 0;
}
