#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define MAX_FILENAME 1024

struct FileInfo {
	uint64_t size;
	char filename[MAX_FILENAME];
	bool forceDownload;
};

struct WriteFileArgs {
	size_t size;
	char filename[MAX_FILENAME];
	void *data;
};

struct ThreadDir;

bool ReceiveFile(char *fileBuffer, uint64_t fileSize);
bool SendFile(const char *filename, bool force);
bool SendFileSockIdx(const char *filename, bool force, int sockIdx);
void DrawDownloadProgress(void);
void SendDir(const char *path);
void ChangeDirectoryRemote(const char *path);
void WriteFileF(struct WriteFileArgs *args);
void DrawUploadProgress(void);
bool InputToConnServer(void);
void ReceiveDirectory(int *sockIdx);
void StartReceivingDir(void);
void ChangeDirectoryRemoteIdx(const char *path, int sockIdx);
void SyncDirectory(const char *dirPath);
void SendDirectoryList(const char *path);
void RecvOneFile(const char *filename, void *fileBuffer, size_t *fileSize);

struct FileSizeReadable {
	const char *units;
	float size;
};

struct FileSizeReadable GetFileSizeInUnits(size_t size);