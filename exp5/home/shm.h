#define __LIBRARY__

#include <unistd.h>

_syscall2(int, CreateSharedMemory, int, size, unsigned long *, p_addr)
_syscall2(int, ShareMemoryWith, int, pid, unsigned long *, p_addr)
