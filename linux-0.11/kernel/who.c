#include <linux/kernel.h>
#include <asm/segment.h>
#include <errno.h>
#include <string.h>

char myname[21];

int sys_whoami(char *name, unsigned int size)
{
    int i, len = strlen(myname);

    if (size < len + 1) return -EINVAL;

    for (i = 0; i <= len; i++)
    {
        put_fs_byte(myname[i], name + i);
    }

    return len;   
}

int sys_iam(const char *name)
{
    int i;
    char c, tmp[22];
    
    for (i = 0; i < sizeof(myname); i++)
    {
        c = get_fs_byte(name + i);
        if (c == '\0') break;
        tmp[i] = c;
    }

    if (i >= sizeof(myname))
        return -EINVAL; 

    tmp[i] = '\0';
    strcpy(myname, tmp);

    return i;
}
