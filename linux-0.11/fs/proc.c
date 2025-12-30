#include <errno.h>
#include <stdarg.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <sys/types.h>
#include <asm/segment.h>

typedef int (*proc_ptr)(char * buf, int count, off_t * pos);

static int get_exec_name(struct task_struct *p, char *name)
{
	int i;
	char *basename = NULL;
	struct m_inode *inode = NULL;
	struct buffer_head *bh = NULL;
	struct dir_entry *de = NULL;

	if (p->pid < 0) {
		return -1;
	}

	inode = p->executable;
	if (inode == NULL) {
		return -1;
	}

	bh = bread(inode->i_dev, inode->i_zone[0]);
	if (bh == NULL) {
		return -1;
	}

	de = (struct dir_entry *)bh->b_data;
	for (i = 0; i < inode->i_size / sizeof(struct dir_entry); i++, de++) {
		if (de->inode == inode->i_num) {
			basename = de->name;
			break;
		}
	}

	if (basename == NULL) {
		return -1;
	}

	strcpy(name, basename);
	return 0;
}

static char *state_str(struct task_struct *p)
{
    switch (p->state) {
        case TASK_RUNNING:
            return current->pid == p->pid ? "running" : "ready";
        case TASK_INTERRUPTIBLE:
        case TASK_UNINTERRUPTIBLE:
            return "waiting";
        case TASK_ZOMBIE:
        case TASK_STOPPED:
            return "stopped";
        default:
            return "-";
    }
}

static char name_buf[256];

static int psinfo_proc_read(char *buf, int count, off_t *pos)
{
    struct task_struct **p, *curr_p;
    int i, j;
    int len = 0;
    int chars_to_copy;
    char *k_pos; /* 指向内核缓冲区当前要读取的位置 */
    
    /* 获得一段空闲内存 */
    unsigned long krnbuf = get_free_page();
    if (!krnbuf) return -ENOMEM;
    
    /* 进程列表 */
    len += sprintf((char *)krnbuf + len, "Processes List:\n");
    len += sprintf((char *)krnbuf + len, "pid\texec\tstate\tuid\tfather-id\tstart_time\n");
    
    for (i = 0; i < NR_TASKS; i++) {
        p = &task[i];
        if (*p) {
            if (len > 3000) break;
            
            if (get_exec_name(*p, name_buf))
                sprintf(name_buf, "-");
            
            len += sprintf((char *)krnbuf + len, "%ld\t%s\t%s\t%d\t%ld\t%ld\n",
                (*p)->pid,
                name_buf,
                state_str(*p),
                (*p)->uid,
                (*p)->father,
                (*p)->start_time
            );
        }
    }
    
    /* 当前进程的详细信息 */
    curr_p = current;
    len += sprintf((char *)krnbuf + len, "\nDetail of Process ID=%d\n", curr_p->pid);

    /* 1. 目录信息 */
    if (curr_p->pwd) {
        len += sprintf((char *)krnbuf + len, "Current Directory inode: %d\n", curr_p->pwd->i_num);
    }
    if (curr_p->root) {
        len += sprintf((char *)krnbuf + len, "Root Directory inode:    %d\n", curr_p->root->i_num);
    }

    /* 2. 打开的文件 */
    len += sprintf((char *)krnbuf + len, "Open Files:\n");
    for (j = 0; j < NR_OPEN; j++) {
        if (curr_p->filp[j] && curr_p->filp[j]->f_inode) {
            len += sprintf((char *)krnbuf + len, "  FD(%d): inode=%d \n",
                j,
                curr_p->filp[j]->f_inode->i_num
            );
        }
    }

    /* 3. LDT 信息 */
    len += sprintf((char *)krnbuf + len, "LDT Information:\n");
    len += sprintf((char *)krnbuf + len, "  LDT(1) Code: Base=0x%08lx, Limit=0x%05lx\n",
        get_base(curr_p->ldt[1]),
        (curr_p->ldt[1].a & 0xffff) | (curr_p->ldt[1].b & 0xf0000)
    );
    len += sprintf((char *)krnbuf + len, "  LDT(2) Data: Base=0x%08lx, Limit=0x%05lx\n",
        get_base(curr_p->ldt[2]),
        (curr_p->ldt[2].a & 0xffff) | (curr_p->ldt[2].b & 0xf0000)
    );
    
    /* 数据拷贝与偏移处理 */
    if (*pos >= len) {
        free_page(krnbuf);
        return 0;
    }

    chars_to_copy = count;
    if (chars_to_copy > len - *pos) {
        chars_to_copy = len - *pos;
    }

    k_pos = (char *)krnbuf + *pos;
    for (i = 0; i < chars_to_copy; i++) {
        put_fs_byte(k_pos[i], buf + i);
    }

    *pos += chars_to_copy;
    free_page(krnbuf);

    /* 返回实际读取的字节数 */
    return chars_to_copy;
}

#define NRPROCS ((sizeof (proc_table))/(sizeof (proc_ptr)))

static proc_ptr proc_table[]={
	NULL,
	NULL,
	NULL,
	psinfo_proc_read,
	NULL};

int proc_read(int dev, char *buf, int count, off_t *pos)
{
    proc_ptr call_addr;

	if (MINOR(dev)>=NRPROCS)
		return -ENODEV;
	if (!(call_addr=proc_table[MINOR(dev)]))
		return -ENODEV;
	return call_addr(buf,count,pos);
}
