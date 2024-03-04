#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h> // lets us use close function
#include <stdlib.h>
#include <math.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <poll.h>

#define MAX_CLIENTS 10
#define DEFAULT_PORT 4659
#define STATIC_BUF_SIZE 4096

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
int ListenForNetMsgs(const char *buf);
bool ConnectToPeer(const char *ip, uint16_t port);
void SendNetMsg(enum packet_type type, void *data, uint32_t size);
void ReceiveFile();
void SendFile(const char *filename);

bool sendFile = false;

int main(int argc, char* argv[]) {
	if (!HostInit(DEFAULT_PORT))
		puts("Could not init socket! Sorry! (this probably means that the ports have already been taken)");
	
	puts("Enter mode: (r recv, s send)");
	while (true) {
		char opt = getchar();
		if (opt == 's') {
			sendFile = true;
			puts("Send mode");
			char sendFilename[50];
			puts("Enter filename to send:");
			scanf("%s", sendFilename);
			puts("Enter ip & port to send to (fmt ip:port or type localhost):");
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
			if (ConnectToPeer(sendToIp, sendToPort)) {
				SendFile(sendFilename);
			}
			break;
		} else if (opt == 'r') {
			puts("Receive mode");
			break;
		}
	}
	//puts("key presesd");
	
	while (!b_exit) {
		bool hasFoundConn = false;
		if (!sendFile) { // if we aren't sending a file listen
			while (!hasFoundConn) {
				hasFoundConn = ListenForConns();
			}
			printf("Peer connected!\n");
			bool listenForData = true;
			while (listenForData) {
				char buffer[STATIC_BUF_SIZE];
				if (ListenForNetMsgs(buffer)) {
					/*FILE* file = fopen("file.bin", "wb");
					fwrite(buffer, 44, 1, file);
					fclose(file);*/
					enum packet_type type = ((struct NetMsg*)buffer)->type;
					char *packData = ((struct NetMsg*)buffer)->data;
					uint32_t packSize = ((struct NetMsg*)buffer)->size;
					switch (type) {
						default:
							printf("Unkown data type: %d", type);
							break;
						case FILE_HEADER:
							int fileSize = ((struct FileInfo*)(packData))->size;
							char filename[30];
							strncpy(filename, ((struct FileInfo*)(packData))->filename, 30);
							printf("Downloading file:\nSize: %d\nName: %s\n", fileSize, filename);
							SendNetMsg(FILE_REQ_ACK, "", 0);
							char *fileBuffer = malloc(fileSize);
							int receivedBytes = 0;
							int receivedFileData = -NETHEADER_SIZE; // skip netheader
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
								}
								
							}
							puts("Finished receiving file :)");
							FILE *file = fopen(filename, "wb");
							fwrite(fileBuffer, fileSize, 1, file);
							fclose(file);
							free(fileBuffer);
							break;
					}
				}
			}
			puts("Finished?");
		} else { // if we are, actively connect
			
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
	
	printf("Port is %hu\n", ntohs(server_address.sin_port));

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
		return 1;
	} else if (pollRet == 0) {
		// no data to be read. do nothing
	} else {
		if (host.poll.revents == 0) // nothing to do
			return 0;

		if (host.poll.revents != POLLIN) { // unexpected result
			//lastErr = POLL_ERR_UNEXP_LISTEN;
			return 1;
		}
		int acceptRet = AcceptPeer();
		if (acceptRet == -1)
			return 1;
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

int ListenForNetMsgs(const char *buf) {
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
		printf("Read %d bytes\n", bytesRead);
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
		puts("Successfully connected to peer.");
		host.peer.status = IDLING;
	} else {
		puts("Could not connect to peer.");
		close(host.peer.sock);
		return false;
	}
	
	return true;
}

void SendFile(const char *filename) {
	FILE *file = fopen(filename, "rb");
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