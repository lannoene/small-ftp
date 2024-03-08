#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h> // lets us use close function
#include <stdlib.h>
#include <math.h>
#include <netdb.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <poll.h>

#include "conio.h"

#define MAX_CLIENTS 10
#define DEFAULT_PORT 4659
#define STATIC_BUF_SIZE 4096
//#define STATIC_BUF_SIZE 128

#define NETHEADER_SIZE ((int)(sizeof(uint32_t) + sizeof(enum packet_type)))
#define CLAMP(min, d, max) \
	((d < min ? min : d) > max ? max : (d < min ? min : d)) \

#define STDIN_FILENO 0

enum conn_status {
	DISCONNECTED = 0,
	CONNECTING,
	DISCONNECTING,
	RECEIVING,
	SENDING,
	IDLING,
};

enum packet_type {
	DATA_HEADER = 1, // unused
	DATA, // unused
	FILE_HEADER,
	FILE_DATA,
	FILE_REQ_ACK,
};

struct Server {
	struct Client {
		int sock;
		enum conn_status status;
		struct pollfd poll;
	} peer;
	uint16_t port;
	int sock;
	struct pollfd poll;
	bool isHosting;
} host;

struct NetMsg {
	uint32_t size;
	enum packet_type type;
	char data[];
};

struct FileInfo {
	uint32_t size;
	char filename[30];
};

long l_true = true; // socket options jank
long l_false = false;

bool b_exit = false;

bool HostInit(const uint16_t port);
int ListenForConns(void);
int AcceptPeer(void);
void AddPeer(int sock);
void ResetPeerData(struct Client *peer);
void SendFileTo(const char *ip, const uint16_t port, const char *filename);
int ListenForNetMsgs(char *buf);
bool ConnectToPeer(const char *ip, uint16_t port);
void SendNetMsg(enum packet_type type, void *data, uint32_t size);
bool ReceiveFile(char *fileBuffer, int fileSize);
void SendFile(const char *filename);
void DrawDownloadProgress(int percent);
void DrawTitleScreen();

int main(int argc, char* argv[]) {
	if (!HostInit(DEFAULT_PORT))
		puts("Could not init socket! Sorry! (this probably means that the ports have already been taken)");
	
	puts("Enter mode: (j join, h host)");
	while (true) {
		char opt = c_getch();
		if (opt == 'j') {
			host.isHosting = false;
			
			//sendFile = true;
			puts("You are a client");
			puts("Enter ip & port to connect to (fmt ip:port or type localhost):");
			char sendToIp[50];
			uint16_t sendToPort;
			scanf("%s", sendToIp); // scanf sucks omg i have to do everything myself
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
			printf("ip: %s, port: %hu\n", sendToIp, sendToPort);
			
			if (!ConnectToPeer(sendToIp, sendToPort)) {
				puts("Could not connect to host. Please renter information.");
				puts("Enter mode: (j join, h host)"); // jank
			} else {
				puts("Successfully connected to host.");
				break;
			}
		} else if (opt == 'h') {
			puts("You are the host");
			host.isHosting = true;
			break;
		}
	}
	puts("To send a file, press s");
	
	while (!b_exit) {
		//puts("new");
		if (c_kbhit() && c_getch() == 's') {
			char sendFilename[50];
			puts("Enter filename to send:");
			scanf("%s", sendFilename);
			SendFile(sendFilename);
		}
		if (host.isHosting) {
			//puts("listening for conns");
			if (ListenForConns()) {
				printf("Peer connected!\n");
			}
		}
		bool listenForData = true;
		char buffer[STATIC_BUF_SIZE];
		int bytesRecv = 0;
		while (bytesRecv = ListenForNetMsgs(buffer)) {
			if (bytesRecv == -1) { // on error
				b_exit = true;
				break;
			}
			enum packet_type type = ((struct NetMsg*)buffer)->type;
			char *packData = ((struct NetMsg*)buffer)->data;
			uint32_t packSize = ((struct NetMsg*)buffer)->size;
			switch (type) {
				default:
					printf("Unkown data type: %d", type);
					break;
				case FILE_HEADER:
					// get relevant data out of packet
					int fileSize = ((struct FileInfo*)(packData))->size;
					char filename[30];
					strncpy(filename, ((struct FileInfo*)(packData))->filename, 30);
					printf("Downloading file:\nSize: %d\nName: %s\n", fileSize, filename);
					// tell peer that we got their request and we're okay with it
					SendNetMsg(FILE_REQ_ACK, "", 0);
					char *fileBuffer = malloc(fileSize);
					// receive file
					if (ReceiveFile(fileBuffer, fileSize)) { // if file receive did not time out, save it
						puts("Finished receiving file :)");
						FILE *file = fopen(filename, "wb");
						fwrite(fileBuffer, fileSize, 1, file);
						fclose(file);
					} else {
						puts("An error has occured! Could not receive file :(");
					}
					free(fileBuffer);
					break;
			}
		}
	}
	
	return 0;
}


bool HostInit(const uint16_t port) {
	struct sockaddr_in server_address;
	
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons(port);

	host.sock = socket(AF_INET, SOCK_STREAM, 0);

	setsockopt(host.sock, SOL_SOCKET, SO_REUSEADDR, &l_true, sizeof(l_true));
	
	struct sockaddr addr;
	memcpy(&addr, &server_address, sizeof(addr));

	if (bind(host.sock, &addr, sizeof(addr)) == 0) {
		//lastErr = BIND_ERR;
		//return false;
	} else {
		server_address.sin_port += 1;
		memcpy(&addr, &server_address, sizeof(addr));
		if (bind(host.sock, &addr, sizeof(addr)) != 0)
			return false;
	}
	// get user address
	char hostName[100];
	gethostname(hostName, 100);
	char myIp[16];
	struct hostent *he = gethostbyname(hostName);
	if (he != NULL) {
		struct in_addr **addr_list;
		
		addr_list = (struct in_addr **) he->h_addr_list;
		
		strcpy(myIp, inet_ntoa(*addr_list[0]));
		
		printf("Your ip is %s Port is %hu\n", myIp, ntohs(server_address.sin_port));
	} else {
		puts("Could not get host ip address, sorry!");
		printf("Your port is %hu\n", ntohs(server_address.sin_port));
	}
	listen(host.sock, 1); // listen for conns on server sock

	host.poll.fd = host.sock;
	host.poll.events = POLLIN; // get info if new people conn to server
	
	ResetPeerData(&host.peer);
	
	return true;
}

void ResetPeerData(struct Client *peer) {
	peer->sock = -1;
	peer->status = DISCONNECTED;
	host.poll.fd = host.sock;
	peer->poll.fd = peer->sock;
	peer->poll.events = POLLIN;
}

int ListenForConns(void) {
	int pollRet = poll(&host.poll, 1, 0); // get current sock poll info

	if (pollRet < 0) {
		//lastErr = BIND_ERR;
		return 0;
	} else if (pollRet == 0) {
		// no data to be read. do nothing
	} else {
		if (host.poll.revents == 0) // nothing to do
			return 0;

		if (host.poll.revents != POLLIN) { // unexpected result
			//lastErr = POLL_ERR_UNEXP_LISTEN;
			return 0;
		}
		int acceptRet = AcceptPeer();
		if (acceptRet == -1)
			return 0;
		//puts("Client connected!");
		AddPeer(acceptRet);
		return true;
	}
	return 0;
}

int AcceptPeer(void) {
	int newSock = accept(host.sock, NULL, NULL); // accept client
	if (newSock == -1) {
		//lastErr = POLL_ERR_ACCP;
		return -1;
	}
	return newSock;
}

void AddPeer(int sock) {
	host.peer.sock = sock;
	host.peer.status = CONNECTING;
	host.peer.poll.fd = sock;
}

int ListenForNetMsgs(char *buf) {
	int pollRet = poll(&host.peer.poll, 1, 0); // get current sock poll info
	//puts("polling");
	
	if (pollRet < 0) {
		//lastErr = BIND_ERR;
		
		return 0;
	} else if (pollRet == 0) {
		//puts("no data");
		// no data to be read. do nothing
	} else if ((pollRet > 0) && (host.peer.poll.revents & POLLIN)) {
		int bytesRead = recv(host.peer.sock, (void*)buf, STATIC_BUF_SIZE, 0);
		//printf("Read %d bytes\n", bytesRead);
		host.peer.poll.revents = 0;
		if (bytesRead <= 0) {
			puts("Peer has closed the connection!");
			return -1;
		}
		return bytesRead;
	}
	return 0;
}

void SendNetMsg(enum packet_type type, void *data, uint32_t size) {
	uint32_t fullSize = size + NETHEADER_SIZE;
	struct NetMsg *msg = malloc(fullSize);
	msg->size = size;
	msg->type = type;
	memcpy(msg->data, data, size);
	
	send(host.peer.sock, msg, fullSize, 0);
}

bool ConnectToPeer(const char *ip, uint16_t port) {
	// connect to peer
	struct sockaddr_in peerInfo;
	memset(&peerInfo, 0, sizeof(peerInfo));
	
	peerInfo.sin_family = AF_INET;
	peerInfo.sin_port = htons(port);
	peerInfo.sin_addr.s_addr = inet_addr(ip);
	
	AddPeer(socket(AF_INET, SOCK_STREAM, IPPROTO_IP));
	
	// connect socket to server
	if (connect(host.peer.sock, (struct sockaddr*)&peerInfo, sizeof(peerInfo)) == 0) {
		//puts("Successfully connected to peer.");
		host.peer.status = IDLING;
	} else {
		//puts("Could not connect to peer.");
		close(host.peer.sock);
		return false;
	}
	
	return true;
}

void SendFile(const char *filename) {
	FILE *file = fopen(filename, "rb");
	if (file == NULL) {
		printf("Could not open file '%s'. Consider checking the filename and rentering the information.\n", filename);
		return;
	}
	fseek(file, 0L, SEEK_END);
	int fileSize = ftell(file);
	rewind(file);
	// allocate buffer for file data
	char *fileData = malloc(fileSize);
	fread(fileData, fileSize, 1, file);
	struct FileInfo fileInfo = {fileSize};
	strcpy(fileInfo.filename, filename);
	SendNetMsg(FILE_HEADER, &fileInfo, sizeof(struct FileInfo)); // send file header
	
	while (true) { // wait for file head ack
		char buffer[STATIC_BUF_SIZE];
		if (ListenForNetMsgs(buffer)) {
			puts("Receieved message!!!!");
			if (((struct NetMsg*)buffer)->type == FILE_REQ_ACK) {
				// send file data
				SendNetMsg(FILE_DATA, fileData, fileSize);
				break;
			}
		}
	}
	
	puts("Finished sending file.");
	
	free(fileData);
}

bool ReceiveFile(char *fileBuffer, int fileSize) {
	printf("Progress:\n\n\n");
	int receivedBytes = 0;
	int receivedFileData = -NETHEADER_SIZE; // skip netheader
	
	int lastGivenProgress = -1;
	while (receivedFileData < fileSize) {
		char newBuffer[STATIC_BUF_SIZE];
		int bytesRead;
		if ((bytesRead = ListenForNetMsgs(newBuffer))) { // if we're in debt to the packet header, we're gonna have to pay it off with our received data
			if (receivedFileData < 0) {
				int diffHeaderSize = receivedFileData;
				memcpy(fileBuffer + CLAMP(0, receivedFileData, STATIC_BUF_SIZE), newBuffer + CLAMP(0, -receivedFileData, STATIC_BUF_SIZE), CLAMP(0, bytesRead - NETHEADER_SIZE, STATIC_BUF_SIZE));
				receivedBytes += bytesRead;
				receivedFileData += bytesRead;
			} else { // otherwise, the received stuff is just raw data :)
				memcpy(fileBuffer + receivedFileData, newBuffer, bytesRead);
				receivedBytes += bytesRead;
				receivedFileData += bytesRead;
			}
			float progress = CLAMP(0, ((float)receivedFileData/fileSize)*100, fileSize);
			if ((int)progress % (1) == 0 && (int)progress != lastGivenProgress) {
				//printf("%.2f%% done\n", progress);
				DrawDownloadProgress(progress);
				lastGivenProgress = (int)progress;
			}
		}
		
	}
	
	printf("\n");
	
	return true;
}

void DrawDownloadProgress(int percent) {
	struct text_info termInfo;
	c_gettextinfo(&termInfo);
	c_gotoxy(0, termInfo.cury - 2);
	for (int i = 0; i < termInfo.screenwidth - 1; i++) { // print top bar
		printf("-");
	}
	//c_gotoxy(0, termInfo.screenheight - 1);
	printf("\n");
	for (int i = 0; i < termInfo.screenwidth - 1; i++) { // print bottom bar
		if (((float)i/termInfo.screenwidth)*100 <= percent) // if the given percent of with is less than the percent progress
			printf("#"); // draw a bar
		else
			printf(" "); // otherwise draw a space
	}
	//c_gotoxy(0, termInfo.screenheight);
	printf("\n");
	for (int i = 0; i < termInfo.screenwidth - 1; i++) { // print progress bar
		printf("-");
	}
}