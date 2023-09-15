#include <stdlib.h>
#include <stdio.h>
#include "mem.h"


void *xmalloc(size_t siz) {
	void *p;
	p = malloc(siz);
	if (!p) {
		perror("malloc: ");
		exit(1);
	}
	return p;
}
