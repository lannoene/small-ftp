#pragma once

#include <stddef.h>

#define DBG_malloc(n) DBG_malloc_i(n, __LINE__, __FILE__)
#define DBG_free(p) DBG_free_i(p, __LINE__, __FILE__)
#define DBG_realloc(p, n) DBG_realloc_i(p, n, __LINE__, __FILE__)

void *DBG_malloc_i(size_t n, int l, const char *f);
void DBG_free_i(void *p, int l, const char *f);
void *DBG_realloc_i(void *p, size_t n, int l, const char *f);