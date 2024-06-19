#pragma once

#include <stdbool.h>
#include <stddef.h>

struct QueueElement {
	const char *path;
	size_t size;
};

bool InitQueue(void);
bool AddFileToQueue(const char *path);
size_t GetQueueLength(void);
struct QueueElement PopFromQueue(void);