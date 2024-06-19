#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_FILENAME 1024

enum DIR_ELEM_TYPE {
	ELEM_FILE = 0,
	ELEM_DIR,
};

enum file_type {
	FT_UNKNOWN,
	FT_FOLDER,
	FT_AUDIO,
	FT_IMAGE,
	FT_VIDEO,
};

struct Directory {
	enum DIR_ELEM_TYPE type;
	char name[MAX_FILENAME];
	int numElems;
	struct Directory *parentDir;
	struct DirElement *elems[];
};

struct File {
	enum DIR_ELEM_TYPE type;
	char name[MAX_FILENAME];
	struct Directory *parentDir;
	uint64_t size;
	int index;
};

struct DirElement {
	enum DIR_ELEM_TYPE type;
	char name[MAX_FILENAME];
};

struct ThreadFile {
	int fd;
};

struct ThreadDir {
	int fd;
};

struct DirectoryStats {
	uint64_t totalSize;
	uint64_t numElems;
	uint64_t numFiles;
	uint64_t numFolders;
};

struct DirectoryElement {
	char name[MAX_FILENAME];
	uint64_t size;
	enum file_type ft;
	struct DirectoryStats folderStats;
};


struct ThreadFile OpenTDFile(const char *path);
struct ThreadDir OpenTDDir(const char *path);

void DirInit(void);
struct Directory *DiscoverDirElements(const char *path, int vPos, int *index);
void PrintDirectory(struct Directory *dir, int vPos);
void freeDirectoryList(struct Directory *dir);
bool DirExists(const char *path);
void CreateDir(const char *path);
bool FileExists(const char *filename);
size_t CountDirFiles(struct Directory *dir);
const char *TDGetPath(struct ThreadDir td);
struct ThreadDir OpenTDDir(const char *path);
const char *TDgetcwd(void);
void TDchdir(struct ThreadDir *td, const char *dir);
bool TDFileExists(struct ThreadDir td, const char *filename);
void TDclose(struct ThreadDir *td);
struct ThreadDir TDopenFile(struct ThreadDir *td, const char *filename);
void WriteFileT(char *filename, char *data, uint64_t *size);
int NumThreadWriteOps(void);
const char *GetFilenameFromPath(const char *path);
void AddDirToQueue(const char *path);
void *mallocData(const void *data, size_t size);
int GetDirectoryElements(struct DirectoryElement **e, const char *path);
size_t FileGetSize(const char *name);