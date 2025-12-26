#define __LIBRARY__
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

_syscall2(int, sem_open, const char *, name, int, value)
_syscall1(int, sem_wait, int, sem_id)
_syscall1(int, sem_post, int, sem_id)
_syscall1(int, sem_unlink, const char *, name)

#define BUFFER_SIZE 10
#define NUM_PRODUCERS 1
#define NUM_CONSUMERS 1
#define NUM_ITEMS 20

const char *SEM_MUTEX = "mutex";
const char *SEM_FULL  = "full";
const char *SEM_EMPTY = "empty";

int main() {
    char i, j;
    int pid;
    int sem_mutex, sem_full, sem_empty;
    int fd;
    char data;
    char in = 0, out = 0;
    int status;

    /* 初始化共享缓冲区文件 */
    /* 文件格式: [in指针(4字节)] [out指针(4字节)] [数据区...] */
    fd = open("buffer.dat", O_CREAT | O_TRUNC | O_RDWR, 0666);
    lseek(fd, 0, SEEK_SET);
    write(fd, &in, sizeof(int));
    write(fd, &out, sizeof(int));
    lseek(fd, BUFFER_SIZE * sizeof(int) + 2 * sizeof(int), SEEK_SET);
    write(fd, &out, sizeof(int));
    close(fd);

    sem_mutex = sem_open(SEM_MUTEX, 1);
    sem_empty = sem_open(SEM_EMPTY, BUFFER_SIZE);
    sem_full  = sem_open(SEM_FULL, 0);

    printf("Semaphores created: mutex=%d, empty=%d, full=%d\n", sem_mutex, sem_empty, sem_full);
    fflush(stdout);

    /* 创建消费者进程 */
    for(i = 0; i < NUM_CONSUMERS; i++) {
        if((pid = fork()) == 0) {
            for(j = 0; j < NUM_ITEMS; j++) {
                sem_wait(sem_full);
                sem_wait(sem_mutex);

                fd = open("buffer.dat", O_RDWR, 0);
                
                lseek(fd, sizeof(int), SEEK_SET);
                read(fd, &out, sizeof(int));
                
                lseek(fd, 2 * sizeof(int) + out * sizeof(int), SEEK_SET);
                read(fd, &data, sizeof(int));
                
                printf("%d: Consumer %d consumes item %d from buffer %d\n", getpid(), i, data, out);
                fflush(stdout);

                out = (out + 1) % BUFFER_SIZE;
                lseek(fd, sizeof(int), SEEK_SET);
                write(fd, &out, sizeof(int));
                
                close(fd);

                sem_post(sem_mutex);
                sem_post(sem_empty);
            }
            exit(0);
        }
    }

    /* 创建生产者进程 */
    for(i = 0; i < NUM_PRODUCERS; i++) {
        if((pid = fork()) == 0) {
            for(j = 0; j < NUM_ITEMS; j++) {
                sem_wait(sem_empty);
                sem_wait(sem_mutex);

                fd = open("buffer.dat", O_RDWR, 0);
                
                lseek(fd, 0, SEEK_SET);
                read(fd, &in, sizeof(int));
                
                lseek(fd, 2 * sizeof(int) + in * sizeof(int), SEEK_SET);
                write(fd, &j, sizeof(int));
                
                printf("%d: Producer %d produces item %d to buffer %d\n", getpid(), i, j, in);
                fflush(stdout);

                in = (in + 1) % BUFFER_SIZE;
                lseek(fd, 0, SEEK_SET);
                write(fd, &in, sizeof(int));
                
                close(fd);

                sem_post(sem_mutex);
                sem_post(sem_full);
            }
            exit(0);
        }
    }

    /* 父进程等待子进程结束 */
    while(wait(&status) > 0);

    /* 清理信号量 */
    sem_unlink(SEM_MUTEX);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_FULL);
    printf("Main process finished.\n");
    return 0;
}