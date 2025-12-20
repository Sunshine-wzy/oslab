#define __LIBRARY__
#include <unistd.h>

_syscall0(int,whoami)
_syscall1(int,iam,const char *,name)
