#include <stdio.h>
#include <string.h>
#include "shm.h"

#define PAGE_SIZE 4096

int main(int argc, char *argv[])
{
    char *baseaddr, *curraddr;
    char c;

    if (argc > 1 && strcmp(argv[1], "startup") == 0)
    {
        printf("------------------The Server Processor is Startup---------------\n");
        printf("The server processor's id = %d\n", getpid());
        fflush(stdout);

        if (CreateSharedMemory(1, &baseaddr) == 0)
        {
            curraddr = baseaddr;
            
            while (1)
            {
                printf("The server will sleep 60\n");
                fflush(stdout);
                sleep(60);

                c = *curraddr;
                while (c && c != '$') /* 用$表示字符串的结束 */
                {
                    printf("%c", c);
                    fflush(stdout);
                    curraddr++;
                    if (curraddr - baseaddr >= PAGE_SIZE)
                        curraddr -= PAGE_SIZE;
                    /* 引入baseaddr的目的是判断是否超过了共享内存的边界 */
                    c = *curraddr;
                }
            }
        }
        else
        {
            printf("CreateSharedMemory failed!\n");
        }
    }
    else
    {
        printf("Usage: ./server startup\n");
    }
    return 0;
}
