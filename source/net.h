#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#ifdef _WIN32
#include <Winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <arpa/inet.h>
#endif
#define DEFAULT_PORT 4659
#define UDP_PORT 4410
#define NETHEADER_SIZE ((sizeof(uint64_t) + sizeof(enum packet_type)))
#define CLAMP(min, d, max) \
	((d < min ? min : d) > max ? max : (d < min ? min : d))
#define STATIC_BUF_SIZE 4096

#define MAX_NTS 10

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
	OK,
	MAKE_DIRECTORY,
	CHDIR,
	NEW_SOCK_CONN_REQ,
	EXT_SOCK_DISCONN,
	START_DIR_SENDING,
	END_DIR_SENDING,
	REQ_DIR_LIST,
	DIR_LIST_END,
	DIR_LIST_ELEM,
	REQ_FILE,
	REQ_CONN_NEW_TS,
	REQ_DIR,
};

enum udp_pack_type {
	UDP_PING
};

struct NetMsg {
	uint64_t size;
	int type;
	char data[];
};

#ifdef _WIN32
typedef struct sockaddr_in addressInfo;
#else
typedef struct sockaddr_in addressInfo;
#endif

bool HostInit(const uint16_t port);
void PingIpAddress(const char *ip, const uint16_t port);
bool ListenForUdpMsgs(char *buffer, addressInfo *addr);
void SendUdpMsgIp(const enum udp_pack_type type, const void *data, const size_t size, const char *ip, const uint16_t port);
void SendUdpMsgSAddr(const enum udp_pack_type type, const void *data, const size_t size, addressInfo addr);
void WaitForOK(void);
void WaitForOKIdx(int sockIdx);
bool ConnectToPeer(const char *ip, uint16_t port);
int64_t ListenForNetMsgs(char *buf);
int64_t ListenForNetMsgsIdx(char *buf, int sockIdx);
void SendNetMsg(enum packet_type type, const void *data, uint64_t size);
void SendNetMsgIdx(enum packet_type type, const void *data, uint64_t size, int sockIdx);
int ListenForConns(void);
bool IsHost(void);
void SetHost(bool b_host);
uint16_t GetPort(void);
void GetIp(char *buf);
void GetIp(char *buf);
int GetNIpNum(void);
bool IsConnected(void);
void SetNTS(int numTransfer);
int GetNTS(void);
void ConnectExtraTransferSockets(void);
void DisconnectExtraTransferSockets(void);
int64_t ListenForNetMsgsExt(char *buf, int sockId);
int GetConfigNTS(void);
void SetConfigNTS(int cnts);
void AddExtraTransferSocket(void);
void SendNetCmd(enum packet_type type);
void SendNetCmdIdx(enum packet_type type, int sockId);
char *ListenForNetMsgsLazy(int64_t maxTime);