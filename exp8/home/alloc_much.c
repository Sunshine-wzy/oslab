#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
	int i, j;
	int size;
	char *p0, *p1;
	{
		printf("alloc_much: allocating 8MB memory\n");
		size = 8 * 1024 * 1024;
		p0 = malloc(size);
		for (j = 0; j < size; j += 4096) {
			p0[j] = 'a';
		}
		printf("alloc_much: 8MB from %p to %p\n", p0, p0 + size);
	}
	{
		printf("alloc_much: allocating extra 2.5MB memory\n");
		size = 2 * 1024 * 1024 + 512 * 1024;
		p1 = malloc(size);
		for (j = 0; j < size; j += 4096) {
			p1[j] = 'b';
		}
	}
	printf("alloc_much: finished (%c %c)\n", p0[0], p1[0]);
	return 0;
}
