#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

#define SEM_NAME_LEN 20
#define SEM_MAX_NUM 20

typedef struct semaphore
{
    char name[SEM_NAME_LEN];
    int value;
    struct task_struct *queue;
    int occupied;
} sem_t;

sem_t semaphores[SEM_MAX_NUM];

int sys_sem_open(const char *name, int value)
{
    char k_name[SEM_NAME_LEN];
    char c;
    int i, len;
    int empty_slot = -1;

    len = 0;
    while ((c = get_fs_byte(name + len)) != '\0' && len < SEM_NAME_LEN - 1)
    {
        k_name[len] = c;
        len++;
    }
    k_name[len] = '\0';

    for (i = 0; i < SEM_MAX_NUM; i++)
    {
        if (semaphores[i].occupied == 1)
        {
            int j = 0;
            while (k_name[j] == semaphores[i].name[j] && k_name[j] != '\0')
                j++;
            if (k_name[j] == '\0' && semaphores[i].name[j] == '\0')
            {
                return i;
            }
        }
        else if (empty_slot == -1)
        {
            empty_slot = i;
        }
    }

    if (empty_slot == -1)
    {
        printk("Error: No semaphores available.\n");
        return -1;
    }

    semaphores[empty_slot].occupied = 1;
    semaphores[empty_slot].value = value;
    semaphores[empty_slot].queue = NULL;

    for (i = 0; i <= len; i++)
        semaphores[empty_slot].name[i] = k_name[i];

    return empty_slot;
}

int sys_sem_wait(int sem_id)
{
    if (sem_id < 0 || sem_id >= SEM_MAX_NUM || semaphores[sem_id].occupied == 0)
        return -1;

    cli();
    semaphores[sem_id].value--;
    if (semaphores[sem_id].value < 0)
    {
        sleep_on(&(semaphores[sem_id].queue));
    }
    sti();
    return 0;
}

int sys_sem_post(int sem_id)
{
    if (sem_id < 0 || sem_id >= SEM_MAX_NUM || semaphores[sem_id].occupied == 0)
        return -1;

    cli();
    semaphores[sem_id].value++;
    if (semaphores[sem_id].value <= 0)
    {
        wake_up(&(semaphores[sem_id].queue));
    }
    sti();
    return 0;
}

int sys_sem_unlink(const char *name)
{
    char k_name[SEM_NAME_LEN];
    char c;
    int i, len;

    len = 0;
    while ((c = get_fs_byte(name + len)) != '\0' && len < SEM_NAME_LEN - 1)
    {
        k_name[len] = c;
        len++;
    }
    k_name[len] = '\0';

    for (i = 0; i < SEM_MAX_NUM; i++)
    {
        if (semaphores[i].occupied == 1)
        {
            int j = 0;
            while (k_name[j] == semaphores[i].name[j] && k_name[j] != '\0')
                j++;
            if (k_name[j] == '\0' && semaphores[i].name[j] == '\0')
            {
                semaphores[i].occupied = 0;
                semaphores[i].queue = NULL;
                return 0;
            }
        }
    }
    return -1;
}