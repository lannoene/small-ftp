#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h> // lets us use close function
#include <stdlib.h>
#include <math.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdatomic.h>

#include "conio.h"
#include "dir.h"
#include "net.h"
#include "thread_wrap.h"
#include "transfer.h"
#include "queue.h"
#include "sdlwrp.h"
#include "menu.h"
#include "shell.h"
#include "address_dbg.h"

#define MAX_CLIENTS 10

struct UdpPingResponse {
	char ip[17];
	uint16_t port;
};

struct Config {
	
} config;

bool b_exit = false;

void RunShell(void);

char *OpenDirDialogue(int flags);
void InitPlatformSpecific(void);
#if defined(_WIN32)
#include <windows.h>
#include <tchar.h>
#include <shobjidl.h>

void InitPlatformSpecific(void) {
	extern LRESULT CALLBACK WndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam);
	WNDCLASSEX  WndClsEx = {0};
	HINSTANCE hInstance = GetModuleHandle(NULL);

    WndClsEx.cbSize        = sizeof (WNDCLASSEX);
    WndClsEx.style         = CS_HREDRAW | CS_VREDRAW;
    WndClsEx.lpfnWndProc   = WndProc;
    WndClsEx.hInstance     = hInstance;
    WndClsEx.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    WndClsEx.lpszClassName = "DialogueBox";
    WndClsEx.hIconSm       = LoadIcon(hInstance, IDI_APPLICATION);

    RegisterClassEx(&WndClsEx);
}

enum F {
	DIALOGUE_OPEN_DIR = 0x1,
};

// stolen from stack overflow
char *OpenDirDialogue(int flags) {
	static bool initialized = false;
	if (!initialized) {
		CoInitialize(NULL);
		initialized = true;
	}
	IFileOpenDialog *pfd;
	IShellItem *psiResult;
	PWSTR pszFilePath = NULL;
	if (SUCCEEDED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_ALL, &IID_IFileOpenDialog, (void**)&pfd))) {
		pfd->lpVtbl->SetOptions(pfd, FOS_PICKFOLDERS);
		pfd->lpVtbl->Show(pfd, NULL);
		if (SUCCEEDED(pfd->lpVtbl->GetResult(pfd, &psiResult))){
			if (SUCCEEDED(psiResult->lpVtbl->GetDisplayName(psiResult, SIGDN_FILESYSPATH, &pszFilePath))) {
				char *ret = malloc(1);
				int i = 0;
				WCHAR *p = pszFilePath;
				while (*p) {
					ret[i] = *p;
					p++;
					ret = realloc(ret, ++i + 1);
				}
				ret[i] = '\0';
				CoTaskMemFree(pszFilePath);
				return ret;
			} else {
				puts("nope 2");
			}
			psiResult->lpVtbl->Release(psiResult);
		}
		pfd->lpVtbl->Release(pfd);
	} else {
		printf("nope 3: %ld\n", GetLastError());
	}
	return NULL;
}

void GetDirectoryElementIcon(const char *path) {
	SHFILEINFOA sfi;
	if (SUCCEEDED(SHGetFileInfoA("C:\\Users\\lannoene\\Videos\\2022-08-25 09-57-01.mkv", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_ICONLOCATION))) {
		puts(sfi.szDisplayName);
		ICONINFO iInfo;
		GetIconInfo(sfi.hIcon, &iInfo);
		char buffer[32];
		GetBitmapBits(iInfo.hbmColor, 32, buffer);
		FILE *fp = fopen("asea.bmp", "wb");
		fwrite(buffer, 256, 1, fp);
		fclose(fp);
		DestroyIcon(sfi.hIcon);
	} else {
		puts("Could not");
	}
}

void OnAddFile(void) {
	
}

#else
char *OpenDirDialogue(int flags) {
	char *sendDirPath = malloc(100);
	puts("Enter directory:");
	fflush(stdin);
	fgets(sendDirPath, 100, stdin);
	sendDirPath[strcspn(sendDirPath, "\n")] = '\0';
	
	
	return sendDirPath;
}
#endif

#include <unistd.h>

#ifdef _WIN32
#include <locale.h>
#endif

void ListenRoutine(void);

void TitlescreenOnClick(int x, int y) {
	
}

bool titleContinue = false;
Menu_t mMain;

void SelectClient() {
	SetHost(false);
	puts("You are a client. Press 'j' to join.");
	puts("Searching for local servers... (this will not ping localhost)");
	unsigned char ipOp = GetNIpNum() >> 16;
	unsigned char ipLast = GetNIpNum() >> 24;
	for (int i = 0; i < 255; i++) {
		if (i == ipLast)
			continue;
		char ip[17];
		snprintf(ip, 17, "192.168.%d.%d", ipOp, i);
		SendUdpMsgIp(UDP_PING, "", 0, ip, UDP_PORT);
	}
	titleContinue = true;
}

void Connect() {
	if (!IsHost() && !IsConnected()) {
		char *sendToIp = CreateDialoguePopup("Input IP and Port", "", 0);
		uint16_t sendToPort;
		if (strcmp(sendToIp, "localhost") != 0) { // if ip is not localhost
			// get port
			for (int i = 0; i < strlen(sendToIp); i++) {
				char portStr[10];
				if (sendToIp[i] == ':') {
					int portStart = i + 1;
					strcpy(portStr, sendToIp + portStart); // copy port to new string
					sendToPort = atoi(portStr); // change port into int
					sendToIp[i] = '\0'; // remove port from ip string
					break;
				}
			}
		} else {
			strcpy(sendToIp, "127.0.0.1");
			sendToPort = DEFAULT_PORT;
		}
		if (!ConnectToPeer(sendToIp, sendToPort)) {
			puts("Could not connect to host. Please reenter information.");
			return;
		} else {
			puts("Successfully connected to host.");
			MenuRemoveElement(&mMain, "connect");
			return;
		}
		free(sendToIp);
	}
}

void Host() {
	puts("You are the host");
	SetHost(true);
	titleContinue = true;
}

void CL_SendDir() {
	char *dirPath = OpenDirDialogue(0);
	if (dirPath) {
		SendDir(dirPath);
		free(dirPath);
	}
}

void CL_SendFile() {
	char sendFilename[50];
	puts("Enter filename to send:");
	scanf("%s", sendFilename);
	SendFile(sendFilename, true);
	puts("Finished sending file");
}

struct ExplorerExtraData {
	int isFolder;
	char *filename;
	uint64_t size;
	enum file_type ft;
	struct DirectoryStats folderStats;
};

void CL_freeElem(struct MenuElement *e) {
	free(((struct ExplorerExtraData*)e->extraData)->filename);
	free(e->extraData);
}

Menu_t mexp; // main explorer window
void CL_RequestNewDirectory(struct MenuElement *e);

bool fileInfoOpen = true;

void DrawFileInfo(struct MenuElement *eP, SDL_Window *win, SDL_Renderer *rend) {
	Menu_t mInf;
	MenuInit(&mInf);
	MenuSetRenderer(&mInf, rend);
	int wId = SDL_GetWindowID(win); // get win id? idk
	// create copy of element extData because menu may be unloaded in the future. still a small race condition, but not too big to worry about.
	struct ExplorerExtraData *ox = eP->extraData;
	struct ExplorerExtraData exd = *ox; // set default stuff
	exd.filename = strdup(ox->filename); // duplicate string. i've grown to love strdup
	char *type;
	switch (exd.ft) {
		case FT_UNKNOWN:
			type = "file";
			break;
		case FT_FOLDER:
			type = "folder";
			break;
		case FT_AUDIO:
			type = "audio";
			break;
		case FT_IMAGE:
			type = "image";
			break;
		case FT_VIDEO:
			type = "video";
			break;
	}
	
	
	
	while (fileInfoOpen) {
		SDL_SetRenderDrawColor(rend, 0xff, 0xff, 0xff, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(rend);
		
		Wrp_DrawTextF_r("Name: %s", 0, 0, 22, rend, exd.filename);
		Wrp_DrawTextF_r("Type: %s", 0, 22, 22, rend, type);
		if (exd.ft != FT_FOLDER)
			Wrp_DrawTextF_r("Size: %llu bytes", 0, 44, 22, rend, exd.size);
		else { // print folder stuff
			Wrp_DrawTextF_r("Contains %llu elements:", 0, 44, 22, rend, exd.folderStats.numElems);
			Wrp_DrawTextF_r("%llu %s", 0, 66, 22, rend, exd.folderStats.numFiles, exd.folderStats.numFiles == 1 ? "file" : "files");
			Wrp_DrawTextF_r("%llu %s", 0, 88, 22, rend, exd.folderStats.numFolders, exd.folderStats.numFolders == 1 ? "folder" : "folders");
			struct FileSizeReadable fsR = GetFileSizeInUnits(exd.folderStats.totalSize);
			Wrp_DrawTextF_r("Total size: %.1f %s", 0, 110, 22, rend, fsR.size, fsR.units);
		}
		
		SDL_RenderPresent(rend);
	}

	fileInfoOpen = true;
	
	free(exd.filename);
	MenuFree(&mInf);
}

SDL_Window *win;
void POP_GetProperties(struct MenuElement *e) {
	struct ExplorerExtraData *extDat = e->extraData;
	win = SDL_CreateWindow("File info",
                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 350,
                          450, SDL_WINDOW_SHOWN);
	SDL_Renderer *rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	
	PthreadSpawnFunc(&thread_temp, DrawFileInfo, 3, e, win, rend);
}

void POP_DownloadFolder(struct MenuElement *e) {
	struct ExplorerExtraData *extDat = e->extraData;
	SendNetMsg(REQ_DIR, extDat->filename, strlen(extDat->filename) + 1);
}

void GetNewRemoteDirectoryList(Menu_t *m, const char *path) {
	SendNetMsg(CHDIR, path, strlen(path) + 1);
	WaitForOK();
	
	// request current directory
	SendNetCmd(REQ_DIR_LIST);
	WaitForOK();
	
	//MenuAddText(&mexp, NULL, 0, 0, "Directory:", 22);
	
	MenuReset(m);
	MenuToggleScroll(m, true);
	
	int num = 0;
	while (true) { // wait for file head acke
		char buffer[STATIC_BUF_SIZE];
		if (ListenForNetMsgs(buffer)) {
			switch (((struct NetMsg*)buffer)->type) {
				case DIR_LIST_ELEM: {
					struct DirectoryElement *e = (void*)((struct NetMsg*)buffer)->data;
					char cutoffFilename[50];
					int nCpy = snprintf(cutoffFilename, 25, "%s", e->name);
					if (nCpy > 24) {
						strcat(cutoffFilename, "...");
					}
					MenuElem_t *el = MenuAddButton(m, NULL, 25 + 125*(num % 6), (num/6)*125, 100, 100, cutoffFilename);
					el->onClick = CL_RequestNewDirectory;
					el->onRemove = CL_freeElem; // destructor
					el->style.borderColor = CLR_WHT;
					el->style.hoverColor = CLR_LBLU;
					el->extraData = malloc(sizeof(struct ExplorerExtraData));
					ElementAddContextMenu(el, 2, (struct ContextMenuElement[]){
						{CE_BUTTON, (e->ft == FT_FOLDER) ? "Download Folder" : "Download File", (e->ft == FT_FOLDER) ? POP_DownloadFolder : CL_RequestNewDirectory},
						{CE_BUTTON, "Properties", POP_GetProperties},
					});
					struct ExplorerExtraData *exDat = el->extraData;
					exDat->isFolder = false;
					exDat->ft = e->ft;
					switch (e->ft) {
						default:break;
						case FT_FOLDER:
							MenuSetBackgroundImage(el, IMG_FOLDER_ICON);
							exDat->isFolder = true;
							exDat->folderStats = e->folderStats;
							break;
						case FT_UNKNOWN:
							MenuSetBackgroundImage(el, IMG_FILE_ICON);
							break;
						case FT_VIDEO:
							MenuSetBackgroundImage(el, IMG_VIDEO_ICON);
							break;
						case FT_AUDIO:
							MenuSetBackgroundImage(el, IMG_AUDIO_ICON);
							break;
						case FT_IMAGE:
							MenuSetBackgroundImage(el, IMG_IMAGE_ICON);
							break;
					}
					exDat->size = e->size;
					exDat->filename = strdup(e->name);
					struct FileSizeReadable fsR = GetFileSizeInUnits(e->size);
					// determine if size has trailing 0 after 2 dec places... ugh
					if (fsR.size == 0) {
						ElementSetHoverBar(el, EvalStringFormat("0 %s", fsR.units));
					} else if ((size_t)(fsR.size*100) % 10 == 0) { // does have leading 0 after 2
						if ((size_t)(fsR.size*10) % 10 == 0) { // check if it also has leading after 1 (if it's .00 or something. we also want to truncate that)
							ElementSetHoverBar(el, EvalStringFormat("%.0f %s", fsR.size, fsR.units));
						} else { // else, just print it with 1 dec
							ElementSetHoverBar(el, EvalStringFormat("%.1f %s", fsR.size, fsR.units));
						}
					} else { // doesn't have
						ElementSetHoverBar(el, EvalStringFormat("%.2f %s", fsR.size, fsR.units));
					}
					++num;
					SendNetCmd(OK);
					break;
				}
				case DIR_LIST_END:
					return;
			}
		}
	}
}

void CL_RequestNewDirectory(struct MenuElement *e) {
	struct ExplorerExtraData* extDat = e->extraData;
	if (extDat->isFolder) {
		GetNewRemoteDirectoryList(&mexp, extDat->filename); // CHANGE TO .extraData LATER
	} else {
		puts(getcwd(NULL, 0));
		SendNetMsg(REQ_FILE, extDat->filename, strlen(extDat->filename) + 1);
	}
}

void CL_ExploreRemoteDirectory() {
	MenuInit(&mexp);
	
	GetNewRemoteDirectoryList(&mexp, "."); // get default directory
	
	size_t i = 0;
	while (true) {
		Wrp_ClearScreen();
		if (!MenuRun(&mexp))
			return;
		ListenRoutine();
		MenuDraw(&mexp);
		Wrp_FinishDrawing();
	}
}

int main(int argc, char* argv[]) {
	DirInit();
	if (!InitQueue())
		return 0;
	InitPlatformSpecific();
	Wrp_InitSDL("Small FTP GUI");
	if (!HostInit(DEFAULT_PORT))
		puts("Could not init socket! Sorry! (this probably means that the ports have already been taken)");
	
	chdir(".");
	
	GetDirectoryElementIcon("romfs");
	
	puts("Enter mode: (c client, h host)");
	// init titlescreen menu
	Menu_t m_title;
	MenuInit(&m_title);
	MenuAddButton(&m_title, "join", 50, 50, 100, 100, "Join");
	MenuGetElement(&m_title, "join")->onClick = SelectClient;
	MenuGetElement(&m_title, "join")->style.hoverColor = CLR_GRY;
	MenuAddButton(&m_title, "host", 175, 50, 100, 100, "Host");
	MenuGetElement(&m_title, "host")->onClick = Host;
	MenuGetElement(&m_title, "host")->style.hoverColor = CLR_GRY;
	MenuAddText(&m_title, NULL, 0, 0, "Select Mode:", 22);
	
	while (!titleContinue) {
		Wrp_ClearScreen();
		if (!MenuRun(&m_title))
			return 0;
		MenuDraw(&m_title);
		Wrp_FinishDrawing();
	}
	MenuFree(&m_title);
	
	puts("To send a file, press s. To send a directory, press d.");
	
	MenuInit(&mMain);
	MenuAddButton(&mMain, "sendDir", 50, 50, 100, 100, "Send Directory");
	MenuGetElement(&mMain, "sendDir")->onClick = CL_SendDir;
	MenuAddText(&mMain, NULL, 0, 0, IsHost() ? "Host" : "Client", 22);
	if (!IsHost()) {
		MenuAddButton(&mMain, "connect", 175, 50, 100, 100, "Connect");
		MenuGetElement(&mMain, "connect")->onClick = Connect;
	}
	
	MenuAddButton(&mMain, "shell", 300, 50, 100, 100, "Shell");
	MenuGetElement(&mMain, "shell")->onClick = RunShell;
	
	MenuAddButton(&mMain, "sendFile", 425, 50, 100, 100, "Send File");
	MenuGetElement(&mMain, "sendFile")->onClick = CL_SendFile;
	
	MenuAddButton(&mMain, "explore", 550, 50, 100, 100, "Explore");
	MenuGetElement(&mMain, "explore")->onClick = CL_ExploreRemoteDirectory;
	
	while (!b_exit) {
		Wrp_ClearScreen();
		ListenRoutine();
		
		if (!MenuRun(&mMain))
			break;
		MenuDraw(&mMain);
		Wrp_FinishDrawing();
	}
	
	MenuFree(&mMain);
	
	return 0;
}

void ListenRoutine(void) {
	if (IsHost()) {
		char buffer[65535];
		addressInfo addr;
		if (ListenForUdpMsgs(buffer, &addr)) {
			enum packet_type type = ((struct NetMsg*)buffer)->type;
			char *packData = ((struct NetMsg*)buffer)->data;
			switch (type) {
				case UDP_PING:
					struct UdpPingResponse ack;
					ack.port = GetPort();
					GetIp(ack.ip);
					printf("Sending: %s:%d\n", ack.ip, ack.port);
					SendUdpMsgSAddr(UDP_PING, &ack, sizeof(ack), addr);
					break;
			}
		}
		
		if (!IsConnected() && ListenForConns()) {
			printf("Peer connected!\n");
		}
	} else {
		char buffer[65535];
		struct sockaddr_in addr;
		if (ListenForUdpMsgs(buffer, &addr)) {
			enum packet_type type = ((struct NetMsg*)buffer)->type;
			switch (type) {
				case UDP_PING:
					FILE *fp = fopen("contents.dat", "w");
					fwrite(buffer, sizeof(buffer), 1, fp);
					fclose(fp);
					struct UdpPingResponse ack;
					memcpy(&ack, ((struct NetMsg*)buffer)->data, sizeof(ack));
					printf("Found %s:%d\n", inet_ntoa(addr.sin_addr), ack.port);
					break;
			}
		}
	}
	bool listenForData = true;
	char buffer[STATIC_BUF_SIZE];
	uint64_t bytesRecv = 0;
	//puts("2");
	while (bytesRecv = ListenForNetMsgs(buffer)) {
		if (bytesRecv == -1) { // on error
			b_exit = true;
			break;
		}
		enum packet_type type = ((struct NetMsg*)buffer)->type;
		char *packData = ((struct NetMsg*)buffer)->data;
		uint64_t packSize = ((struct NetMsg*)buffer)->size;
		switch (type) {
			default:
				printf("Unkown data type: %d\n", type);
				break;
			case FILE_HEADER: {
				// get relevant data out of packet
				uint64_t fileSize = ((struct FileInfo*)(packData))->size;
				char *filename = strdup(((struct FileInfo*)(packData))->filename);
				printf("Downloading file:\nSize: %lld\nName: %s\n", fileSize, filename);
				// tell peer that we got their request and we're okay with it
				if (!FileExists(filename) || ((struct FileInfo*)(packData))->forceDownload) {
					char e = 1;
					SendNetMsg(FILE_REQ_ACK, &e, 1);
					char *fileBuffer = malloc(fileSize);
					RecvOneFile(filename, fileBuffer, mallocData(&fileSize, sizeof(size_t)));
					SendNetCmd(OK);
				} else {
					char e = 0;
					SendNetMsg(FILE_REQ_ACK, &e, 1);
					free(filename);
				}
				break;
			}
			case CHDIR:
				if (!DirExists(packData))
					CreateDir(packData);
				chdir(packData);
				SendNetCmd(OK);
				break;
			case NEW_SOCK_CONN_REQ:
				AddExtraTransferSocket();
				puts("finished doing");
				break;
			case START_DIR_SENDING:
				puts("Got start dir sending");
				StartReceivingDir();
				break;
			case REQ_DIR_LIST:
				SendNetCmd(OK);
				SendDirectoryList(".");
				break;
			case REQ_FILE:
				SendFile(packData, true);
				break;
			case REQ_CONN_NEW_TS:
				static_assert(sizeof(int) == 4, "This architecture is not compatible to be compiled for.");
				int oldCNTS = GetConfigNTS();
				int newNts;
				memcpy(&newNts, packData, sizeof(int));
				SetConfigNTS(newNts);
				ConnectExtraTransferSockets();
				SetConfigNTS(oldCNTS);
				break;
			case REQ_DIR:
				SendDir(packData);
				break;
		}
	}
}