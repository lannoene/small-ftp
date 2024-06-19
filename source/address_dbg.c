#include "address_dbg.h"

#include <stdio.h>
#include <stdlib.h>

void *DBG_malloc_i(size_t n, int l, const char *f) {
	void *p = malloc(n);
	printf("%p malloc'd size %llu on line %d in file %s\n", p, n, l, f);
	return p;
}
void DBG_free_i(void *p, int l, const char *f) {
	printf("%p free'd on line %d in file %s\n", p, l, f);
	free(p);
}
void *DBG_realloc_i(void *p, size_t n, int l, const char *f) {
	printf("%p realloc'd to ", p);
	void *np = realloc(p, n);
	printf("%p size %llu on line %d in file %s\n", np, n, l, f);
	return np;
}