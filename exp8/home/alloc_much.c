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
	}
	{
		printf("alloc_much: allocating extra 5MB memory\n");
		size = 5 * 1024 * 1024;
		p = malloc(size);
		for (j = 0; j < size; j += 4096) {
			p[j] = 0;
		}
	}
	return 0;
}
