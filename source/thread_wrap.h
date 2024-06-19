#pragma once

#include <pthread.h>

void PthreadSpawnFunc(pthread_t *ptid, void *funcd, int numArgs, ...);

extern pthread_t thread_temp; // thread_wrap.c