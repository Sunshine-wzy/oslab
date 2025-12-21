#define __LIBRARY__
#include <unistd.h>
#include <stdio.h>

_syscall2(int,whoami,char *,name,unsigned int,size);

int main()
{
    char myname[22];
    int len = whoami(myname, 128);

    if (len == -1)
    {
        printf("System call failed!\n");
        return 1;
    }

    printf("I am %s\n", myname);

    return 0;
}