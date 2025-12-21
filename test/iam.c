#define __LIBRARY__
#include <unistd.h>
#include <stdio.h>

_syscall1(int,iam,const char *,name);

int main(int argc, char **argv)
{
    int len;
    if (argc < 2) return 1;

    len = iam(argv[1]);

    if (len == -1)
    {
        printf("System call failed!\n");
        return 1;
    }

    return 0;
}