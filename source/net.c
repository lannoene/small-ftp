#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "address_dbg.h"

#ifdef _WIN32
#else
#include <poll.h>
#endif

int configNumTS = MAX_NTS;

struct Server {
	struct Client {
		enum conn_status status;
		struct sockaddr_in info;
		int sock[MAX_NTS];
#ifndef _WIN32
		struct pollfd poll[MAX_NTS];
#else
		// wsapoll stuff goes
#endif
	} peer;
	uint16_t port;
#ifndef _WIN32
	int sock, udpSock;
	struct pollfd poll;
#else
	SOCKET sock, udpSock;
	WSAEVENT pollEvent; // event to look for
	WSANETWORKEVENTS networkEvents;
	WSAPOLLFD pollFd;
#endif
	bool isHosting;
	char ip[17];
	int nIpNum;
	bool connected;
	int numTransferSockets;
} host;


#ifdef _WIN32

WSADATA wsaData;
unsigned long i_true = true;

bool HostInit(const uint16_t port) { // i wish i didn't ever have to use winsock
	host.connected = false;
	host.numTransferSockets = 1;
	
	if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
		puts("wsastartup failed");
		return false;
	}
	
	memset(host.peer.sock, -1, sizeof(int)*MAX_NTS);
	
	// create tcp sock
	if ((host.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == SOCKET_ERROR) {
		printf("socket() failed with error code: %d", WSAGetLastError());
	}
	
	// we want to reuse the same address because we only have one address
	/*if (setsockopt(host.sock, SOL_SOCKET, SO_REUSEADDR, (char *)&i_true, sizeof(i_true)) == SOCKET_ERROR) {
		puts("Could not set so reuseaddr");
	}*/
	// winsock event var
	host.pollEvent = WSACreateEvent();
	// with the listening socket and poll event listening for FD_ACCEPT
	WSAEventSelect(host.sock, host.pollEvent, FD_ACCEPT);
	//host.pollFd.events = POLLRDNORM;
	
	if (host.sock == INVALID_SOCKET) {
		printf("Error at socket(): %d\n", WSAGetLastError());
		WSACleanup();
		return false;
	}
	
	struct sockaddr_in sa;
	
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_port = htons(port);
	
	// Setup the TCP listening socket
	if (bind(host.sock, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR) {
		sa.sin_port++;
		if (bind(host.sock, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR) {
			puts("could not bind socket");
			return false;
		}
	}
	
	if (listen(host.sock, SOMAXCONN) == SOCKET_ERROR) {
		printf("Listen failed with error: %d\n", WSAGetLastError());
		closesocket(host.sock);
		WSACleanup();
		return false; 
	}
	
	// create udp sock (todo: it's not that important)
	if ((host.udpSock = socket(AF_INET, SOCK_DGRAM, 0)) == SOCKET_ERROR) {
		printf("socket() failed with error code: %d", WSAGetLastError());
	}
	
	if (ioctlsocket(host.udpSock, FIONBIO, &(unsigned long){1}) == SOCKET_ERROR) {
		printf("Could not set nonblocking sockets.");
	}
	struct sockaddr_in sin;
	int addrlen = sizeof(sin);
	if (getsockname(host.sock, (struct sockaddr *)&sin, &addrlen) == 0) {
		printf("Your port: %hu\n", ntohs(sin.sin_port));
	}
	return true;
}

int ListenForConns(void) {
	if (host.sock == INVALID_SOCKET) {
		puts("Could not listen, host socket is invalid!");
	}
	if (WSAWaitForMultipleEvents(1, &host.pollEvent, false, 0, false) == WSA_WAIT_EVENT_0) { // only event
		host.peer.sock[0] = accept(host.sock, NULL, NULL);
		if (ioctlsocket(host.peer.sock[0], FIONBIO, &i_true) == SOCKET_ERROR) {
			printf("Could not set nonblocking socket on peer: %d sock: %d.\n", WSAGetLastError(), host.peer.sock[0]);
		}
		if (WSAEnumNetworkEvents(host.sock, host.pollEvent, &host.networkEvents) > 0) { // reset net events?
			puts("error in wsaenumnetworkevents");
		}
		getpeername(host.peer.sock[0], (struct sockaddr*)&host.peer.info, &(int){sizeof(host.peer.info)});
		host.connected = true;
		return true;
	}
	return false;
}

bool ConnectToPeer(const char *ip, uint16_t port) {
	// connect to peer
	memset(&host.peer.info, 0, sizeof(host.peer.info));
	
	host.peer.info.sin_family = AF_INET;
	host.peer.info.sin_port = htons(port);
	host.peer.info.sin_addr.s_addr = inet_addr(ip);
	
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	host.peer.sock[0] = sock;
	host.peer.status = CONNECTING;
	
	//if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(unsigned char){1}, sizeof(unsigned char)) < 0)
	//	puts("setsockopt(SO_REUSEADDR) failed");
	
	// connect socket to server
	if (connect(host.peer.sock[0], (struct sockaddr*)&host.peer.info, sizeof(host.peer.info)) != SOCKET_ERROR) {
		host.peer.status = IDLING;
		host.connected = true;
		host.peer.info.sin_port = htons(port);
		printf("PORT: %hu\n", host.peer.info.sin_port);
		
		if (ioctlsocket(host.peer.sock[0], FIONBIO, &(unsigned long){1}) == SOCKET_ERROR) {
			printf("Could not set nonblocking sockets.\n");
		}
	} else {
		printf("Connect error: %d\n", WSAGetLastError());
		close(host.peer.sock[0]);
		return false;
	}
	
	return true;
}

// return bytes read if got data. otherwise, return 0

int64_t ListenForNetMsgsIdx(char *buf, int sockIdx) {
	int size;
	if ((size = recv(host.peer.sock[sockIdx], buf, STATIC_BUF_SIZE, 0)) != SOCKET_ERROR) {
		return size;
	} else {
		int error = WSAGetLastError();
		if (error == WSAECONNRESET) {
			printf("Connection was reset on socket %d\n", sockIdx);
			return -1;
		} else if (host.peer.sock[sockIdx] != -1 && error != WSAEWOULDBLOCK) {
			printf("Unhandled winsock expection: ListenForNetMsgsIdx: %d on %d\n", WSAGetLastError(), host.peer.sock[sockIdx]);
		}
	}
	return 0;
}

void AddExtraTransferSocket(void) {
	puts("Added extra transfer socket");
	++host.numTransferSockets;
	int i = host.numTransferSockets - 1;
	while (true) {
		if (WSAWaitForMultipleEvents(1, &host.pollEvent, false, 0, false) == WSA_WAIT_EVENT_0) { // only event
			host.peer.sock[i] = accept(host.sock, NULL, NULL);
			if (ioctlsocket(host.peer.sock[i], FIONBIO, &i_true) == SOCKET_ERROR) {
				printf("Could not set nonblocking socket on peer: %d sock: %d.\n", WSAGetLastError(), host.peer.sock[0]);
			}
			if (WSAEnumNetworkEvents(host.sock, host.pollEvent, &host.networkEvents) > 0) { // reset net events?
				puts("error in wsaenumnetworkevents");
			}
			break;
		}
	}
	
	/*if (setsockopt(host.peer.sock[i], SOL_SOCKET, SO_REUSEADDR, &(const char){1}, sizeof(const char)) == SOCKET_ERROR) {
		printf("setsockopt(SO_REUSEADDR) failed. error: %s\n", strerror(errno));
		exit(1);
	}*/
	if (ioctlsocket(host.peer.sock[i], FIONBIO, &(unsigned long){1}) == SOCKET_ERROR) {
		printf("Could not set nonblocking socket: %d %d (actual: %d)\n", i, WSAGetLastError(), host.peer.sock[i]);
	}
	SendNetCmdIdx(OK, i);
}

#include "menu.h"

void ConnectExtraTransferSockets(void) {
	uint16_t port;
	/*printf("Input port: ");
	char *portstr = CreateDialoguePopup("Input Port", "", 0);
	printf("got %s\n", portstr);
	port = atoi(portstr);
	free(portstr);*/
	puts("in new conn ts func");
	printf("Cur: %d conf: %d\n", host.numTransferSockets, configNumTS);
	
	for (int i = host.numTransferSockets; i < configNumTS; i++) {
		puts("Adding extra transfer socket");
		SendNetCmd(NEW_SOCK_CONN_REQ);
		host.peer.sock[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		/*if (setsockopt(host.peer.sock[i], SOL_SOCKET, SO_REUSEADDR, &(const char){1}, sizeof(const char)) < 0) {
			printf("setsockopt(SO_REUSEADDR) failed. error: %s\n", strerror(errno));
			exit(1);
		}*/
		// connect to peer
		struct sockaddr_in info;
		memset(&info, 0, sizeof(host.peer.info));
		
		info.sin_family = AF_INET;
		info.sin_port = host.peer.info.sin_port;
		info.sin_addr.s_addr = host.peer.info.sin_addr.s_addr;
		
		// connect socket to server
		if (connect(host.peer.sock[i], (struct sockaddr*)&info, sizeof(info)) != SOCKET_ERROR) {
			if (ioctlsocket(host.peer.sock[i], FIONBIO, &(unsigned long){1}) == SOCKET_ERROR) {
				printf("Could not set nonblocking socket: %d\n", i);
			}
		} else {
			printf("Could not connect socket to server. Error: %d\n", WSAGetLastError());
		}
		
		WaitForOKIdx(i);
		puts("Added extra transfer socket");
	}
	host.numTransferSockets = configNumTS;
	puts("exiting conn ts func");
}

#else
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/socket.h>

long l_true = true; // socket options jank
long l_false = false;

static void ResetPeerData(struct Client *peer);

bool HostInit(const uint16_t port) {
	// init tcp
	host.connected = false;
	host.numTransferSockets = 1;
	
	struct sockaddr_in server_address;
	struct sockaddr addr;
	
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons(port);

	host.sock = socket(AF_INET, SOCK_STREAM, 0);

	if (setsockopt(host.sock, SOL_SOCKET, SO_REUSEADDR, &l_true, sizeof(l_true)) < 0) {
		printf("SET SOCK OPT FAILED, %s\n", strerror(errno));
	}
	
	memcpy(&addr, &server_address, sizeof(addr));

	if (bind(host.sock, &addr, sizeof(addr)) == 0) {
		//lastErr = BIND_ERR;
		//return false;
	} else {
		server_address.sin_port += 1;
		memcpy(&addr, &server_address, sizeof(addr));
		if (bind(host.sock, &addr, sizeof(addr)) != 0) {
			return false;
		}
	}
	// get user address
	char hostName[100];
	gethostname(hostName, 100);
	char myIp[16];
	struct hostent *he = gethostbyname(hostName);
	if (he != NULL) {
		struct in_addr **addr_list;
		
		addr_list = (struct in_addr **) he->h_addr_list;
		
		strcpy(host.ip, inet_ntoa(*addr_list[0]));
		host.nIpNum = addr_list[0]->s_addr;
		
		printf("Your ip is %s Port is %hu\n", host.ip, ntohs(server_address.sin_port));
	} else {
		puts("Could not get host ip address, sorry!");
		printf("Your port is %hu\n", ntohs(server_address.sin_port));
	}
	listen(host.sock, 1); // listen for conns on server sock

	host.poll.fd = host.sock;
	host.poll.events = POLLIN; // get info if new people conn to server
	host.port = ntohs(server_address.sin_port);
	
	ResetPeerData(&host.peer);
	
	// init udp
	
	if ((host.udpSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		puts("Could not create udp socket");
	}
	
	memset(&server_address, 0, sizeof(server_address));
    
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons(UDP_PORT);

	// Bind the socket with the server address 
	if (bind(host.udpSock, (const struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
		puts("Could not bind udp socket. This is essential for network discovery!");
		server_address.sin_port = htons(UDP_PORT + 1);
		if (bind(host.udpSock, (const struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
			return false;
		}
	}
	if (fcntl(host.udpSock, F_SETFL, O_NONBLOCK) != 0) {
		puts("Could not set socket to unblocking mode!");
		return false;
	}
	return true;
}

static void ResetPeerData(struct Client *peer) {
	peer->sock[0] = -1;
	peer->status = DISCONNECTED;
	host.poll.fd = host.sock;
	peer->poll->fd = *(peer->sock);
	peer->poll->events = POLLIN;
}


static void AddPeer(int sock) {
	host.peer.sock[0] = sock;
	host.peer.status = CONNECTING;
	host.peer.poll[0].fd = sock;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
		puts("setsockopt(SO_REUSEADDR) failed");
}

static int AcceptPeer(void) {
	int newSock = accept(host.sock, NULL, NULL); // accept client
	if (newSock == -1) {
		//lastErr = POLL_ERR_ACCP;
		return -1;
	}
	host.connected = true;
	return newSock;
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

		if (!(host.poll.revents & POLLIN)) { // unexpected result
			//lastErr = POLL_ERR_UNEXP_LISTEN;
			return 0;
		}
		int acceptRet = AcceptPeer();
		if (acceptRet == -1)
			return 0;
		//puts("Client connected!");
		AddPeer(acceptRet);
		struct sockaddr_in my_addr;
		socklen_t len = sizeof(my_addr);
		if (getpeername(acceptRet, (struct sockaddr *)&my_addr, &len) == 0) {
			host.peer.info.sin_addr.s_addr = my_addr.sin_addr.s_addr;
			host.peer.info.sin_port = my_addr.sin_port;
			printf("new host with port: %hu connected (actual %hu)\n", my_addr.sin_port, htons(DEFAULT_PORT) + 1);
		} else {
			host.numTransferSockets = 1;
			puts("Could not resolve ip address/port of peer! This is nothing to worry about, but it means that the NTS must be 1 or a crash will occur.");
		}
		return true;
	}
	return 0;
}

int64_t ListenForNetMsgsIdx(char *buf, int sockIdx) {
	int pollRet = poll(&host.peer.poll[sockIdx], 1, 0); // get current sock poll info
	//puts("polling");
	
	if (pollRet < 0) {
		return 0;
	} else if ((pollRet > 0) && (host.peer.poll[sockIdx].revents & POLLIN)) {
		uint64_t bytesRead = recv(host.peer.sock[sockIdx], (void*)buf, STATIC_BUF_SIZE, 0);
		//printf("Read %d bytes\n", bytesRead);
		host.peer.poll[sockIdx].revents = 0;
		if (bytesRead <= 0) {
			puts("Peer has closed the connection!");
			return -1;
		}
		return bytesRead;
	}
	return 0;
}

bool ConnectToPeer(const char *ip, uint16_t port) {
	// connect to peer
	memset(&host.peer.info, 0, sizeof(host.peer.info));
	
	host.peer.info.sin_family = AF_INET;
	host.peer.info.sin_port = htons(port);
	host.peer.info.sin_addr.s_addr = inet_addr(ip);
	
	AddPeer(socket(AF_INET, SOCK_STREAM, 0));
	
	// connect socket to server
	if (connect(host.peer.sock[0], (struct sockaddr*)&host.peer.info, sizeof(host.peer.info)) == 0) {
		host.peer.status = IDLING;
		host.connected = true;
		host.peer.info.sin_port = htons(port);
		printf("PORT: %hu\n", host.peer.info.sin_port);
	} else {
		printf("Connect failed on %d: %s\n", host.peer.sock[0], strerror(errno));
		close(host.peer.sock[0]);
		return false;
	}
	
	return true;
}

void AddExtraTransferSocket(void) {
	puts("Added extra transfer socket");
	++host.numTransferSockets;
	host.peer.sock[host.numTransferSockets - 1] = accept(host.sock, NULL, NULL);
	
	if (setsockopt(host.peer.sock[host.numTransferSockets - 1], SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
		printf("setsockopt(SO_REUSEADDR) failed. error: %s\n", strerror(errno));
		exit(1);
	}
	host.peer.poll[host.numTransferSockets - 1].fd = host.peer.sock[host.numTransferSockets - 1];
	host.peer.poll[host.numTransferSockets - 1].events = POLLIN;
	
	SendNetCmdIdx(OK, host.numTransferSockets - 1);
}

void ConnectExtraTransferSockets(void) {
	char ipAddr[50];
	snprintf(ipAddr, 50, "%d.%d.%d.%d",
	(int)(host.peer.info.sin_addr.s_addr&0xFF),
	(int)((host.peer.info.sin_addr.s_addr&0xFF00)>>8),
	(int)((host.peer.info.sin_addr.s_addr&0xFF0000)>>16),
	(int)((host.peer.info.sin_addr.s_addr&0xFF000000)>>24));
	printf("port is: %d ip is %s\n", (int) ntohs(host.peer.info.sin_port), ipAddr);
	
	printf("real port: %hu, sussy port: %hu\n", htons(DEFAULT_PORT) + 1, host.peer.info.sin_port);
	
	uint16_t port;
	printf("Input port: ");
	scanf("%hu", &port);
	
	printf("Cur: %d conf: %d\n", host.numTransferSockets, configNumTS);
	
	for (int i = host.numTransferSockets; i < configNumTS; i++) {
		puts("Adding extra transfer socket");
		SendNetMsg(NEW_SOCK_CONN_REQ, "", 0);
		host.peer.sock[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		if (setsockopt(host.peer.sock[i], SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
			printf("setsockopt(SO_REUSEADDR) failed. error: %s\n", strerror(errno));
			exit(1);
		}
		// connect to peer
		struct sockaddr_in info;
		memset(&info, 0, sizeof(host.peer.info));
		
		info.sin_family = AF_INET;
		info.sin_port = htons(port);
		info.sin_addr.s_addr = host.peer.info.sin_addr.s_addr;
		
		// connect socket to server
		if (connect(host.peer.sock[i], (struct sockaddr*)&info, sizeof(info)) == 0) {
			host.peer.poll[i].fd = host.peer.sock[i];
			host.peer.poll[i].events = POLLIN;
		} else { // should be unused for now
			info.sin_port = htons(DEFAULT_PORT) + 1;
			if (connect(host.peer.sock[i], (struct sockaddr*)&info, sizeof(info)) == 0) {
				host.peer.poll[i].fd = host.peer.sock[i];
			}
			// error
			puts("COULD NOT CONNECT EXTRA TRANSFER SOCKETS. Please turn nts to 1 and restart the peer. This program will now exit.");
			exit(1);
			return;
		}
		
		WaitForOKIdx(i);
		puts("Added extra transfer socket");
	}
	host.numTransferSockets = configNumTS;
}

#endif

int GetConfigNTS(void) {
	return configNumTS;
}

void SetConfigNTS(int cnts) {
	configNumTS = cnts;
}

void SendNetCmd(enum packet_type type) {
	SendNetCmdIdx(type, 0);
}

void SendNetCmdIdx(enum packet_type type, int sockId) {
	SendNetMsgIdx(type, "", 0, sockId);
}

bool IsHost(void) {
	return host.isHosting;
}

void SetHost(bool b_host) {
	host.isHosting = b_host;
}

uint16_t GetPort(void) {
	return host.port;
}

void GetIp(char *buf) {
	strcpy(buf, host.ip);
}

int GetNIpNum(void) {
	return host.nIpNum;
}

bool IsConnected(void) {
	return host.connected;
}

void SetNTS(int numTransfer) {
	host.numTransferSockets = numTransfer;
}

int GetNTS(void) {
	return host.numTransferSockets;
}

void WaitForOK(void) {
	WaitForOKIdx(0);
}

void WaitForOKIdx(int sockIdx) {
	while (true) {
		char buffer[STATIC_BUF_SIZE];
		if (ListenForNetMsgsIdx(buffer, sockIdx)) {
			if (((struct NetMsg*)buffer)->type == OK) {
				return;
			}
		}
	}
}

int64_t ListenForNetMsgs(char *buf) {
	return ListenForNetMsgsIdx(buf, 0);
}

void SendNetMsg(enum packet_type type, const void *data, uint64_t size) {
	SendNetMsgIdx(type, data, size, 0);
}

void SendUdpMsgIp(const enum udp_pack_type type, const void *data, const size_t size, const char *ip, const uint16_t port) {
	addressInfo info;
	info.sin_family = AF_INET;
    info.sin_port = htons(port);
    info.sin_addr.s_addr = inet_addr(ip);
	
	SendUdpMsgSAddr(type, data, size, info);
}

void SendUdpMsgSAddr(const enum udp_pack_type type, const void *data, const size_t size, addressInfo addr) {
	uint64_t fullSize = size + NETHEADER_SIZE;
	struct NetMsg *msg = malloc(fullSize);
	msg->size = size;
	msg->type = type;
	memcpy(msg->data, data, size);
	
	sendto(host.udpSock, (const char *)msg, fullSize, 0, (struct sockaddr*)&addr, sizeof(addr));
	
	free(msg);
}

bool ListenForUdpMsgs(char *buffer, addressInfo *addr) {
	char sockBuffer[65507] = {0};
	int bytesReceived = 0;
	int sizeofSockaddr = sizeof(struct sockaddr_in);
	
	bytesReceived = recvfrom(host.udpSock, sockBuffer, 65507, 0, (struct sockaddr*)addr, &sizeofSockaddr);
	if (bytesReceived > 0) {
		memcpy(buffer, sockBuffer, bytesReceived);
		return true;
	}
	return false;
}

void SendNetMsgIdx(enum packet_type type, const void *data, uint64_t size, int sockIdx) {
	uint64_t fullSize = size + NETHEADER_SIZE;
	struct NetMsg *msg = malloc(fullSize);
	msg->size = size;
	msg->type = type;
	memcpy(msg->data, data, size);
	
	send(host.peer.sock[sockIdx], (char*)msg, fullSize, 0);
	free(msg);
}

void DisconnectExtraTransferSockets(void) {
	for (int i = 1; i < host.numTransferSockets; i++) {
		close(host.peer.sock[i]);
	}
	host.numTransferSockets = 1;
}

char *ListenForNetMsgsLazy(int64_t maxTime) {
	static char buffer[STATIC_BUF_SIZE];
	int64_t bytesRecv = 0;
	while ((bytesRecv = ListenForNetMsgs(buffer)) <= 0) {
		
	}
	return buffer;
}