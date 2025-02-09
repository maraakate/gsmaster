#ifndef MASTER_H
#define MASTER_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

#define PORTREUSE SO_REUSEADDR
void SetGSMasterRegKey (const char* name, const char *value);
void GetGSMasterRegKey (const char* name, const char *value);
typedef int socklen_t;
#define selectsocket(x, y, z, a, b) select((int)x, y, z, a, b) /* FS: nfds is fake on Windows.  Cast it to int so x64 VS2005 shuts up */

#else

// Linux and Mac versions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>

#ifndef __DJGPP__
	#include <sys/signal.h>
#else
	#include <tcp.h>
	#include <signal.h>
#endif /* __DJGPP__ */

#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/ioctl.h>

enum {FALSE, TRUE};

// stuff not defined in sys/socket.h
#ifndef SOCKET
#define SOCKET int
#endif /* SOCKET */

#ifdef __DJGPP__
#define selectsocket select_s
typedef int socklen_t;
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
#define strnicmp strncasecmp
#define stricmp strcasecmp
#define gsmaster_main main
#define closesocket close
#define SetGSMasterRegKey(x,y)
#define GetGSMasterRegKey(x,y)
#define WSACleanup()
void signal_handler(int sig);

#endif

#include "shared.h"

#define	S2C_CHALLENGE		'c'
#define	M2C_SERVERLST		'd'
#define A2A_PING			'k'
#define	A2A_ACK				'l'
#define	S2M_SHUTDOWN		'C'
#define S2M_HEARTBEAT		'a'

#define MAX_GSPY_MTU_SIZE 1022
#define GSPY_BUFFER_GROWBY_SIZE 511

#define GSPY_DB_VERSION 1

#define NETQUAKE_MAXSTR	64
typedef struct netquake_data_s netquake_data_t;

struct netquake_data_s
{
	char	ip[NETQUAKE_MAXSTR];
	char	hostname[NETQUAKE_MAXSTR];
	char	mapname[NETQUAKE_MAXSTR];
	int		users;
	int		maxusers;
	int		protocol;
};

typedef struct server_s server_t;

struct server_s
{
	server_t		*prev;
	server_t		*next;
	struct sockaddr_in	ip;
	unsigned short	port;
	unsigned short	gs3dport; /* FS: Mostly for Sin with GS3D. */
	unsigned int	queued_pings;
	unsigned int	heartbeats;
	unsigned long	last_heartbeat;
	unsigned long	last_ping;
	unsigned char	shutdown_issued;
	bool		validated;
	char	gamename[MAX_GAMENAME_LEN];
	char	challengeKey[64]; /* FS: Needed for GameSpy validation. */
	char	hostnameIp[MAX_DNS_NAMELEN+1];
	char	serverInfo[4000];
	netquake_data_t	nqData;
};

typedef struct gamespy_filter_s gamespy_filter_t;

struct gamespy_filter_s
{
	char	gametype[64];
	char	gamemode[64];
	char	mapname[64];
	int		location;
	char	advancedQuery[4000];
};

void ParseCommandLine (int argc, char **argv);
void Add_Servers_From_List (const char *filename, char *gamename);

#endif /* MASTER_H */
