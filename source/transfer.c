#include "transfer.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>

#ifdef _WIN32
//#include "openat.h"
#endif

#include "net.h"
#include "dir.h"
#include "conio.h"
#include "thread_wrap.h"
#include "queue.h"
#include "sdlwrp.h"

#include "address_dbg.h"

bool SendFileSockIdx(const char *filename, bool force, int sockIdx) {
	//printf("%d: In send file func\n", sockIdx);
	FILE *file = fopen(filename, "rb");
	if (file == NULL) {
		char *dir = getcwd(NULL, 0);
		printf("Could not open file '%s'. Consider checking the filename and rentering the information.\n", filename);
		free(dir);
		printf("Error: %s\n", strerror(errno));
		return false;
	}
	uint64_t fileSize = FileGetSize(filename);
	// allocate buffer for file data
	char *fileData = malloc(fileSize);
	if (fileData == NULL) {
		puts("COULD NOT ALLOCATE FILE DATA!!!!!");
		fileSize = 0;
	}
	fread(fileData, fileSize, 1, file);
	//puts("Finished reading file");
	struct FileInfo fileInfo = {fileSize};
	strcpy(fileInfo.filename, filename);
	fileInfo.forceDownload = force;
	SendNetMsgIdx(FILE_HEADER, &fileInfo, sizeof(struct FileInfo), sockIdx); // send file header
	
	bool sentFile = false;
	while (true) { // wait for file head acke
		char buffer[STATIC_BUF_SIZE];
		if (ListenForNetMsgsIdx(buffer, sockIdx)) {
			if (((struct NetMsg*)buffer)->type == FILE_REQ_ACK) {
				if (((struct NetMsg*)buffer)->data[0] == 1) {
					//puts("Sending file.");
					// send file data
					sentFile = true;
					SendNetMsgIdx(FILE_DATA, fileData, fileSize, sockIdx);
					//puts("Finished sending file data");
				}
				break;
			} else {
				//printf("Unkown packet type: %d\n", ((struct NetMsg*)buffer)->type);
			}
		}
	}
	
	//puts("Finished sending file.");
	
	free(fileData);
	fclose(file); // weird bug log:
	/* 
	 * when testing on one machine, 
	 * closing the file will wait for any write operations to finish,
	 * halting the program. THE ISSUE... what if they're synchronous?
	 * in my case, the file is manually being written to by the receiving end,
	 * causing a write operation on the sending end too (because it's one machine).
	 * this means fclose will take until the operation is done, by design.
	 * :(. i mean it makes sense... but i just wish it was cooler.
	 */
	
	//printf("%d: almost finished send file func\n", sockIdx);
	
	struct timeval tm1, tm2;
	gettimeofday(&tm1, NULL);
	if (sentFile)
		WaitForOKIdx(sockIdx);
	//printf("%d: completely finished send file func\n", sockIdx);
	gettimeofday(&tm2, NULL);
	//printf("okay wait took %f seconds\n", ((tm2.tv_sec - tm1.tv_sec)*1000000 + (tm2.tv_usec - tm1.tv_usec))/1000000.f);
	return sentFile;
}

bool SendFile(const char *filename, bool force) {
	return SendFileSockIdx(filename, force, 0);
}

void WriteFileF(struct WriteFileArgs *args) {
	FILE *file = fopen(args->filename, "wb");
	fwrite(args->data, args->size, 1, file);
	fclose(file);
	free(args->data);
}

static struct {
	char filename[MAX_FILENAME];
	float percent;
	bool done;
} fileCount[MAX_NTS] = {};

_Atomic int filesDownloaded = 0;

bool ReceiveFileIdx(char *fileBuffer, uint64_t fileSize, int sockIdx) {
	uint64_t receivedBytes = 0;
	int64_t receivedFileData = -NETHEADER_SIZE; // skip netheader
	
	int lastGivenProgress = -1;
	while (receivedFileData < (int64_t)fileSize) {
		char newBuffer[STATIC_BUF_SIZE];
		uint64_t bytesRead;
		if ((bytesRead = ListenForNetMsgsIdx(newBuffer, sockIdx))) { // if we're in debt to the packet header, we're gonna have to pay it off with our received data
			if (receivedFileData < 0) {
				memcpy(fileBuffer + CLAMP(0, receivedFileData, STATIC_BUF_SIZE), newBuffer + CLAMP(0, -receivedFileData, STATIC_BUF_SIZE), CLAMP(0, bytesRead - NETHEADER_SIZE, STATIC_BUF_SIZE));
				receivedBytes += bytesRead;
				receivedFileData += bytesRead;
			} else { // otherwise, the received stuff is just raw data :)
				memcpy(fileBuffer + receivedFileData, newBuffer, bytesRead);
				receivedBytes += bytesRead;
				receivedFileData += bytesRead;
			}
			float progress = CLAMP(0, ((float)receivedFileData/fileSize)*100, fileSize);
			if ((int)progress*4 % (1) == 0 && (int)progress != lastGivenProgress) {
				//printf("%.2f%% done\n", progress);
				fileCount[sockIdx].percent = progress;
				//fprintf(stderr, "%d: progress: %d\n", sockIdx, (int)progress);
				lastGivenProgress = (int)progress;
			}
		}
	}
	
	//printf("\n");
	
	return true;
}

bool ReceiveFile(char *fileBuffer, uint64_t fileSize) {
	bool b = ReceiveFileIdx(fileBuffer, fileSize, 0);
	fileCount->done = true; // dumb hack.
	/*
	 * this works because if i call receive file (as opposed to receivefileidx)
	 * i can reasonably assume that there aren't any other threads working
	 * so i can set the file count to done after just 1 transfer
	 */
	return b;
}

pthread_mutex_t downloadDrawMutex = PTHREAD_MUTEX_INITIALIZER;

#define BAR_MAX_WIDTH 500

void DrawDownloadProgress(void) {
	for (int i = 0; i < GetNTS(); i++) {
		Wrp_DrawRectangle(10, 20 + 30*i, BAR_MAX_WIDTH*(fileCount[i].percent/100.f), 25, CLR_GRN);
		Wrp_DrawRectangleNoFill(10, 20 + 30*i, BAR_MAX_WIDTH, 25, CLR_BLK);
		Wrp_DrawText(fileCount[i].filename, 10, 20 + 30*i, 22);
	}
}

int numFilesTotal = 0;
_Atomic int numFilesSent = 0;
char filenames[MAX_NTS][MAX_FILENAME];

pthread_mutex_t uploadDrawMutex;

void DrawUploadProgress(void) {
	Wrp_DrawRectangle(10, 20 + 30, BAR_MAX_WIDTH*(numFilesSent/(float)numFilesTotal), 25, CLR_GRN);
	Wrp_DrawRectangleNoFill(10, 20 + 30, BAR_MAX_WIDTH, 25, CLR_BLK);
	Wrp_DrawTextF("%.2f% Done", 10, 20 + 30, 22, (numFilesSent/(float)numFilesTotal)*100);
	
	for (int i = 0; i < GetNTS(); i++) {
		Wrp_DrawText(fileCount[i].filename, 10, 100 + 30*i, 22);
	}
	//
}

bool forceDownloadNextFile = true; // temp set to true. og false
bool hasSelectedAll = true; // temp set to true. og false
int minAvailableFileIndex = 0;

pthread_mutex_t fileTransferStartMutex;
pthread_mutex_t anyInputMutex; // pause for any inputs
pthread_mutex_t detachThreadMutex;

void SendQueueFiles(int *socketIndex) {
	while (true) {
		// mutex lock while checking exit conditions
		pthread_mutex_lock(&fileTransferStartMutex);
		if (GetQueueLength() == 0) {
			pthread_mutex_unlock(&fileTransferStartMutex);
			break;
		} else {
			//printf("Queue len: %lld\n", GetQueueLength());
		}
		struct QueueElement elem = PopFromQueue();
		strncpy(fileCount[*socketIndex].filename, GetFilenameFromPath(elem.path), MAX_FILENAME);
		
		pthread_mutex_unlock(&fileTransferStartMutex);
		
		//printf("Sending: %s #%d\n", elem.path, ++numFilesSent);
		
		SendFileSockIdx(elem.path, true, *socketIndex);
		++numFilesSent;
		fileCount[*socketIndex].filename[*socketIndex] = '\0';
	}
	//printf("Exited func %d\n", *socketIndex);
	SendNetCmdIdx(END_DIR_SENDING, *socketIndex);
	fileCount[*socketIndex].done = true;
}

void ReceiveQueue(int *sockIdx) {
	//printf("In recv dir: %d\n", *sockIdx);
	
	//printf("Fd origional: %d\n", curDir.fd);
	//printf("\n\n\n");
	bool exit = false;
	while (!exit) {
		char buffer[STATIC_BUF_SIZE];
		uint64_t bytesRecv = 0;
		while ((bytesRecv = ListenForNetMsgsIdx(buffer, *sockIdx))) {
			if (bytesRecv == -1) { // on error
				exit = true;
				break;
			}
			enum packet_type type = ((struct NetMsg*)buffer)->type;
			char *packData = ((struct NetMsg*)buffer)->data;
			//uint64_t packSize = ((struct NetMsg*)buffer)->size;
			switch (type) {
				default:
					printf("Receiving dir but got unknown packet type: %d\n", type);
					break;
				case END_DIR_SENDING:
					fileCount[*sockIdx].done = true;
					return;
				case FILE_HEADER:
					// get relevant data out of packet
					uint64_t fileSize = ((struct FileInfo*)(packData))->size;
					char filename[100];
					strncpy(fileCount[*sockIdx].filename, GetFilenameFromPath(((struct FileInfo*)(packData))->filename), MAX_FILENAME);
					strncpy(filename, ((struct FileInfo*)(packData))->filename, 100);
					//printf("Downloading file:\nSize: %lld\nName: %s\n", fileSize, filename);
					// tell peer that we got their request and we're okay with it
					if (!FileExists(filename) || ((struct FileInfo*)(packData))->forceDownload) {
						char e = 1;
						SendNetMsgIdx(FILE_REQ_ACK, &e, 1, *sockIdx);
						char *fileBuffer = malloc(fileSize);
						// receive file
						if (ReceiveFileIdx(fileBuffer, fileSize, *sockIdx)) { // if file receive did not time out, save it
							//fprintf(stderr, "Finished receiving file %s\n", ((struct FileInfo*)(packData))->filename);
							uint64_t *size = malloc(sizeof(uint64_t));
							*size = fileSize;
							char *newFilename = malloc(strlen(((struct FileInfo*)(packData))->filename) + 1);
							strcpy(newFilename, ((struct FileInfo*)(packData))->filename);
							PthreadSpawnFunc(&thread_temp, WriteFileT, 3, newFilename, fileBuffer, size);
							fileCount[*sockIdx].filename[0] = '\0';
							++filesDownloaded;
						} else {
							puts("An error has occured! Could not receive file :(");
							free(fileBuffer);
						}
						//puts("Reached end of recvdir func");
						SendNetCmdIdx(OK, *sockIdx);
					} else {
						char e = 0;
						SendNetMsgIdx(FILE_REQ_ACK, &e, 1, *sockIdx);
					}
					break;
				case CHDIR: // should go unused
					if (!DirExists(packData))
						CreateDir(packData);
					chdir(packData);
					SendNetCmdIdx(OK, *sockIdx);
					break;
			}
		}
	}
}


void ChangeDirectoryRemote(const char *path) {
	ChangeDirectoryRemoteIdx(path, 0);
}

void ChangeDirectoryRemoteIdx(const char *path, int sockIdx) {
	chdir(path);
	SendNetMsgIdx(CHDIR, path, strlen(path) + 1, sockIdx);
	WaitForOKIdx(sockIdx);
}

void SendQueueMultithreaded(const char *startPath) {
	char *ogPath = getcwd(NULL, 0);
	int socketIndexes[GetConfigNTS()];
	pthread_t pthreads[GetConfigNTS()];
	
	for (int i = 0; i < GetConfigNTS(); i++) { // pre thread init
		*filenames[i] = '\0';
	}
	
	// connect any extra transfer sockets
	if (!IsHost()) { // we can safely request conn
		ConnectExtraTransferSockets();
	} else { // we have to request to request conn
		SendNetMsg(REQ_CONN_NEW_TS, &(int){GetConfigNTS()}, sizeof(int));
		while (GetNTS() < GetConfigNTS()) {
			char *buffer = ListenForNetMsgsLazy(-1);
			enum packet_type type = ((struct NetMsg*)buffer)->type;
			char *packData = ((struct NetMsg*)buffer)->data;
			switch (type) {
				default:
					printf("Got unknown pt: %d\n", type);
					break;
				case NEW_SOCK_CONN_REQ:
					AddExtraTransferSocket();
					printf("finished doing: %d\n", GetNTS());
					break;
			}
		}
	}
	SendNetCmd(START_DIR_SENDING);
	// init mutexes
	pthread_mutex_init(&fileTransferStartMutex, NULL);
	pthread_mutex_init(&anyInputMutex, NULL);
	pthread_mutex_init(&detachThreadMutex, NULL);
	pthread_mutex_init(&uploadDrawMutex, NULL);
	WaitForOK();
	ChangeDirectoryRemote(GetFilenameFromPath(startPath));
	for (int i = 0; i < GetConfigNTS(); i++) {
		socketIndexes[i] = i;
		fileCount[i].done = false;
		fileCount[i].filename[0] = '\0';
		PthreadSpawnFunc(&pthreads[i], SendQueueFiles, 1, &socketIndexes[i]);
	}
	
	while (true) {
		bool done = true;
		for (int i = 0; i < GetNTS(); i++) {
			if (!fileCount[i].done)
				done = false;
		}
		Wrp_ClearScreen();
		Wrp_CheckDefault();
		DrawUploadProgress();
		Wrp_FinishDrawing();
		if (done)
			break;
	}
	
	for (int i = 0; i < GetConfigNTS(); i++) {
		pthread_join(pthreads[i], NULL);
		//printf("joined pthread %d\n", i);
	}
	
	puts("Finished sending dir");
	
	putchar('\n');
	DisconnectExtraTransferSockets(); // disconnect any extra transfer sockets
	
	for (int i = 0; i < GetConfigNTS(); i++) { // post thread deinit
		//TDclose(&dirs[i]);
	}
	numFilesSent = 0;
	chdir(ogPath);
	free(ogPath);
}

void SendDir(const char *path) {
	if (!DirExists(path)) {
		puts("Directory does not exist. Please check the path and type in your information again.");
		return;
	}
	puts("Discovering files...");
	char *d = getcwd(NULL, 0);
	chdir(path);
	printf("Cur cwd: %s\n", d);
	AddDirToQueue(".");
	
	// init sending dir files
	puts("Finished discovering files");
	
	numFilesTotal = GetQueueLength();
	//printf("there are %d files\n", numFilesTotal);
	printf("Uploading:\n\n\n\n");
	
	SendQueueMultithreaded(path); // start sending files recursively
	
	//DrawUploadProgress("Done!");
	printf("\nFinished uploading folder!\n");
	//freeDirectoryList(dir);
	hasSelectedAll = true;
	forceDownloadNextFile = true;
	//numFilesTotal = numFilesSent = minAvailableFileIndex = 0;
	chdir(d);
	free(d);
}

void StartReceivingDir(void) {
	char *ogDir = getcwd(NULL, 0);
	puts("RECEIVING DIRECTORY!!1");
	int socketIndexes[GetNTS()];
	pthread_t pthreads[GetNTS()];
	
	struct timeval tm1, tm2;
	gettimeofday(&tm1, NULL);
	printf("Progress:\n");
	for (int i = 0; i < GetNTS()*2; i++) {
		putchar('\n');
	}
	pthread_mutex_init(&downloadDrawMutex, NULL);
	for (int i = 0; i < GetNTS(); i++) {
		//printf("Starting recv %d\n", i);
		socketIndexes[i] = i;
		fileCount[i].done = false;
		PthreadSpawnFunc(&pthreads[i], ReceiveQueue, 1, &socketIndexes[i]);
		//printf("started recv %d\n", i);
	}
	SendNetCmd(OK);
	puts("Sent OK");
	while (true) {
		bool done = true;
		for (int i = 0; i < GetNTS(); i++) {
			if (!fileCount[i].done)
				done = false;
		}
		Wrp_ClearScreen();
		Wrp_CheckDefault();
		DrawDownloadProgress();
		Wrp_FinishDrawing();
		if (done)
			break;
	}
	
	for (int i = 0; i < GetNTS(); i++) {
		pthread_join(pthreads[i], NULL);
	}
	DisconnectExtraTransferSockets();
	gettimeofday(&tm2, NULL);
	printf("Transfer took %f seconds\n", ((tm2.tv_sec - tm1.tv_sec)*1000000 + (tm2.tv_usec - tm1.tv_usec))/1000000.f);
	if (NumThreadWriteOps() > 0) {
		puts("Waiting for all file write operations to finish.");
		do {
			usleep(500000);
		} while (NumThreadWriteOps());
		puts("All write operations have been completed.");
	}
	printf("Finished receiving folder.\n");
	chdir(ogDir);
	free(ogDir);
}

void SyncDirectory(const char *dirPath) {
	
}

void SendDirectoryList(const char *path) {
	struct DirectoryElement *dir;
	int numElems = GetDirectoryElements(&dir, path);
	for (int i = 0; i < numElems; i++) {
		SendNetMsg(DIR_LIST_ELEM, &dir[i], sizeof(struct DirectoryElement));
		WaitForOK();
	}
	SendNetCmd(DIR_LIST_END);
}

void RecvOneFile(const char *filename, void *fileBuffer, size_t *fileSize) {
	pthread_t downloadThread;
	static_assert(sizeof(size_t) == sizeof(void*), "Size_t will not fit in void*");
	fileCount[0].done = false;
	fileCount[0].percent = 0;
	PthreadSpawnFunc(&downloadThread, ReceiveFile, 2, fileBuffer, *fileSize);
	while (!fileCount[0].done) {
		Wrp_ClearScreen();
		Wrp_CheckDefault();
		DrawDownloadProgress();
		Wrp_FinishDrawing();
	}
	bool ret = false;
	pthread_join(downloadThread, &ret);
	// receive file
	if (ret) { // if file receive did not time out, save it
		puts("Finished receiving file");
		PthreadSpawnFunc(&thread_temp, WriteFileT, 3, filename, fileBuffer, fileSize);
	} else {
		puts("An error has occured! Could not receive file :(");
		free(fileBuffer);
	}
}

static const char *unitIndex[] = {
	"bytes",
	"KB",
	"MB",
	"GB",
	"TB",
	"PB"
};

struct FileSizeReadable GetFileSizeInUnits(size_t size) {
	float s = size; // conv to float cuz i feel like it
	#define MAX_UNIT sizeof(unitIndex)/sizeof(unitIndex[0])
	
	// i have a vision for the world... for all code to be as condensed as possible...
	for (int i = MAX_UNIT - 1; i >= 0; i--) {
		size_t curUtSize;
		if (size >= (curUtSize = pow(1000, i))) {
			return (struct FileSizeReadable){.units = unitIndex[i], s/curUtSize};
		}
	}
	return (struct FileSizeReadable){.units = unitIndex[0], size};
}