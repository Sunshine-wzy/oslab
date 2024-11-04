#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
	int i, j;
	int size;
	char *p;
	{
		printf("alloc_much: allocating 8MB memory\n");
		size = 8 * 1024 * 1024;
		p = malloc(size);
		for (j = 0; j < size; j += 4096) {
			p[j] = 0;
		}
		printf("alloc_much: 8MB from %p to %p\n", p, p + size);
	}
	{
		printf("alloc_much: allocating extra 2.5MB memory\n");
		size = 2 * 1024 * 1024 + 512 * 1024;
		p = malloc(size);
		for (j = 0; j < size; j += 4096) {
			p[j] = 0;
		}
	}
	printf("alloc_much: finished\n");
	return 0;
}
