#include <linux/kernel.h>

int sys_whoami()
{
    printk("I am XXX.");
    return 0;
}

int sys_iam(const char *name)
{
    return 0;
}
