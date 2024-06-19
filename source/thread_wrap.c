#include "thread_wrap.h"

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>

#include "address_dbg.h"

struct pThreadArgs {
	int numArgs;
	void **argList;
	void *funcp;
};

pthread_t thread_temp; // throwaway addr for threads

void PthreadDecodeArgs(struct pThreadArgs *args) {
	switch (args->numArgs) {
		default:
			printf("arg count too high (max 4): %d\n", args->numArgs);
			exit(1);
			break;
		case 0: {
			void (*func)() = args->funcp;
			func();
			break;
		}
		case 1: {
			void (*func)(void*) = args->funcp;
			func(args->argList[0]);
			break;
		}
		case 2: {
			void (*func)(void*, void*) = args->funcp;
			func(args->argList[0], args->argList[1]);
			break;
		}
		case 3: {
			void (*func)(void*, void*, void*) = args->funcp;
			func(args->argList[0], args->argList[1], args->argList[2]);
			break;
		}
		case 4: {
			void (*func)(void*, void*, void*, void*) = args->funcp;
			func(args->argList[0], args->argList[1], args->argList[2], args->argList[3]);
			break;
		}
	}
	free(args->argList);
	free(args);
}

void PthreadSpawnFunc(pthread_t *ptid, void *funcd, int numArgs, ...) {
	va_list args;
	
	va_start(args, numArgs);
	
	struct pThreadArgs *argStruct = malloc(sizeof(struct pThreadArgs));
	argStruct->argList = malloc(numArgs*sizeof(void*));
	argStruct->numArgs = numArgs;
	argStruct->funcp = funcd;
	
	for (int i = 0; i < numArgs; i++) {
		argStruct->argList[i] = va_arg(args, void*);
	}
	
	va_end(args);
	
	pthread_create(ptid, NULL, (void*)PthreadDecodeArgs, argStruct);
}