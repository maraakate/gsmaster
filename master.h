#ifndef MASTER_H
#define MASTER_H

#ifdef _WIN32

#define PORTREUSE SO_REUSEADDR
void SetQ2MasterRegKey(char* name, char *value);
void GetQ2MasterRegKey(char* name, char *value);
typedef int socklen_t;
#define selectsocket select
#define stricmp _stricmp
#define strdup _strdup

#else

// Linux and Mac versions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#ifndef __FreeBSD__
#include <tcp.h>
#endif

#ifndef __DJGPP__
	#include <sys/signal.h>
#else
	#include <signal.h>
#endif /* __DJGPP__ */

#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <assert.h>

enum {FALSE, TRUE};

// stuff not defined in sys/socket.h
#ifndef SOCKET
#define SOCKET unsigned int
#endif /* SOCKET */

#ifdef __DJGPP__
#define selectsocket select_s
extern int	_watt_do_exit;	/* in sock_ini.h, but not in public headers. */
#else
#define selectsocket select
#endif /* __DJGPP__ */

#ifndef SOCKET_ERROR
	#define SOCKET_ERROR -1
#endif

#ifndef INVALID_SOCKET
	#define INVALID_SOCKET -1
#endif

#define TIMEVAL struct timeval
#define ioctlsocket ioctl

#ifdef __FreeBSD__
#define PORTREUSE SO_REUSEPORT
#else
#define PORTREUSE SO_REUSEADDR
#endif

// portability, rename or delete functions
#define _strnicmp strncasecmp
#define _stricmp strcasecmp
#define stricmp strcasecmp
#define My_Main main
#define closesocket close // FS
#define SetQ2MasterRegKey(x,y)
#define	GetQ2MasterRegKey(x,y)
#define WSACleanup()
void signal_handler(int sig);

#endif

#include "shared.h"

typedef struct server_s server_t;

typedef enum {waiting, inuse} pingstate;
struct server_s
{
	server_t		*prev;
	server_t		*next;
	struct sockaddr_in	ip;
	unsigned short	port;
	unsigned int	queued_pings;
	unsigned int	heartbeats;
	unsigned long	last_heartbeat;
	unsigned long	last_ping;
	unsigned char	shutdown_issued;
	int		validated; // FS: Changed from unsigned char to int
	char	gamename[MAX_GAMENAME_LEN]; // FS: Added
	char	challengeKey[64]; // FS: For gamespy encode type 0 validation
	char	hostnameIp[MAX_DNS_NAMELEN+1]; // FS: If server was added from a list
};

#if 0
struct querySocket_s
{
	SOCKET	socket;
	pingstate state;
};
#endif

void RunFrame (void);
void ExitNicely (void);
void DropServer (server_t *server);
int  AddServer (struct sockaddr_in *from, int normal, unsigned short queryPort, char *gamename, char *hostnameIp); // FS: Added queryPort, gamename, and hostnameIp
void QueueShutdown (struct sockaddr_in *from, server_t *myserver);
void Ack (struct sockaddr_in *from, char *dataPacket);
int  HeartBeat (struct sockaddr_in *from, char *data);
void ParseCommandLine(int argc, char *argv[]);
int ParseResponse (struct sockaddr_in *from, char *data, int dglen);

/* FS: Gamespy specific helper functions */
void SendGamespyListToClient (int socket, char *gamename, struct sockaddr_in *from, int basic);
void Close_TCP_Socket_On_Error (int socket, struct sockaddr_in *from);
void Gamespy_Parse_List_Request(char *clientName, char *querystring, int socket, struct sockaddr_in *from);
int Gamespy_Challenge_Cross_Check(char *challengePacket, char *validatePacket, int rawsecurekey);
void Gamespy_Parse_UDP_Packet(int socket, struct sockaddr_in *from);
void Gamespy_Parse_TCP_Packet (int socket, struct sockaddr_in *from);
int UDP_OpenSocket (int port);

#ifdef QUAKE2_SERVERS
void SendServerListToClient (struct sockaddr_in *from);
#endif /* QUAKE2_SERVERS */

void Add_Servers_From_List(char *filename); // FS
void Check_Port_Boundaries (void); // FS
struct in_addr Hostname_to_IP (struct in_addr *server, char *hostnameIp); // FS: For serverlist

#endif /* MASTER_H */
