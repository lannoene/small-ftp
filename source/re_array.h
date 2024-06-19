#pragma once

#include <stddef.h>

#define NEW_RE_ARRAY_TYPE(type, name) \
typedef struct { \
	size_t length; \
	type *array;\
} ReArray_ ## name;

#define NEW_RE_ARRAY(type, name)\
NEW_RE_ARRAY_TYPE(type, name)\
ReArray_ ## name name;

#define NEW_RE_ARRAY_AUTO(type, name, len)\
NEW_RE_ARRAY_TYPE(type, name)\
ReArray_ ## name name = {len, len > 0 ? malloc(len*sizeof(type)) : malloc(1)};

#define RE_ARRAY_ADD_LEN(name, newLen)\
name.length += newLen;\
name.array = realloc(name.array, sizeof(typeof(name.array[0]))*name.length);

#define free_RE_ARRAY(name)\
free(name.array);

#define RE_BACK(name)\
name.array[name.length - 1]

#define RE_INSERT(name, value)\
RE_ARRAY_ADD_LEN(name, 1)\
	RE_BACK(name) = value;

#define FOR_EACH_START(arrayName, varName)\
for (size_t __i__ = 0; __i__ < arrayName.length; ++__i__) {\
	typeof(arrayName.array[0]) varName = arrayName.array[__i__];

#define FOR_EACH_END(arrayName, varName) \
	arrayName.array[__i__] = varName;\
}
