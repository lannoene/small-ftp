#include "queue.h"

#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>

#include "dir.h"
#include "address_dbg.h"

static _Atomic size_t length;
static struct QueueElement *elems;

pthread_mutex_t queueAccessMutex;

bool InitQueue(void) {
	length = 0;
	elems = malloc(1);
	if (!elems) {
		puts("Could not malloc queue elems");
		return false;
	}
	pthread_mutex_init(&queueAccessMutex, NULL);
	return true;
}

void freeQueue(void) {
	free(elems);
}

bool AddFileToQueue(const char *path) {
	pthread_mutex_lock(&queueAccessMutex);
	elems = realloc(elems, sizeof(struct QueueElement)*++length);
	
	struct QueueElement *newElem = &elems[length - 1];
	newElem->path = strdup(path);
	struct stat st;
	if (!FileExists(path))
		goto addFileToQueue_onError;
    if (stat(path, &st) == 0) {
		newElem->size = st.st_size;
	} else
		goto addFileToQueue_onError;
	pthread_mutex_unlock(&queueAccessMutex);
	return true;
addFileToQueue_onError:
	pthread_mutex_unlock(&queueAccessMutex);
	return false;
}

struct QueueElement PopFromQueue(void) {
	pthread_mutex_lock(&queueAccessMutex);
	struct QueueElement elem = elems[0];
	for (int i = 0; i < length - 1; i++) {
		elems[i] = elems[i + 1];
	}
	elems = realloc(elems, sizeof(struct QueueElement)*(length > 0 ? --length : 0));
	pthread_mutex_unlock(&queueAccessMutex);
	return elem;
}

struct QueueElement *GetQueueElement(size_t i) {
	if (true || i > length) // unsafe and should not be used.
		return NULL;
	return &elems[i];
}

size_t GetQueueLength(void) {
	return length;
}