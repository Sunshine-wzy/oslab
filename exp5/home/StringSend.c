#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shm.h"

#define PAGE_SIZE 4096

int main(int argc, char *argv[])
{
    int sid;
    char *baseaddr, *curraddr;
    char c;
    char *string;

    if (argc < 3)
    {
        printf("Usage: ./client <pid> <message>\n");
        return 1;
    }

    sid = atoi(argv[1]);
    string = argv[2];
    printf("string: %s\n", string);

    if (ShareMemoryWith(sid, &baseaddr) == 0)
    {
        curraddr = baseaddr;
        c = *string;
        while (c != '$')
        {
            if (c == '#')
                c = ' ';
            *curraddr = c; /*curraddr是当前的逻辑地址，向这个参数写入数据才能
                             保证StringSend xx1 Hello$后再执行StringSend xx1
                             OS#world$后，写入的是Hello OS world，而没有被覆盖
                             掉。实际上确切的说是HelloOS world，想一想怎么办？*/
            string++;
            curraddr++;
            if (curraddr - baseaddr >= PAGE_SIZE)
                curraddr -= PAGE_SIZE;
            c = *string;
        }
        printf("Message sent to PID %d\n", sid);
    }
    else
    {
        printf("ShareMemoryWith failed! Check PID or if Server is running.\n");
    }

    return 0;
}
