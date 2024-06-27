#include "dir.h"

#include <stddef.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>

#include "re_array.h"
#include "queue.h"
#include "address_dbg.h"

_Atomic int writeFileOperations = 0;

pthread_mutex_t mkdirMutex;

void DirInit() {
	pthread_mutex_init(&mkdirMutex, NULL);
}

const char *GetFilenameFromPath(const char *path) {
	for (int i = strlen(path) - 1; i >= 0; i--) {
		if (path[i] == '/' || path[i] == '\\') {
			return path + i + 1;
		}
	}
	return path;
}

static inline size_t GetNumDirElems(const char *path) {
	char *old = getcwd(NULL, 0);
	chdir(path);
	char *cwDir = getcwd(NULL, 0);
	DIR *dr = opendir(".");
	free(cwDir);
	if (dr == NULL)
		return 0;
	size_t i = 0;
	struct dirent *de;
	while ((de = readdir(dr)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;
		++i;
	}
	closedir(dr);
	chdir(old);
	free(old);
	return i;
}

struct Directory *DiscoverDirElements(const char *path, int vPos, int *index) {
	struct Directory *dir = malloc(sizeof(struct Directory) + sizeof(void*)*GetNumDirElems(path));
	if (dir == NULL) {
		puts("Could not allocate new directory space");
		return NULL;
	}
	dir->numElems = GetNumDirElems(path);
	dir->type = ELEM_DIR;
	strncpy(dir->name, GetFilenameFromPath(path), MAX_FILENAME);
	chdir(path);
	char *cwDir = getcwd(NULL, 0);
	DIR *dr = opendir(".");
	if (dr == NULL) {
		printf("Could not open directory. CWD: %s\n", cwDir);
		return NULL;
	}
	
	struct dirent *de;
	
	int i = 0;
	while ((de = readdir(dr)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;
		struct stat st = {0};
		stat(de->d_name, &st);
		if (S_ISDIR(st.st_mode)) {
			dir->elems[i] = (struct DirElement*)DiscoverDirElements(de->d_name, vPos + 1, index);
			chdir(cwDir); // return to current dir
		} else {
			dir->elems[i] = (struct DirElement*)malloc(sizeof(struct File));
			if (dir->elems[i] == NULL) {
				puts("Could not allocate file space");
				dir->numElems = i + 1;
				break;
			}
			struct File *newFile = (struct File*)dir->elems[i];
			newFile->type = ELEM_FILE;
			strncpy(newFile->name, de->d_name, MAX_FILENAME);
			newFile->index = index[0]++;
		}
		i++;
	}
	dir->numElems = i;
	free(cwDir);
	closedir(dr);
	return dir;
}

void freeDirectoryList(struct Directory *dir) {
	for (int i = 0; i < dir->numElems; i++) {
		switch (dir->elems[i]->type) {
			case ELEM_FILE:
				free(dir->elems[i]);
				break;
			case ELEM_DIR:
				freeDirectoryList((struct Directory*)dir->elems[i]);
				free(dir->elems[i]);
				break;
		}
	}
}

void PrintDirectory(struct Directory *dir, int vPos) {
	for (int i = 0; i < dir->numElems; i++) {
		for (int j = 0; j < vPos; j++)
			printf("-");
		switch (dir->elems[i]->type) {
			case ELEM_FILE:
				printf("%s\n", dir->elems[i]->name);
				break;
			case ELEM_DIR:
				printf("%s:\n", dir->elems[i]->name);
				PrintDirectory((struct Directory*)dir->elems[i], vPos + 1);
				break;
		}
	}
}

size_t CountDirFiles(struct Directory *dir) {
	size_t count = 0;
	for (int i = 0; i < dir->numElems; i++) {
		switch (dir->elems[i]->type) {
			case ELEM_FILE:
				++count;
				break;
			case ELEM_DIR:
				count += CountDirFiles((struct Directory*)dir->elems[i]);
				break;
		}
	}
	return count;
}

bool DirExists(const char *path) {
	struct stat st = {0};
	return stat(path, &st) != -1;
}

void CreateDir(const char *path) {
#ifdef _WIN32
	mkdir(path);
#else
	mkdir(path, 0700);
#endif
}

bool FileExists(const char *filename) {
	return access(filename, F_OK) == 0;
}


// this uses forward slashes, but some os's use backslashes. TODO: FIX!!!
void rek_mkdir(char *path) {
    char *sep = strrchr(path, '\\');
    if(sep != NULL) {
        *sep = 0;
        rek_mkdir(path);
        *sep = '\\';
    }
#ifndef _WIN32
#define EXTRADIR ,0777
#else
#define EXTRADIR
#endif
    if(mkdir(path EXTRADIR) && errno != EEXIST)
        fprintf(stderr, "error while trying to create '%s'\n%m\n", path); 
}

FILE *fopen_mkdir(char *path, char *mode) {
	pthread_mutex_lock(&mkdirMutex);
	char *curD = getcwd(NULL, 0);
    char *sep = strrchr(path, '\\');
    if (sep) { 
        char *path0 = strdup(path);
        path0[sep - path] = 0;
        rek_mkdir(path0);
        free(path0);
    }
	FILE *ff = fopen(path, mode);
	chdir(curD);
	pthread_mutex_unlock(&mkdirMutex);
    return ff;
}

#include <windows.h>

void WriteFileT(char *filename, char *data, uint64_t *size) {
	++writeFileOperations;
	
	FILE *writeFile = fopen_mkdir(filename, "wb");
	
	fwrite(data, *size, 1, writeFile);
	
	fclose(writeFile);
	//ShellExecute(NULL, "open", filename, NULL, NULL, SW_SHOWNORMAL);
	
	--writeFileOperations;
	// todo: free size. i don't feel like breaking anything right now though...
}

int NumThreadWriteOps(void) {
	return writeFileOperations;
}

static char *strdup_2cat(const char *base, const char *cat1, const char *cat2) { // this is kind of stupid but idc
	size_t maxLen = strlen(base) + 
	strlen(cat1) + 
	strlen(cat2) + 1;
	char *data = malloc(maxLen);
	snprintf(data, maxLen, "%s%s%s", base, cat1, cat2);
	return data;
}

int vLevel = 0;

static size_t AddDirToQueue_Internal(const char *relPath, const char *fullPath) {
	DIR *dr = opendir(".");
	if (dr == NULL) {
		printf("Could not open directory. CWD: %s\n", getcwd(NULL, 0)); // yes this causes a memory leak, but it's not that big. also you're fricked anyways.
	}
	
	struct dirent *de;
	
	size_t i = 0;
	while ((de = readdir(dr)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;
		struct _stat64 st = {0};
		stat64(de->d_name, &st);
		char *newFullPath = strdup_2cat(fullPath, "\\", de->d_name);
		if (S_ISDIR(st.st_mode)) {
			chdir(de->d_name);
			vLevel += 1;
			size_t filesInFolder = AddDirToQueue_Internal(de->d_name, newFullPath);
			i += filesInFolder;
			vLevel -= 1;
			chdir("..");
			if (vLevel == 0) {
				printf("Folder %s has %lld file%c\n", de->d_name, filesInFolder, filesInFolder == 1 ? ' ' : 's');
			}
		} else { // is file
			AddFileToQueue(newFullPath);
			i++;
		}
		free(newFullPath);
	}
	
	closedir(dr);
	return i;
}

void AddDirToQueue(const char *path) {
	chdir(path);
	printf("found %lld files\n", AddDirToQueue_Internal(path, path));
}

void *mallocData(const void *data, size_t size) {
	char *ret = malloc(size);
	memcpy(ret, data, size);
	return ret;
}

static const char *imageFT[] = {
	"png",
	"jpg",
	"jpeg",
	"bmp",
	"webp",
};

static const char *videoFT[] = {
	"mp4",
	"mov",
	"avi",
	"webm",
	"mkv",
	"wmv",
};

static const char *audioFT[] = {
	"mp3",
	"flac",
	"wav",
	"ogg",
	"opus",
	"mid",
	"midi",
};

#define LEN(x) sizeof(x)/sizeof(x[0])

bool InStrArray(const char *item, const char *arr[], size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (strcasecmp(arr[i], item) == 0)
			return true;
	}
	return false;
}

enum file_type GetFileExtensionType(const char *name) {
	const char *extStart = name + strlen(name);
	for (; extStart > name; extStart--) {
		if (*extStart == '.') {
			extStart++; // don't count dot
			break;
		}
	}
	
	if (InStrArray(extStart, imageFT, LEN(imageFT)))
		return FT_IMAGE;
	else if (InStrArray(extStart, videoFT, LEN(videoFT)))
		return FT_VIDEO;
	else if (InStrArray(extStart, audioFT, LEN(audioFT)))
		return FT_AUDIO;
	
	return FT_UNKNOWN;
}

struct DirectoryStats GetDirectoryStats(const char *path) {
	char *oldCwd = getcwd(NULL, 0);
	if (chdir(path) != 0) {
		goto freeCwd;
	}
	DIR *dr = opendir(".");
	if (dr == NULL) {
		goto closeDir;
	}
	
	struct dirent *de;
	
	struct DirectoryStats d = {0};
	
	while ((de = readdir(dr)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;
		struct _stat64 st = {0};
		stat64(de->d_name, &st);
		
		if (S_ISDIR(st.st_mode)) {
			++d.numFolders;
			/*struct DirectoryStats nd = GetDirectoryStats(de->d_name); // too slow
			d.numFiles += nd.numFiles;
			d.numFolders += nd.numFolders;
			d.totalSize += nd.totalSize;
			d.numElems += nd.numElems;*/
			
		} else { // is file
			++d.numFiles;
			d.totalSize += st.st_size;
		}
	}
	
	d.numElems = d.numFiles + d.numFolders;

closeDir:
	closedir(dr);
freeCwd:
	chdir(oldCwd);
	free(oldCwd);
	return d;
}

int GetDirectoryElements(struct DirectoryElement **e, const char *path) {
	DIR *dr = opendir(".");
	if (dr == NULL) {
		printf("Could not open directory. CWD: %s\n", getcwd(NULL, 0)); // yes this causes a memory leak, but it's not that big. also you're fricked anyways.
	}
	
	struct dirent *de;
	
	*e = malloc(1);
	
	size_t i = 0;
	while ((de = readdir(dr)) != NULL) {
		/*if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;*/
		struct _stat64 st = {0};
		stat64(de->d_name, &st);
		++i;
		*e = realloc(*e, i*sizeof(struct DirectoryElement));
		if (S_ISDIR(st.st_mode)) {
			(*e)[i - 1] = (struct DirectoryElement){
				.folderStats = GetDirectoryStats(de->d_name),
			};
			(*e)[i - 1].size = (*e)[i - 1].folderStats.totalSize; // dumb hack to get the hover menu to show the size
			strncpy((*e)[i - 1].name, de->d_name, MAX_FILENAME);
			(*e)[i - 1].ft = FT_FOLDER;
		} else { // is file
			(*e)[i - 1] = (struct DirectoryElement){
				.size = st.st_size,
			};
			strncpy((*e)[i - 1].name, de->d_name, MAX_FILENAME);
			(*e)[i - 1].ft = GetFileExtensionType(de->d_name);
		}
	}
	
	closedir(dr);
	return i;
}

size_t FileGetSize(const char *name) {
	struct _stat64 st = {0};
	stat64(name, &st);
	return st.st_size;
}