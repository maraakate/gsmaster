/*
* Copyright (C) 2015-2021 Frank Sapone
* Copyright (C) 2002-2003 r1ch.net
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <winerror.h>
#include <time.h>
#include <process.h>
#include <direct.h>
#if defined(_MSC_VER) && _MSC_VER < 1400 /* FS: VS2005 Compatibility */
#include <winwrap.h>
#endif

#include <errno.h>

#include "service.h"

// Windows Service structs
static SERVICE_STATUS          MyServiceStatus;
static SERVICE_STATUS_HANDLE   MyServiceStatusHandle;
#endif

#include "master.h"
#include "dk_essentials.h"
#include "gamestable.h"

/* FS: IF YOU DON'T NEED THIS DISABLE IT OTHERWISE ADAPT TO YOUR HOSTNAME ACCORDINGLY!
 *     THIS IS INTENDED FOR THOSE THAT RUN DED SERVERS ON THE SAME IP AS THE MASTER SERVER!
 */
//#define HOSTNAME_AND_LOCALHOST_HACK
#ifdef HOSTNAME_AND_LOCALHOST_HACK
static const char HostnameHack[] = "Maraakate.org";
#endif

// for debugging as a console application in Windows or in Linux
int debug;
int timestamp;
#ifdef _MSC_VER /* FS: Not on mingw. */
bool bMinidumpAutogen;
#endif
static bool bSendGameSpyAck;
static bool bHttpEnable;
static unsigned long numservers;	// global count of the currently listed servers

static int runmode;	// server loop control

static server_t servers;

static struct sockaddr_in listenaddress;
static struct sockaddr_in listenaddressTCP;
static SOCKET out;
static SOCKET listener;
static SOCKET listenerTCP;
static SOCKET newConnection;
static SOCKET maxConnections;
static TIMEVAL delay;

#ifdef _WIN32
static WSADATA ws;
#endif

static fd_set set;
static fd_set master;

static char incoming[MAX_INCOMING_LEN];
static char incomingTcpValidate[MAX_INCOMING_LEN];
static char incomingTcpList[MAX_INCOMING_LEN];
static char rconPassword[KEY_LEN];
static SOCKET tcpSocket;
static int totalRetry = 10; /* FS: Total retry attempts waiting for the GameSpy validate stuff */
static unsigned long heartbeatInterval = DEFAULTHEARTBEAT; /* FS: Time (in minutes) before sending the next status packet */

static char bind_ip[KEY_LEN] = "0.0.0.0"; // default IP to bind
static char bind_port[KEY_LEN] = "27900";	// default port to bind
static char bind_port_tcp[KEY_LEN] = "28900";	/* FS: default TCP port to bind */
static char serverlist_filename[MAX_PATH] = ""; /* FS: For a list of servers to add at startup */
static char masterserverlist_filename[MAX_PATH] = ""; /* FS: For a list of master servers to add at startup */
static char httpdl_filename[MAX_PATH] = ""; /* FS: Scrape HTTP lists. */
static char logtcp_filename[MAX_PATH] = LOGTCP_DEFAULTNAME;
static int load_Serverlist;
static int load_MasterServerlist;

static bool bValidate_newserver_immediately;
static int validation_required;
static bool bMotd;
static bool bLogTCP;
static unsigned int minimumHeartbeats = 2; /* FS: Minimum number of heartbeats required before we're added to the list, used to verify it's a real server. */
static double lastMasterListDL;

static char serverListGenerationPath[MAX_PATH] = "";
static double lastServerListGeneration;
static double serverListGenerationTime = DEFAULTSERVERLISTGENERATIONTIME;
static bool bGenerateServerList;

/* FS: For GameSpy list */
static const char listheader[] = "\\";
static const char finalstring[] = "final\\";
static const char finalstringerror[] = "\\final\\";
static const char statusstring[] = "\\status\\secure\\";
static const char quakeworldquake2statusstring[] = OOB_SEQ "status"; /* FS: QW and Q2 use this */
static const char quake1querystring[] = "\x80\x00\x00\x0C\x02" "QUAKE" "\x00"; /* FS: Raw data that's sent down for a "QUAKE" query string.  NOTE HAS A TRAILING ZERO! OTHERS DO NOT! */
static const char hexen2querystring[] = "\x80\x00\x00\x0D\x02" "HEXENII"; /* FS: Raw data that's sent down for a "HEXENII" query string */
static const char hexenworldstatusstring[] = OOB_SEQ "\xff" "status"; /* FS: HW wants an extra 0xff */
static const char challengeHeader[] = "\\basic\\\\secure\\"; /* FS: This is the start of the handshake */

static const char ackstring[] = OOB_SEQ "ack";
static const int ackstringlen = sizeof(ackstring) - 1;

static const char daikatanagetserversstring[] = OOB_SEQ "getservers daikatana";
static const int daikatanagetserverslen = sizeof(daikatanagetserversstring) - 1;

/* FS: Need these two for Parse_UDP_MS_List because of the strlwr in AddServer */
static char quakeworld[] = "quakeworld";
static char quake2[] = "quake2";
static char hexenworld[] = "hexenworld";

/* FS: Re-adapted from uhexen2 */
static const unsigned char hw_hwq_msg[] =
		{ 255, S2C_CHALLENGE, '\0' };

static const unsigned char hw_gspy_msg[] =
		{ 255, S2C_CHALLENGE, '\n' };

static const unsigned char qw_msg[] =
		{ S2C_CHALLENGE, '\n' };

static const unsigned char hw_server_msg[] =
		{ 255, A2A_PING, '\0' };

static const unsigned char hw_hearbeat_msg[] =
		{ 255, S2M_HEARTBEAT, '\0' };

static const unsigned char hw_ack_msg[] =
		{ 255, 255, 255, 255, 255, A2A_ACK, '\0' };

static const unsigned char qw_server_msg[] =
		{ A2A_PING, '\0' };

static const unsigned char qw_hearbeat_msg[] =
		{ S2M_HEARTBEAT, '\0' };

static const unsigned char qw_ack_msg[] =
		{ 255, 255, 255, 255, A2A_ACK, '\0' };

static const unsigned char hw_server_shutdown[] =
		{ 255, S2M_SHUTDOWN, '\n' };

static const unsigned char qw_server_shutdown[] =
		{ S2M_SHUTDOWN, '\n' };

static const unsigned char qspy_req_msg[] =
		{ 'D', '\n' };

static const unsigned char hw_reply_hdr[] =
		{ 255, 255, 255, 255, 255, M2C_SERVERLST, '\n' };

static const unsigned char qw_reply_hdr[] =
		{ 255, 255, 255, 255, M2C_SERVERLST, '\n' };

static const unsigned char qw_reply_hdr2[] =
		{ 255, 255, 255, 255, M2C_SERVERLST, '\0' };

static const unsigned char q2_reply_hdr[] =
		{ 255, 255, 255, 255, 's', 'e', 'r', 'v', 'e', 'r', 's', ' '};

static const unsigned char q2_msg[] =
		{ 255, 255, 255, 255, 'g', 'e', 't', 's', 'e', 'r', 'v', 'e', 'r', 's', '\0'};

static const unsigned char q2_msg_alternate[] =
		{ 255, 255, 255, 255, 'q', 'u', 'e', 'r', 'y', '\0'};

static const unsigned char q2_msg_noOOB[] =
		{ 'g', 'e', 't', 's', 'e', 'r', 'v', 'e', 'r', 's', '\0'};

static const unsigned char q2_msg_alternate_noOOB[] =
		{ 'q', 'u', 'e', 'r', 'y', '\0'};

static bool GameSpy_Challenge_Cross_Check(char *challengePacket, char *validatePacket, char *challengeKey, int rawsecurekey, int enctype);
static void GameSpy_Parse_TCP_Packet (SOCKET socket, struct sockaddr_in *from);
static void Parse_UDP_Packet (SOCKET connection, int len, struct sockaddr_in *from);
static void Check_Port_Boundaries (void);
static struct in_addr Hostname_to_IP (struct in_addr *server, char *hostnameIp);
static void RunFrame (void);
static void Rcon (struct sockaddr_in *from, char *queryString);
static void HTTP_DL_List(void);
static void Master_DL_List(char *filename);
static void Parse_UDP_MS_List (unsigned char *tmp, char *gamename, int size);
static void GenerateServersList (void);
static void GenerateMasterDBBlob (void);
static void ReadMasterDBBlob (void);

static const char *GetValidationRequiredString (void)
{
	if (validation_required >= GAMESPY_VALIDATION_REQUIRED_ALL)
	{
		return "3 - All";
	}
	else if (validation_required == GAMESPY_VALIDATION_REQUIRED_SERVERS_ONLY)
	{
		return "2 - Servers Only";
	}
	else if (validation_required == GAMESPY_VALIDATION_REQUIRED_CLIENTS_ONLY)
	{
		return "1 - Clients Only";
	}

	return "0 - Disabled";
}

static void PrintBanner (void)
{
	printf("GSMaster v%s.  A GameSpy Encode Type 0 and Type 1 Emulator Master Server.\nBased on Q2-Master 1.1 by QwazyWabbit.  Originally GloomMaster.\n(c) 2002-2003 r1ch.net. (c) 2007 by QwazyWabbit.\n", VERSION);
	printf("Built: %s at %s for %s.\n\n", __DATE__, __TIME__, OS_STRING);
}

static __inline int SendAcknowledge (const char *gamename)
{
	return (gamename && (!stricmp(gamename, "quake2") || !stricmp(gamename, "quakeworld") || !stricmp(gamename, "hexenworld"))) ? 1 : 0;
}

/* FS: Set a socket to be non-blocking */
#ifdef _WIN32
#define TCP_BLOCKING_ERROR WSAEWOULDBLOCK
static int Set_Non_Blocking_Socket (SOCKET socket)
{
	u_long _true = true;

	return ioctlsocket(socket, FIONBIO, &_true);
}

static __inline int Get_Last_Error (void)
{
	return WSAGetLastError();
}
#else
#define TCP_BLOCKING_ERROR EWOULDBLOCK
static int Set_Non_Blocking_Socket (SOCKET socket)
{
	int _true = true;
	return ioctlsocket(socket, FIONBIO, IOCTLARG_T & _true);
}

static __inline int Get_Last_Error (void)
{
	return errno;
}
#endif

static __inline void msleep (unsigned long msec)
{
#ifndef _WIN32
	usleep(msec * 1000);
#else
	Sleep(msec);
#endif
}

static const char *NET_ErrorString (void)
{
#ifdef _WIN32
	int		code;

	code = WSAGetLastError();
	switch (code)
	{
		case WSAEINTR: return "WSAEINTR";
		case WSAEBADF: return "WSAEBADF";
		case WSAEACCES: return "WSAEACCES";
		case WSAEDISCON: return "WSAEDISCON";
		case WSAEFAULT: return "WSAEFAULT";
		case WSAEINVAL: return "WSAEINVAL";
		case WSAEMFILE: return "WSAEMFILE";
		case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK";
		case WSAEINPROGRESS: return "WSAEINPROGRESS";
		case WSAEALREADY: return "WSAEALREADY";
		case WSAENOTSOCK: return "WSAENOTSOCK";
		case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
		case WSAEMSGSIZE: return "WSAEMSGSIZE";
		case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
		case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
		case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
		case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
		case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
		case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
		case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
		case WSAEADDRINUSE: return "WSAEADDRINUSE";
		case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL";
		case WSAENETDOWN: return "WSAENETDOWN";
		case WSAENETUNREACH: return "WSAENETUNREACH";
		case WSAENETRESET: return "WSAENETRESET";
		case WSAECONNABORTED: return "WSWSAECONNABORTEDAEINTR";
		case WSAECONNRESET: return "WSAECONNRESET";
		case WSAENOBUFS: return "WSAENOBUFS";
		case WSAEISCONN: return "WSAEISCONN";
		case WSAENOTCONN: return "WSAENOTCONN";
		case WSAESHUTDOWN: return "WSAESHUTDOWN";
		case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
		case WSAETIMEDOUT: return "WSAETIMEDOUT";
		case WSAECONNREFUSED: return "WSAECONNREFUSED";
		case WSAELOOP: return "WSAELOOP";
		case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
		case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
		case WSASYSNOTREADY: return "WSASYSNOTREADY";
		case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
		case WSANOTINITIALISED: return "WSANOTINITIALISED";
		case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
		case WSATRY_AGAIN: return "WSATRY_AGAIN";
		case WSANO_RECOVERY: return "WSANO_RECOVERY";
		case WSANO_DATA: return "WSANO_DATA";
		default: return "NO ERROR";
	}
#else
	return strerror (errno);
#endif
}

static void Log_Sucessful_TCP_Connections (char *logbuffer)
{
	FILE *f = fopen(logtcp_filename, "a+");
	if (!f)
	{
		return;
	}

	fseek(f, 0, SEEK_END);

	if (timestamp)
	{
		char timestampLogBuffer[MAXPRINTMSG];

		Com_sprintf(timestampLogBuffer, sizeof(timestampLogBuffer), "%s", Con_Timestamp(logbuffer));
		fputs(timestampLogBuffer, f);
	}
	else
	{
		fputs(logbuffer, f);
	}

	fflush(f);
	fclose(f);
}

SOCKET UDP_OpenSocket (unsigned short port)
{
	SOCKET newsocket;
	struct sockaddr_in address;

	if ((newsocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
	{
		printf("[E] UDP_OpenSocket: socket: %s", NET_ErrorString());
		return INVALID_SOCKET;
	}

	// make it non-blocking
	if (Set_Non_Blocking_Socket(newsocket) == SOCKET_ERROR)
	{
		printf("[E] UDP_OpenSocket: ioctl FIONBIO: %s\n", NET_ErrorString());
		goto ErrorReturn;
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
	if (bind(newsocket, (struct sockaddr *)&address, sizeof(address)) == -1)
		goto ErrorReturn;

	return newsocket;

ErrorReturn:
	closesocket(newsocket);
	return INVALID_SOCKET;
}

static void NET_Init (void)
{
#ifdef _WIN32
	// overhead to tell Windows we're using TCP/IP.
	int err = WSAStartup((WORD)MAKEWORD(2, 2), &ws);
	if (err)
	{
		printf("Error loading Windows Sockets! Error: %d\n", err);
		exit(err);
	}
	else
	{
		printf("[I] Winsock Initialized\n");
	}

	if (LOBYTE(ws.wVersion) != 2 || HIBYTE(ws.wHighVersion) != 2)
	{
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
		exit(1);
	}
#elif __DJGPP__
	int i;
	int err;

	/*	dbug_init();*/

	i = _watt_do_exit;
	_watt_do_exit = 0;
	err = sock_init();
	_watt_do_exit = i;

	if (err != 0)
	{
		printf("[E] WATTCP initialization failed (%s)", sock_init_err(err));
	}
	else
	{
		printf("[I] WATTCP Initialized\n");
	}
#else
	return;
#endif
}

//
// This becomes main for Linux
// In Windows, main is in service.c and it decides if we're going to see a console or not
// this function gets called when we have decided if we are a server or a console app.
//
//int gsmaster_main (int argc, char *argv[])
int gsmaster_main (int argc, char **argv)
{
	int len;
	int optval = 1;
	socklen_t fromlen;
	SOCKET i, j;
	struct sockaddr_in from;

	PrintBanner();
	numservers = 0;

	NET_Init();

#ifndef WIN32	// Already done in ServiceStart() if Windows
	ParseCommandLine(argc, argv);
#endif

	printf("Debugging mode: %d\n", debug);
	printf("Send Acknowledgments from GameSpy Heartbeats: %d\n", bSendGameSpyAck);
	printf("Validate New Server Immediately: %d\n", bValidate_newserver_immediately);
	printf("Require Validation: %s\n", GetValidationRequiredString());
	printf("Heartbeat interval: %lu Minutes\n", heartbeatInterval / 60);
	printf("Minimum Heartbeats Required: %u\n", minimumHeartbeats);
	printf("Timestamps: %d\n", timestamp);
	printf("HTTP Server List Download: %d\n", bHttpEnable);
	if (bHttpEnable)
	{
		printf("\tFilename: %s\n", httpdl_filename);
	}
	printf("MOTD: %d\n", bMotd);
	printf("Log TCP connections: %d\n", bLogTCP);
	printf("Write servers to external file list: %d\n", bGenerateServerList);
	if (bGenerateServerList)
	{
		printf("\tLocation: %s\n", serverListGenerationPath);
		printf("\tTimer: %g seconds\n", serverListGenerationTime);
	}

	listener = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	listenerTCP = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	out = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	// only in Windows, null def in Linux
	GetGSMasterRegKey(REGKEY_BIND_IP, bind_ip);
	GetGSMasterRegKey(REGKEY_BIND_PORT, bind_port);
	GetGSMasterRegKey(REGKEY_BIND_PORT_TCP, bind_port_tcp);

	/* FS: Ensure we don't set the ports to something stupid or have corrupt registry values */
	Check_Port_Boundaries();

	listenaddress.sin_addr.s_addr = inet_addr(bind_ip);
	listenaddress.sin_family = AF_INET;
	listenaddress.sin_port = htons((unsigned short)atoi(bind_port));

	if (setsockopt(listener, SOL_SOCKET, PORTREUSE, (char *)&optval, sizeof(optval)) == -1)
	{
		printf("[W] Couldn't set port %s UDP SO_REUSEADDR\n", bind_port);
	}

	if ((bind(listener, (struct sockaddr *)&listenaddress, sizeof(listenaddress))) == SOCKET_ERROR)
	{
		printf("[E] Couldn't bind to port %s UDP (something is probably using it)\n", bind_port);
		return 1;
	}

	listenaddressTCP.sin_addr.s_addr = inet_addr(bind_ip);
	listenaddressTCP.sin_family = AF_INET;
	listenaddressTCP.sin_port = htons((unsigned short)atoi(bind_port_tcp));

	if (setsockopt(listenerTCP, SOL_SOCKET, PORTREUSE, (char *)&optval, sizeof(optval)) == -1)
	{
		printf("[W] Couldn't set port %s TCP SO_REUSEADDR\n", bind_port_tcp);
	}

	if ((bind(listenerTCP, (struct sockaddr *)&listenaddressTCP, sizeof(listenaddressTCP))) == SOCKET_ERROR)
	{
		printf("[E] Couldn't bind to port %s TCP (something is probably using it)\n", bind_port_tcp);
		return 1;
	}

	if (listen(listenerTCP, MAXPENDING) < 0)
	{
		printf("[E] Couldn't set port %s TCP to listen mode\n", bind_port_tcp);
		return 1;
	}

	delay.tv_sec = 0;
	delay.tv_usec = 0;

	FD_ZERO(&set);
	FD_SET(listener, &set);

	fromlen = sizeof(from);
	printf("listening on %s:%s (UDP)\n", bind_ip, bind_port);
	printf("listening on %s:%s (TCP)\n", bind_ip, bind_port_tcp);
	runmode = SRV_RUN; // set loop control

#ifndef WIN32
#ifndef __DJGPP__
// in Linux or BSD we fork a daemon
// ...but not if debug mode
	if (!debug && (daemon(0, 0) < 0))
	{
		printf("Forking error, running as console, error number was: %d\n", errno);
		debug = 1;
	}
#endif // __DJGPP__

	if (!debug)
	{
#ifndef __DJGPP__
		signal(SIGCHLD, SIG_IGN); /* ignore child */
		signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
		signal(SIGTTOU, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
#endif
		signal(SIGHUP, signal_handler); /* catch hangup signal */
		signal(SIGTERM, signal_handler); /* catch terminate signal */
	}
#endif // WIN32

	FD_ZERO(&master);
	FD_SET(listener, &master);
	maxConnections = listener + listenerTCP;

	ReadMasterDBBlob();
	CURL_HTTP_Init();
	HTTP_DL_List();

	if (load_MasterServerlist)
	{
		Master_DL_List(masterserverlist_filename);
	}

	if (load_Serverlist)
	{
		Add_Servers_From_List(serverlist_filename, NULL);
	}

	while (runmode == SRV_RUN) // 1 = running, 0 = stop, -1 = stopped.
	{
		delay.tv_sec = 1;
		delay.tv_usec = 0;

		CURL_HTTP_Update();

		if ((double)time(NULL) - lastMasterListDL > 3600.0) /* FS: Every hour get a new serverlist from QTracker */
		{
			HTTP_DL_List();

			if (load_MasterServerlist)
			{
				Master_DL_List(masterserverlist_filename);
			}
		}

		if (bGenerateServerList && ((double)time(NULL) - lastServerListGeneration > serverListGenerationTime))
		{
			GenerateServersList();
		}

		FD_ZERO(&master);
		FD_SET(listener, &master);
		set = master;
		for (i = 0; i <= maxConnections; i++)
		{
			if (FD_ISSET(i, &set))
			{ // we got one!!
				if (i == listener)
				{
					if (selectsocket(maxConnections + 1, &set, NULL, NULL, &delay) == 1)
					{
						len = recvfrom (i, incoming, sizeof(incoming), 0, (struct sockaddr *)&from, &fromlen);
						if (len != SOCKET_ERROR)
						{
							newConnection++;
							FD_SET(newConnection, &master); // add to master set
							if (newConnection > maxConnections)
							{	// keep track of the max
								maxConnections = newConnection;
							}
							Parse_UDP_Packet(i, len, &from);
						}
						else
						{
							Con_DPrintf("[E] UDP socket error during select from %s:%d (%s)\n",
								inet_ntoa(from.sin_addr),
								ntohs(from.sin_port),
								NET_ErrorString());
						}
					}
					FD_CLR(newConnection, &master);
				} //listener
				//reset for next packet
				memset(incoming, 0, sizeof(incoming));
			} // FD_ISSET
		} // for loop

		FD_SET(listenerTCP, &master);
		set = master;
		for (j = 0; j <= maxConnections; j++)
		{
			if (FD_ISSET(j, &set))
			{
				if (j == listenerTCP)
				{
					/* FS: Now do the GameSpy TCP handshake */
					if (selectsocket(maxConnections + 1, &set, NULL, NULL, &delay))
					{
						if (FD_ISSET(listenerTCP, &set))
						{
							tcpSocket = accept(listenerTCP, (struct sockaddr *)&from, &fromlen);
							if (tcpSocket != INVALID_SOCKET)
							{
								newConnection++;
								FD_SET(newConnection, &master); // add to master set
								if (newConnection > maxConnections)
								{    // keep track of the max
									maxConnections = newConnection;
								}
								GameSpy_Parse_TCP_Packet(tcpSocket, &from);
							}
							FD_CLR(newConnection, &master);
						}
					}
				}
			}
		}

		// destroy old servers, etc
		RunFrame();
		msleep(1); /* FS: Don't suck up 100% CPU */
	}

	GenerateMasterDBBlob();

	WSACleanup();	// Windows Sockets cleanup
	runmode = SRV_STOPPED;
	return 0;
}

static __inline void Close_TCP_Socket_On_Error (SOCKET socket, struct sockaddr_in *from)
{
	Con_DPrintf("[E] TCP socket error during accept from %s:%d (%s)\n",
				inet_ntoa(from->sin_addr),
				ntohs(from->sin_port),
				NET_ErrorString());
	closesocket(socket);
	socket = INVALID_SOCKET; //-V1001
}

//
// Called by ServiceCtrlHandler after the server loop is dead
// this frees the server memory allocations.
//
static void ExitNicely (void)
{
	server_t	*server;
	server_t	*next = NULL;

	printf("[I] shutting down.\n");

	/* FS: FIXME: Have to skip over the first one since this one is allocated as blank at startup. */
	for (server = servers.next; server; server = next)
	{
		next = server->next;

		free(server);
		server = NULL;
	}

	servers.next = NULL;
	CURL_HTTP_Shutdown();
}

static void DropServer (server_t *server)
{
	if (!server)
	{
		return;
	}

	//unlink
	if (server->next)
	{
		server->next->prev = server->prev;
	}

	if (server->prev)
	{
		server->prev->next = server->next;
	}

	if (numservers != 0)
	{
		numservers--;
	}

	free(server);
}

static void AddServer (struct sockaddr_in *from, int acknowledge, unsigned short queryPort, char *gamename, char *hostnameIp)
{
	server_t	*server = &servers;
	int			preserved_heartbeats = 0;
	struct sockaddr_in addr;
	char validateString[MAX_GSPY_VAL] = {0};
	size_t validateStringLen = 0;
#ifdef HOSTNAME_AND_LOCALHOST_HACK
	bool bHostnameAndLocalhostHack = FALSE;
#endif

	if (!gamename || (gamename[0] == 0))
	{
		Con_DPrintf("[E] No gamename sent from %s:%u.  Aborting AddServer.\n", inet_ntoa(from->sin_addr), htons(from->sin_port));
		return;
	}

	if (queryPort <= 0)
	{
		Con_DPrintf("[E] No Query Port sent from %s:%u.  Aborting AddServer.\n", inet_ntoa(from->sin_addr), htons(from->sin_port));
		return;
	}

	if (!(GameSpy_Get_Game_SecKey(gamename)))
	{
		Con_DPrintf("[E] Game %s not supported from %s:%u.  Aborting AddServer.\n", gamename, inet_ntoa(from->sin_addr), htons(from->sin_port));
		return;
	}

	while (server->next)
	{
		server = server->next;

		if ((*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr) && (htons(queryPort) == htons(server->port)))
		{
			//already exists - could be a pending shutdown (ie killserver, change of map, etc)
			if (server->shutdown_issued)
			{
				Con_DPrintf("[I] scheduled shutdown server %s sent another ping!\n", inet_ntoa(from->sin_addr));
				DropServer(server);
				server = &servers;

				while (server->next)
				{
					server = server->next;
				}

				break;
			}
			else
			{
				Con_DPrintf("[W] dupe ping from %s:%u!! ignored.\n", inet_ntoa(server->ip.sin_addr), htons(server->port));
				return;
			}
		}
	}

	server->next = (server_t *)calloc(1, sizeof(server_t));
	if (server->next == NULL)
	{
		printf("Fatal Error: memory allocation failed in AddServer\n");
		return;
	}

	server->next->prev = server;
	server = server->next;
	server->heartbeats = preserved_heartbeats;
	memcpy(&server->ip, from, sizeof(server->ip));
	server->last_heartbeat = (unsigned long)time(NULL);
	server->next = NULL;

	if (!hostnameIp || hostnameIp[0] == 0) /* FS: If we add servers from a list using dynamic IPs, etc.  let's remember it.  Othewrise, just copy the ip */
	{
		hostnameIp = inet_ntoa(from->sin_addr);
	}

	DK_strlwr(gamename); /* FS: Some games (mainly SiN) send it partially uppercase */
	DG_strlcpy(server->gamename, gamename, sizeof(server->gamename));

	srand((unsigned)time(NULL));

	server->port = queryPort;
	server->shutdown_issued = 0;
	server->queued_pings = 0;
	server->last_ping = (unsigned long)time(NULL)-(rand()%heartbeatInterval); /* FS: Don't ping a bunch of stuff at the same time */
	server->validated = 0;

	GameSpy_Create_Challenge_Key(server->challengeKey, 6);

	DG_strlcpy(server->hostnameIp, hostnameIp, sizeof(server->hostnameIp));

	numservers++;

	Con_DPrintf("[I] %s server %s:%u added to queue! (%d) number: %u\n",
		server->gamename,
		server->hostnameIp,
		htons(server->port),
		acknowledge,
		numservers);

	if (bValidate_newserver_immediately)
	{
		Con_DPrintf("[I] immediately validating new server %s:%u to master server.\n", server->hostnameIp, htons(server->port));
		server->validated = 1;
		server->heartbeats = minimumHeartbeats;
	}

	memcpy(&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
	addr.sin_family = AF_INET;
	addr.sin_port = server->port;
	memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

	if (acknowledge || bSendGameSpyAck) /* FS: This isn't standard for GameSpy, it will show messages about the ack.  This is more a courtesy to tell the ded server that we received the heartbeat */
	{
		if (!stricmp(gamename, "quakeworld"))
		{
			sendto(listener, qw_ack_msg, sizeof(qw_ack_msg), 0, (struct sockaddr *)&addr, sizeof(addr));
		}
		else if (!stricmp(gamename, "hexenworld"))
		{
			sendto(listener, hw_ack_msg, sizeof(hw_ack_msg), 0, (struct sockaddr *)&addr, sizeof(addr));
		}
		else
		{
			sendto(listener, ackstring, ackstringlen, 0, (struct sockaddr *)&addr, sizeof(addr));
		}
	}

	if (!stricmp(server->gamename, "quakeworld") || !stricmp (server->gamename, "quake2"))
	{
		memcpy(validateString, quakeworldquake2statusstring, sizeof(quakeworldquake2statusstring));
		validateStringLen = sizeof(quakeworldquake2statusstring);
	}
	else if (!stricmp(server->gamename, "hexenworld"))
	{
		memcpy(validateString, hexenworldstatusstring, sizeof(hexenworldstatusstring));
		validateStringLen = sizeof(hexenworldstatusstring);
	}
	else if (!stricmp(server->gamename, "hexen2"))
	{
		memcpy(validateString, hexen2querystring, sizeof(hexen2querystring));
		validateStringLen = sizeof(hexen2querystring);
	}
	else if (!stricmp(server->gamename, "quake1"))
	{
		memcpy(validateString, quake1querystring, sizeof(quake1querystring));
		validateStringLen = sizeof(quake1querystring);
	}
	else
	{
		Com_sprintf(validateString, sizeof(validateString), "%s%s", statusstring, server->challengeKey);
		validateStringLen = DG_strlen(validateString);
	}

	validateString[validateStringLen] = '\0'; /* FS: GameSpy null terminates the end */

#ifdef HOSTNAME_AND_LOCALHOST_HACK
	if (!stricmp(server->hostnameIp, HostnameHack)
		|| !stricmp(server->hostnameIp, "127.0.0.1"))
	{
		Con_DPrintf("[I] %s and Localhost port clashing hack from AddServer().\n", HostnameHack);
		bHostnameAndLocalhostHack = true;
	}

	if (!bHostnameAndLocalhostHack)
#endif
	{
		sendto(listener, validateString, validateStringLen, 0, (struct sockaddr *)&addr, sizeof(addr)); /* FS: GameSpy sends this after a heartbeat. */
	}
}

//
// We received a shutdown frame from a server, set the shutdown flag
// for it and send it a ping to ack the shutdown frame.
//
static void QueueShutdown (struct sockaddr_in *from, server_t *myserver)
{
	server_t	*server = &servers;

	if (!myserver)
	{
		while (server->next)
		{
			server = server->next;

			if ((*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr) && (from->sin_port == server->port))
			{
				myserver = server;
				break;
			}
		}
	}

	if (myserver)
	{
		struct sockaddr_in addr;
		char validateString[MAX_GSPY_VAL] = {0};
		size_t validateStringLen = 0;

		memcpy(&addr.sin_addr, &myserver->ip.sin_addr, sizeof(addr.sin_addr));
		addr.sin_family = AF_INET;
		addr.sin_port = server->port;
		memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

#ifdef HOSTNAME_AND_LOCALHOST_HACK
		if (!stricmp(server->hostnameIp, HostnameHack) || !stricmp(server->hostnameIp, "127.0.0.1"))
		{
			Con_DPrintf("[I] %s and Localhost port clashing hack from QueueShutdown().\n", HostnameHack);
			myserver->shutdown_issued = 0;
			return;
		}
#endif
		//hack, server will be dropped in next minute IF it doesn't respond to our ping
		myserver->shutdown_issued = 1;

		GameSpy_Create_Challenge_Key(myserver->challengeKey, 6);

		Con_DPrintf("[I] shutdown queued %s:%u \n", inet_ntoa(myserver->ip.sin_addr), htons(server->port));

		if (!stricmp(server->gamename, "quakeworld") || !stricmp (server->gamename, "quake2"))
		{
			memcpy(validateString, quakeworldquake2statusstring, sizeof(quakeworldquake2statusstring));
			validateStringLen = sizeof(quakeworldquake2statusstring);
		}
		else if (!stricmp(server->gamename, "hexenworld"))
		{
			memcpy(validateString, hexenworldstatusstring, sizeof(hexenworldstatusstring));
			validateStringLen = sizeof(hexenworldstatusstring);
		}
		else if (!stricmp(server->gamename, "hexen2"))
		{
			memcpy(validateString, hexen2querystring, sizeof(hexen2querystring));
			validateStringLen = sizeof(hexen2querystring);
		}
		else if (!stricmp(server->gamename, "quake1"))
		{
			memcpy(validateString, quake1querystring, sizeof(quake1querystring));
			validateStringLen = sizeof(quake1querystring);
		}
		else
		{
			Com_sprintf(validateString, sizeof(validateString), "%s%s", statusstring, server->challengeKey);
			validateStringLen = DG_strlen(validateString);
		}

		validateString[validateStringLen] = '\0'; /* FS: GameSpy null terminates the end */

		sendto(listener, validateString, validateStringLen, 0, (struct sockaddr *)&addr, sizeof(addr));
		return;
	}
	else
	{
		Con_DPrintf("[W] shutdown issued from unregistered server %s!\n", inet_ntoa(from->sin_addr));
	}
}

//
// Walk the server list and ping them as needed, age the ones
// we have not heard from in a while and when they get too
// old, remove them from the list.
//
static void RunFrame (void)
{
	server_t		*server = &servers;
	unsigned long	curtime = (unsigned long)time(NULL);

	while (server->next)
	{
		server = server->next;

		if (curtime - server->last_heartbeat > 60)
		{
			server_t *old = server;

			server = old->prev;

			if (old->shutdown_issued || old->queued_pings > 6)
			{
				Con_DPrintf("[I] %s:%u shut down.\n", inet_ntoa(old->ip.sin_addr), htons(old->port));
				DropServer(old);
				continue;
			}

			server = old;

			if (curtime - server->last_ping >= heartbeatInterval)
			{
				struct sockaddr_in addr;
				char validateString[MAX_GSPY_VAL] = {0};
				size_t validateStringLen = 0;

				srand((unsigned)time(NULL));

				addr.sin_addr = Hostname_to_IP(&server->ip.sin_addr, server->hostnameIp); /* FS: Resolve hostname if it's from a serverlist file */
				addr.sin_family = AF_INET;
				addr.sin_port = server->port;
				memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));
				server->queued_pings++;
				server->last_ping = curtime-(rand()%heartbeatInterval); /* FS: Don't ping a bunch of stuff at the same time */

				GameSpy_Create_Challenge_Key(server->challengeKey, 6); /* FS: Challenge key for this server */

				Con_DPrintf("[I] ping %s(%s):%u\n", server->hostnameIp, inet_ntoa(addr.sin_addr), htons(server->port));

				if (!stricmp(server->gamename, "quakeworld") || !stricmp (server->gamename, "quake2"))
				{
					memcpy(validateString, quakeworldquake2statusstring, sizeof(quakeworldquake2statusstring));
					validateStringLen = sizeof(quakeworldquake2statusstring);
				}
				else if (!stricmp(server->gamename, "hexenworld"))
				{
					memcpy(validateString, hexenworldstatusstring, sizeof(hexenworldstatusstring));
					validateStringLen = sizeof(hexenworldstatusstring);
				}
				else if (!stricmp(server->gamename, "hexen2"))
				{
					memcpy(validateString, hexen2querystring, sizeof(hexen2querystring));
					validateStringLen = sizeof(hexen2querystring);
				}
				else if (!stricmp(server->gamename, "quake1"))
				{
					memcpy(validateString, quake1querystring, sizeof(quake1querystring));
					validateStringLen = sizeof(quake1querystring);
				}
				else
				{
					Com_sprintf(validateString, sizeof(validateString), "%s%s", statusstring, server->challengeKey);
					validateStringLen = DG_strlen(validateString);
				}

				validateString[validateStringLen] = '\0'; /* FS: GameSpy null terminates the end */

#ifdef HOSTNAME_AND_LOCALHOST_HACK
				if (!stricmp(server->hostnameIp, HostnameHack) || !stricmp(server->hostnameIp, "127.0.0.1"))
				{
					Con_DPrintf("[I] %s and Localhost port clashing hack from RunFrame().\n", HostnameHack);
					server->shutdown_issued = 0;
					server->queued_pings = 0;
					server->last_heartbeat = curtime-(rand()%heartbeatInterval); /* FS: Don't ping a bunch of stuff at the same time */
					server->heartbeats++;
					server->validated = 1;
					msleep(1);
					continue;
				}
#endif
				sendto(listener, validateString, validateStringLen, 0, (struct sockaddr *)&addr, sizeof(addr)); /* FS: GameSpy sends an Out-of-Band status */
				msleep(1);
			}
		}
	}
}

//
// This function assembles the reply header preamble and 6 bytes for each
// listed server into a buffer for transmission to the client in response
// to a query frame.
//
static void SendUDPServerListToClient (struct sockaddr_in *from, const char *gamename)
{
	int				buflen = 0;
	int				udpheadersize = 0;
	char			*buff = NULL;
	char			*udpheader = NULL;
	server_t		*server = &servers;
	unsigned int	servercount = 0;
	size_t			bufsize = 0;

	/* FS: *** NOTE THIS DOES NOT WORK WELL WITH LISTS OVER 1400 MTU!  THIS PROTOCOL HAS NO HINT
	       *** THAT THERE ARE ADDITIONAL PACKETS WAITING!
	 */

	// assume buffer size needed is for all current servers (numservers)
	// and eligible servers in list will always be less or equal to numservers
	if (!gamename || gamename[0] == 0)
	{
		Con_DPrintf("[E] No gamename specified for UDP List Request!  Aborting.\n");
		return;
	}

	if (!stricmp(gamename, "hexenworld"))
	{
		udpheadersize = sizeof(hw_reply_hdr);
		udpheader = (char *)calloc(1, udpheadersize);
		if (!udpheader)
		{
			Con_DPrintf("Fatal Error: memory allocation failed in SendUDPServerListToClient\n");
			return;
		}
		memcpy(udpheader, hw_reply_hdr, udpheadersize);
	}
	else if (!stricmp(gamename, "quakeworld"))
	{
		udpheadersize = sizeof(qw_reply_hdr);
		udpheader = (char *)calloc(1, udpheadersize);
		if (!udpheader)
		{
			Con_DPrintf("Fatal Error: memory allocation failed in SendUDPServerListToClient\n");
			return;
		}
		memcpy(udpheader, qw_reply_hdr, udpheadersize);
	}
	else if (!stricmp(gamename, "quake2") || !stricmp(gamename, "daikatana"))
	{
		udpheadersize = sizeof(q2_reply_hdr);
		udpheader = (char*)calloc(1, udpheadersize);
		if (!udpheader)
		{
			Con_DPrintf("Fatal Error: memory allocation failed in SendUDPServerListToClient\n");
			return;
		}
		memcpy(udpheader, q2_reply_hdr, udpheadersize);
	}
	else
	{
		Con_DPrintf("[E] Unsupported gamename in UDP List Request: %s!\n", gamename);
		return;
	}

	bufsize = (udpheadersize) + 6 * (numservers + 1); // n bytes for the reply header, 6 bytes for game server ip and port
	buflen = 0;
	buff = (char *)calloc(1, bufsize);
	if (!buff)
	{
		free(udpheader);

		Con_DPrintf("Fatal Error: memory allocation failed in SendServerListToClient\n");
		return;
	}
	memcpy(buff, udpheader, udpheadersize);	// n = length of the reply header
	buflen += (udpheadersize);
	servercount = 0;

	while (server->next)
	{
		server = server->next;

		if (server->heartbeats >= minimumHeartbeats && !server->shutdown_issued && server->validated && !strcmp(server->gamename, gamename) && server->ip.sin_port && server->port != 0)
		{
			memcpy(buff + buflen, &server->ip.sin_addr, 4);
			buflen += 4;

			memcpy(buff + buflen, &server->port, 2);
			buflen += 2;
			servercount++;
		}
	}

	if ((sendto(listener, buff, buflen, 0, (struct sockaddr *)from, sizeof(*from))) == SOCKET_ERROR)
	{
		Con_DPrintf("[E] list socket error on send! code %s.\n", NET_ErrorString());
	}
	else
	{
		Con_DPrintf("[I] query response (%d bytes) sent to %s:%d\n", buflen, inet_ntoa(from->sin_addr), ntohs(from->sin_port));
		Con_DPrintf("[I] sent %s server list to client %s, servers: %u of %u\n",
			gamename,
			inet_ntoa(from->sin_addr),
			servercount, /* sent */
			numservers); /* on record */
	}

	free(udpheader);
	free(buff);
}

/* GameSpy BASIC data is in the form of '\ip\1.2.3.4:1234\ip\1.2.3.4:1234\final\'
 * GameSpy non-basic data is in the form of '<sin_addr><sin_port>\final\'
 */
static void SendGameSpyListToClient (SOCKET socket, char *gamename, char *challengeKey, int encType, struct sockaddr_in *from, bool uncompressed)
{
	unsigned char	*buff;
	int				buflen = 0;
	char			port[10] = {0};
	char			*ip = NULL;
	server_t		*server = &servers;
	unsigned int	servercount;
	int				maxsize = GSPY_BUFFER_GROWBY_SIZE;

	// assume buffer size needed is for all current servers (numservers)
	// and eligible servers in list will always be less or equal to numservers
	if (!gamename || gamename[0] == 0)
	{
		Con_DPrintf("[E] No gamename specified for GameSpy List Request!  Aborting.\n");
		return;
	}

	buff = calloc(1, maxsize);
	if (!buff)
	{
		Con_DPrintf("[E] Couldn't allocate temporary buffer!\n");
		return;
	}

	DK_strlwr(gamename); /* FS: Some games (mainly SiN) send it partially uppercase */

	if (uncompressed)
	{
		memcpy(buff, listheader, 1);
		buflen += 1;
	}

	servercount = 0;

	while (server->next)
	{
		server = server->next;

		if (server->heartbeats >= minimumHeartbeats && !server->shutdown_issued && server->validated && !strcmp(server->gamename, gamename))
		{
			ip = inet_ntoa(server->ip.sin_addr);

			if (uncompressed)
			{
				if ((encType != GAMESPY_ENCTYPE1) && (buflen + 3 + 16 + 1 + 6 >= MAX_GSPY_MTU_SIZE))
				{
					Con_DPrintf("[I] Sending chunked packet to %s:%d\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port));
					if (send(socket, buff, buflen, 0) == SOCKET_ERROR)
					{
						Con_DPrintf("[E] TCP list socket error on send! code %s.\n", NET_ErrorString());
						free(buff);
						return;
					}
					memset(buff, 0, maxsize);
					buflen = 0;
				}

				if (buflen + 3 + 16 + 1 + 6 >= maxsize)
				{
					unsigned char *temp;

					maxsize += GSPY_BUFFER_GROWBY_SIZE;
					temp = realloc(buff, maxsize);
					if (!temp)
					{
						Con_DPrintf("[E] Couldn't grow temporary buffer!\n");
						free(buff);
						return;
					}
					buff = temp;
				}

				memcpy(buff + buflen, "ip\\", 3); // 3
				buflen += 3;
				memcpy(buff + buflen, ip, DG_strlen(ip)); // 16
				buflen += DG_strlen(ip);
				memcpy(buff + buflen, ":", 1); // 1
				buflen += 1;

				sprintf(port, "%d\\", ntohs(server->port));
				memcpy(buff + buflen, port, DG_strlen(port)); // 6
				buflen += DG_strlen(port);
				servercount++;
			}
			else
			{
				if ((encType != GAMESPY_ENCTYPE1) && (buflen + 6 >= MAX_GSPY_MTU_SIZE))
				{
					Con_DPrintf("[I] Sending chunked packet to %s:%d\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port));
					if (send(socket, buff, buflen, 0) == SOCKET_ERROR)
					{
						free(buff);
						Con_DPrintf("[E] TCP list socket error on send! code %s.\n", NET_ErrorString());
						return;
					}
					memset(buff, 0, maxsize);
					buflen = 0;
				}

				if (buflen + 6 >= maxsize)
				{
					unsigned char *temp;

					maxsize += GSPY_BUFFER_GROWBY_SIZE;
					temp = realloc(buff, maxsize);
					if (!temp)
					{
						Con_DPrintf("[E] Couldn't grow temporary buffer!\n");
						free(buff);
						return;
					}
					buff = temp;
				}

				memcpy(buff + buflen, &server->ip.sin_addr, 4);
				buflen += 4;
				memcpy(buff + buflen, &server->port, 2);
				buflen += 2;
				servercount++;
			}
		}
	}

	if (uncompressed)
	{
		if (encType != GAMESPY_ENCTYPE1)
		{
			if (buflen + 6 >= MAX_GSPY_MTU_SIZE)
			{
				Con_DPrintf("[I] Sending chunked packet before final to %s:%d\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port));
				if (send(socket, buff, buflen, 0) == SOCKET_ERROR)
				{
					free(buff);
					Con_DPrintf("[E] TCP list socket error on send! code %s.\n", NET_ErrorString());
					return;
				}
				memset(buff, 0, maxsize);
				buflen = 0;
			}

			memcpy(buff + buflen, finalstring, 6);
			buflen += 6;
		}
	}
	else
	{
		if (encType != GAMESPY_ENCTYPE1) /* FS: This COULD be added to enctype1 and GS3D will process it just fine, but this is non-standard and it breaks Aluigi gslist. */
		{
			if (buflen + 7 >= MAX_GSPY_MTU_SIZE)
			{
				Con_DPrintf("[I] Sending chunked packet before final to %s:%d\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port));
				if (send(socket, buff, buflen, 0) == SOCKET_ERROR)
				{
					free(buff);
					Con_DPrintf("[E] TCP list socket error on send! code %s.\n", NET_ErrorString());
					return;
				}
				memset(buff, 0, maxsize);
				buflen = 0;
			}

			if (buflen + 7 >= maxsize)
			{
				unsigned char *temp;

				maxsize += GSPY_BUFFER_GROWBY_SIZE;
				temp = realloc(buff, maxsize);
				if (!temp)
				{
					Con_DPrintf("[E] Couldn't grow temporary buffer!\n");
					free(buff);
					return;
				}
				buff = temp;
			}

			memcpy(buff + buflen, "\\", 1);
			buflen += 1;
			memcpy(buff + buflen, finalstring, 6);
			buflen += 6;
		}
	}

	if (encType == GAMESPY_ENCTYPE1)
	{
		unsigned char *encryptedBuffer;
		unsigned char *head;

		/* FS: If we don't send something then GS3D shows the icon as "red".
		 *     \\final\\ is not part of standard protocol from old Aluigi dumps, but GS3D responds to it so... */
		if (buflen == 0)
		{
			memcpy(buff + buflen, "\\", 1);
			buflen += 1;
			memcpy(buff + buflen, finalstring, 6);
			buflen += 6;
		}

		encryptedBuffer = calloc(1, (buflen * 2) + 100);
		if (!encryptedBuffer)
		{
			Con_DPrintf("[E] Failed to allocate memory for temporary encrypt buffer!\n");
			free(buff);
			return;
		}

		head = encryptedBuffer;
		buflen = create_enctype1_buffer(challengeKey, buff, buflen, encryptedBuffer);
		while (buflen >= MAX_GSPY_MTU_SIZE)
		{
			if (send(socket, encryptedBuffer, MAX_GSPY_MTU_SIZE, 0) == SOCKET_ERROR)
			{
				Con_DPrintf("[E] TCP list socket error on send! code %s.\n", NET_ErrorString());
				free(buff);
				free(head);
				return;
			}
			buflen -= MAX_GSPY_MTU_SIZE;
			encryptedBuffer += MAX_GSPY_MTU_SIZE;
		}

		if (send(socket, encryptedBuffer, buflen, 0) == SOCKET_ERROR)
		{
			Con_DPrintf("[E] TCP list socket error on send! code %s.\n", NET_ErrorString());
			free(buff);
			free(head);
			return;
		}

		free(head);
	}
	else
	{
		if (send(socket, buff, buflen, 0) == SOCKET_ERROR)
		{
			Con_DPrintf("[E] TCP list socket error on send! code %s.\n", NET_ErrorString());
			free(buff);
			return;
		}
	}

	free(buff);

	Con_DPrintf("[I] TCP GameSpy list response (%d bytes) sent to %s:%d\n", buflen, inet_ntoa(from->sin_addr), ntohs(from->sin_port));
	Con_DPrintf("[I] sent TCP GameSpy %s list to client %s, servers: %u of %u\n",
				gamename,
				inet_ntoa(from->sin_addr),
				servercount, /* sent */
				numservers); /* on record */
}

static void Ack (struct sockaddr_in *from, char* dataPacket)
{
	server_t	*server = &servers;

	//iterate through known servers
	while (server->next)
	{
		server = server->next;

		//a match!
		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port)
		{
			Con_DPrintf("[I] ack from %s:%u (%d)(%d).\n",
				inet_ntoa(server->ip.sin_addr),
				htons(server->port),
				server->queued_pings,
				server->validated);

			server->last_heartbeat = (unsigned long)time(NULL);

			/* FS: These games are too old to send a challenge back. */
			if (!stricmp(server->gamename, "quake2")
				|| !stricmp(server->gamename, "quakeworld")
				|| !stricmp(server->gamename, "quake1")
				|| !stricmp(server->gamename, "hexenworld")
				|| !stricmp(server->gamename, "hexen2"))
			{
				server->validated = 1;
			}
			else
			{
				server->validated = GameSpy_Challenge_Cross_Check(server->challengeKey, dataPacket, server->challengeKey, 1, GAMESPY_ENCTYPE0);
			}

			server->queued_pings = 0;

			if (server->shutdown_issued)
			{
				Con_DPrintf("[I] aborting scheduled shutdown from %s:%u.\n", inet_ntoa(server->ip.sin_addr), htons(server->port));
			}

			server->shutdown_issued = 0; /* FS: If we're still responding then we didn't shutdown, just changed the map */
			server->heartbeats++;
			return;
		}
	}
}

static void HeartBeat (struct sockaddr_in *from, char *data)
{
	server_t	*server = &servers;
	unsigned short queryPort = 0;
	char seperators[] = "\\";
	char *cmdToken = NULL;
	char *cmdPtr = NULL;
	int statechanged = FALSE;
	bool bSendAckLegacy = FALSE; /* FS: For QW, HW, and Q2. */
#ifdef HOSTNAME_AND_LOCALHOST_HACK
	struct in_addr addr;
	bool bHostnameAndLocalhostHack = FALSE;
#endif

	if (!data || data[0] == '\0')
	{
		return;
	}

	if (strstr(data, "\\statechanged\\")) /* FS: Schedule a shutdown if statechanged is sent with heartbeat */
	{
		if (strstr(data, "\\statechanged\\" GAMESPY_STATE_UPDATE_STRING)) /* FS: Map change?  Don't abort */
		{
			statechanged = FALSE;
		}
		else /* FS: If we don't get a key after it or it's >= 2 assume shutdown.  We'll still ping it anyways to verify it's removal. */
		{
			statechanged = TRUE;
		}
	}

	data += 10; /* FS: heartbeat\\ */
	cmdToken = DK_strtok_r(data, seperators, &cmdPtr); /* FS: \\actual port\\ */
	if (!cmdToken || !cmdPtr)
	{
		Con_DPrintf("[E] Invalid heartbeat packet (No query port) from %s:%u!\n", inet_ntoa(from->sin_addr), htons(from->sin_port));
		return;
	}

	queryPort = (unsigned short)atoi(cmdToken); /* FS: Query port */
	cmdToken = DK_strtok_r(NULL, seperators, &cmdPtr); /* FS: \\gamename\\ */
	if (!cmdToken || !cmdPtr || strcmp(cmdToken, "gamename") != 0)
	{
		Con_DPrintf("[E] Invalid heartbeat packet (No gamename) from %s:%u!\n", inet_ntoa(from->sin_addr), htons(from->sin_port));
		return;
	}

	cmdToken = DK_strtok_r(NULL, seperators, &cmdPtr); /* FS: \\actual gamename\\ */

#ifdef HOSTNAME_AND_LOCALHOST_HACK
	if (!strcmp(inet_ntoa(from->sin_addr), "127.0.0.1"))
	{
		struct hostent *remoteHost;
		remoteHost = gethostbyname(HostnameHack);

		addr.s_addr = *(u_long *) remoteHost->h_addr_list[0];
		from->sin_addr.s_addr = addr.s_addr;
		bHostnameAndLocalhostHack = true;
		Con_DPrintf("[I] %s and Localhost port clashing hack from Heartbeat().\n", HostnameHack);
	}
#endif

	//walk through known servers
	while (server->next)
	{
		server = server->next;
		//a match!

		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && queryPort == htons(server->port))
		{
			struct sockaddr_in addr;
			char validateString[MAX_GSPY_VAL] = {0};
			int validateStringLen = 0;

			memcpy(&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
			addr.sin_family = AF_INET;
			server->port = htons(queryPort);
			addr.sin_port = server->port;
			memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

			GameSpy_Create_Challenge_Key(server->challengeKey, 6);

			server->last_heartbeat = (unsigned long)time(NULL);
			Con_DPrintf("[I] heartbeat from %s:%u.\n",	inet_ntoa(server->ip.sin_addr), htons(server->port));

			if (!stricmp(server->gamename, "quakeworld") || !stricmp (server->gamename, "quake2"))
			{
				memcpy(validateString, quakeworldquake2statusstring, sizeof(quakeworldquake2statusstring));
				validateStringLen = sizeof(quakeworldquake2statusstring);
				bSendAckLegacy = true;
			}
			else if (!stricmp(server->gamename, "hexenworld"))
			{
				memcpy(validateString, hexenworldstatusstring, sizeof(hexenworldstatusstring));
				validateStringLen = sizeof(hexenworldstatusstring);
				bSendAckLegacy = true;
			}
			else if (!stricmp(server->gamename, "hexen2"))
			{
				memcpy(validateString, hexen2querystring, sizeof(hexen2querystring));
				validateStringLen = sizeof(hexen2querystring);
			}
			else if (!stricmp(server->gamename, "quake1"))
			{
				memcpy(validateString, quake1querystring, sizeof(quake1querystring));
				validateStringLen = sizeof(quake1querystring);
			}
			else
			{
				Com_sprintf(validateString, sizeof(validateString), "%s%s", statusstring, server->challengeKey);
				validateStringLen = DG_strlen(validateString);
			}

			validateString[validateStringLen] = '\0'; /* FS: GameSpy null terminates the end */

#ifdef HOSTNAME_AND_LOCALHOST_HACK
			if (!bHostnameAndLocalhostHack)
#endif
			{
				sendto(listener, validateString, validateStringLen, 0, (struct sockaddr *)&addr, sizeof(addr)); /* FS: GameSpy uses the \status\ data for collection in a database so people can see the current stats without having to really ping the server. */

				if (bSendGameSpyAck || bSendAckLegacy) /* FS: This isn't standard for GameSpy.  This is more a courtesy to tell the ded server that we received the heartbeat */
				{
					if (!stricmp(server->gamename, "quakeworld"))
					{
						sendto(listener, qw_ack_msg, sizeof(qw_ack_msg), 0, (struct sockaddr *)&addr, sizeof(addr));
					}
					else if (!stricmp(server->gamename, "hexenworld"))
					{
						sendto(listener, hw_ack_msg, sizeof(hw_ack_msg), 0, (struct sockaddr *)&addr, sizeof(addr));
					}
					else
					{
						sendto(listener, ackstring, ackstringlen, 0, (struct sockaddr *)&addr, sizeof(addr));
					}
				}
			}

			if (statechanged)
			{
				QueueShutdown(&addr, NULL);
			}

			return;
		}
	}

	// we didn't find server in our list
	AddServer (from, SendAcknowledge(cmdToken), htons(queryPort), cmdToken, NULL);
}

static void ParseResponse (struct sockaddr_in *from, char *data, int dglen)
{
	char *cmd = data;
	char *line = data;
	char packetData[64] = { 0 };
	unsigned char *mslist = (unsigned char *)data;

	while (*line && *line != '\n')
	{
		line++;
	}

	*(line++) = '\0';

	/* FS: TODO: This actually sends out total number of active players, could be useful for SmartSpy query. */
	if (!memcmp(incoming, hw_hearbeat_msg, sizeof(hw_hearbeat_msg)))
	{
		Con_DPrintf("[I] HexenWorld Server sending a heartbeat.\n");
		Com_sprintf(packetData, sizeof(packetData), "heartbeat\\%d\\gamename\\hexenworld", ntohs(from->sin_port));
		HeartBeat(from, packetData);
		return;
	}
	else if (!memcmp(incoming, qw_hearbeat_msg, sizeof(qw_hearbeat_msg)))
	{
		Con_DPrintf("[I] QuakeWorld Server sending a heartbeat.\n");
		Com_sprintf(packetData, sizeof(packetData), "heartbeat\\%d\\gamename\\quakeworld", ntohs(from->sin_port));
		HeartBeat(from, packetData);
		return;
	}

	if (strstr(data, OOB_SEQ)) /* FS: GameSpy doesn't send the 0xFF out-of-band. */
	{
		if (!strnicmp(data, (char *)q2_reply_hdr, sizeof(q2_reply_hdr)-1))
		{
			Con_DPrintf("[I] Got a Quake 2 master server list!\n");

			mslist += sizeof(q2_reply_hdr);
			Parse_UDP_MS_List(mslist, quake2, dglen-sizeof(q2_reply_hdr));
			return;
		}
		else if (!strnicmp(data, (char *)hw_reply_hdr, sizeof(hw_reply_hdr)-1))
		{
			Con_DPrintf("[I] Got a HexenWorld master server list!\n");
			mslist += sizeof(hw_reply_hdr);
			Parse_UDP_MS_List(mslist, hexenworld, dglen-sizeof(hw_reply_hdr));
			return;
		}
		else if (!strnicmp(data, (char *)qw_reply_hdr, sizeof(qw_reply_hdr)-1) || !strnicmp(data, (char *)qw_reply_hdr2, sizeof(qw_reply_hdr2)-1)) /* FS: Some servers send '\n' others send '\0' so check both. */
		{
			Con_DPrintf("[I] Got a QuakeWorld master server list!\n");

			mslist += sizeof(qw_reply_hdr);
			Parse_UDP_MS_List(mslist, quakeworld, dglen-sizeof(qw_reply_hdr)); /* FS: Length is the same, so doesn't really matter what we pass here. */
			return;
		}
		else if (!strnicmp(data, daikatanagetserversstring, daikatanagetserverslen))
		{
			Con_DPrintf("[I] %s:%d : query (%d bytes)\n",
			inet_ntoa(from->sin_addr),
			htons(from->sin_port),
			dglen);

			SendUDPServerListToClient(from, "daikatana");
			return;
		}
		else if (!strnicmp(data, OOB_SEQ "query", 9) || !strnicmp(data, OOB_SEQ "getservers", 14))
		{
			Con_DPrintf("[I] %s:%d : query (%d bytes)\n",
			inet_ntoa(from->sin_addr),
			htons(from->sin_port),
			dglen);

			SendUDPServerListToClient(from, "quake2");
			return;
		}
		else if (!strnicmp(data, OOB_SEQ "rcon", 8))
		{
			cmd +=9;
			Rcon(from, cmd);
			return;
		}
		else if (!strnicmp(data, OOB_SEQ "heartbeat", 13))
		{
			char q2heartbeat[96];

			Com_sprintf(q2heartbeat, sizeof(q2heartbeat), "heartbeat\\%d\\gamename\\quake2", ntohs(from->sin_port));
			HeartBeat(from, q2heartbeat);
			return;
		}
		else
		{
			cmd +=4;
		}
	}
	else
	{
		if (!strnicmp(data, "query", 5)) /* FS: One of the many non-standard responses to a quake 2 query. */
		{
			Con_DPrintf("[I] %s:%d : query (%d bytes)\n",
			inet_ntoa(from->sin_addr),
			htons(from->sin_port),
			dglen);

			SendUDPServerListToClient(from, "quake2");
			return;
		}
		cmd +=1;
	}

	Con_DPrintf("[I] %s: %s:%d (%d bytes)\n",
		cmd,
		inet_ntoa(from->sin_addr), htons(from->sin_port),
		dglen);

	/* FS: If we got here then it's some GameSpy related stuff. */
	if (!strnicmp(cmd, "ping", 4))
	{
		/* FS: Impossible to determine the game from this.  Likely Q2, but could be KP, DK, etc.
		 *     Heartbeat gets sent soon we can figure it out then.
		 */
	}
	else if (!strnicmp(cmd, "heartbeat", 9)) /* FS: GameSpy only responds to "heartbeat", print is Q2 */
	{
		HeartBeat(from, cmd);
	}
	else if (!strnicmp(cmd, "ack", 3))
	{
		Ack(from, data);
	}
	else if (!strnicmp(cmd, "shutdown", 8))
	{
		QueueShutdown(from, NULL);
	}
	else
	{
		// Con_DPrintf("[W] Unknown command from %s!\n", inet_ntoa(from->sin_addr));
		/* FS: Assume anything else passed in here is some ack from a heartbeat or \\status\\secure\\<key> */
		Ack(from, data);
	}
}

void ParseCommandLine (int argc, char **argv)
{
	int i = 0;

	if (argc >= 2)
	{
		debug = 3; //initializing
	}

	for (i = 1; i < argc; i++)
	{
		if (debug == 3)
		{
			if (!strnicmp(argv[i] + 1,"debug", 5))
			{
				debug = TRUE;	//service debugged as console
			}
			else
			{
				debug = FALSE;
			}
		}

		if (!strnicmp(argv[i] + 1, "?", 1) || !strnicmp(argv[i] + 1, "help", 4))
		{
			printf("\nOptions:\n");

			printf("* -debug - Asserts debug mode. The program prints status messages to console\n" \
					"           while running. Shows pings, heartbeats, number of servers listed.\n\n");

			printf("* -ip <ip address> - causes server to bind to a particular IP address when\n" \
					"                     used with multi-homed hosts. Default is 0.0.0.0 (any).\n\n");

			printf("* -port <port> - UDP port used for status query. Default is %s.\n\n", bind_port);

			printf("* -sendack: by default GameSpy doesn't not send this type of packet out.\n" \
					"            if you want to extend the courtesy of acknowleding the\n" \
					"            heartbeat then enable this setting.\n\n");

			printf("* -quickvalidate - by default the master server requires 1 extra heartbeat\n" \
					"                   and a successful ping request to be added to the query\n" \
					"                   list.  Set this to allow any new server to show up\n" \
					"                   immediately.\n\n");

			printf("* -validationrequired <1, 2, or 3> - Require validation from the challenge key\n" \
					"                                     cross-checked with the games secure key.\n" \
					"                                     1 - client list requests only.\n" \
					"                                     2 - servers.\n" \
					"                                     3 - clients and servers (recommended).\n\n");

			printf("* -timestamp <1 or 2> - Debug outputs are timestampped.  1 - for AM/PM.\n" \
					"                        2 for military.\n\n");

			printf("* -tcpport <port> - causes server to bind to a particular TCP port for the\n" \
					"                    GameSpy list query from clients. Default is %s.\n\n", bind_port_tcp);

			printf("* -serverlist <filename> - Adds servers from a list.  Hostnames are supported.\n" \
					"                           Format is <ip>,<query port>,<gamename>\n" \
					"                           i.e. maraakate.org,27982,daikatana.\n\n");

			printf("* -httpenable <filename> - Uses HTTP to grab server lists from a filename.\n" \
					"                Format is <url> <filename_to_save_to> <gamename>.\n\n");

			printf("* -heartbeatinterval <minutes> - Time (in minutes) when a heartbeat is sent out\n" \
					"                                 to a server on the list\n\n");

			printf("* -masterlist <filename> - Adds master servers from a list.  Every hour\n" \
					"                           GSMaster will ping these servers to grab their lists.\n" \
					"                           Hostnames are supported.\n" \
					"                           Format is <ip>,<query port>,<gamename>\n" \
					"                           i.e. maraakate.org,27900,quakeworld.\n\n");

			printf("* -logtcp <filename> - Log successful GameSpy TCP list requests.\n" \
					"                       If no filename is specified it will use the default\n" \
					"                       of %s\n\n", logtcp_filename);

			printf("* -motd <filename> - Send a MOTD Out-of-Band packet.\n"
					"                     See gamestable.cpp for supported games\n\n");

			printf("* -cwd <path> - Sets the current working directory.  Useful for\n" \
				   "                running as a windows service or scheduled task.\n\n");

			printf("* -generateserverlist <folderpath> - Generate a server list as a text file to\n" \
				   "                                       the specified folder path.\n\n");

			printf("* -serverlisttimer <seconds> - Time (in seconds) to generate a\n" \
				   "                                server list.  Use with\n" \
				   "                                -generateserverlist\n\n");
			exit(0);
		}
		else if (!strnicmp(argv[i] + 1, "sendack", 7))
		{
			bSendGameSpyAck = true;
		}
		else if (!strnicmp(argv[i] + 1, "quickvalidate", 13))
		{
			bValidate_newserver_immediately = true;
		}
		else if (!strnicmp(argv[i] + 1, "validationrequired", 18))
		{
			validation_required = atoi(argv[i+1]);
		}
		else if (!strnicmp(argv[i] + 1, "timestamp", 9))
		{
			timestamp = atoi(argv[i+1]);
		}
		else if (!strnicmp(argv[i] + 1, "httpenable", 10))
		{
			if (argv[i + 1] && (argv[i+1][0] != '-'))
			{
				DG_strlcpy(httpdl_filename, argv[i + 1], sizeof(httpdl_filename));
				bHttpEnable = true;
			}
		}
#ifdef _MSC_VER /* FS: Not on mingw. */
		else if(!strnicmp(argv[i] + 1, "minidumpautogen", 15))
		{
			bMinidumpAutogen = true;
		}
#endif
		else if (!strnicmp(argv[i] + 1, "rconpassword", 12))
		{
			DG_strlcpy(rconPassword, argv[i+1], sizeof(rconPassword));
			printf("[I] rcon password set to %s\n", rconPassword);
		}
		else if (!strnicmp(argv[i] + 1, "heartbeatinterval", 17))
		{
			heartbeatInterval = atol(argv[i+1]);
			if (heartbeatInterval < 1)
			{
				printf("[W] Heartbeat interval less than one minute!  Setting to one minute.\n");
				heartbeatInterval = 60;
			}
			else
			{
				heartbeatInterval = heartbeatInterval * 60;
			}
		}
		else if (!strnicmp(argv[i] + 1, "minimumheartbeats", 17))
		{
			minimumHeartbeats = atoi(argv[i+1]);
			if (minimumHeartbeats < 1)
			{
				printf("[W] Minimum heartbeat less than one!  Setting to one heartbeat required.\n");
				minimumHeartbeats = 1;
			}
		}
		else if (!strnicmp(argv[i] + 1, "ip", 2))
		{
			//bind_ip, a specific host ip if desired
			DG_strlcpy(bind_ip, argv[i+1], sizeof(bind_ip));
			SetGSMasterRegKey(REGKEY_BIND_IP, bind_ip);
		}
		else if (!strnicmp(argv[i] + 1, "port", 4))
		{
			//bind_port, if other than default port
			DG_strlcpy(bind_port, argv[i+1], sizeof(bind_port));
			SetGSMasterRegKey(REGKEY_BIND_PORT, bind_port);
		}
		else if (!strnicmp(argv[i] + 1, "tcpport", 7))
		{
			//bind_port_tcp, if other than default TCP port
			DG_strlcpy(bind_port_tcp, argv[i+1], sizeof(bind_port_tcp));
			SetGSMasterRegKey(REGKEY_BIND_PORT_TCP, bind_port_tcp);
		}
		else if (!strnicmp(argv[i] + 1, "serverlisttimer", 15))
		{
			if (!argv[i+1][0] || (argv[i+1][0] == '-'))
			{
				serverListGenerationTime = DEFAULTSERVERLISTGENERATIONTIME;
				printf("No time set for server list generation  Using default of %g.\n", serverListGenerationTime);
			}
			else
			{
				serverListGenerationTime = atof(argv[i+1]);
				printf("Set server list generation time to %g\n", serverListGenerationTime);
			}
		}
		else if (!strnicmp(argv[i] + 1, "serverlist", 10))
		{
			DG_strlcpy(serverlist_filename, argv[i+1], sizeof(serverlist_filename));
			printf("Using serverlist: %s\n", serverlist_filename);
			load_Serverlist = 1;
		}
		else if (!strnicmp(argv[i] + 1, "masterlist", 10))
		{
			DG_strlcpy(masterserverlist_filename, argv[i+1], sizeof(masterserverlist_filename));
			printf("Using masterlist: %s\n", masterserverlist_filename);
			load_MasterServerlist = 1;
		}
		else if (!strnicmp(argv[i] + 1, "motd", 4)) /* FS: Added motd.txt support */
		{
			bMotd = true;
		}
		else if (!strnicmp(argv[i] + 1, "logtcp", 6)) /* FS: Write out successful GameSpy TCP requests */
		{
			DG_strlcpy(logtcp_filename, argv[i+1], sizeof(logtcp_filename));
			if (!DG_strlen(logtcp_filename) || logtcp_filename[0] == '-')
			{
				DG_strlcpy(logtcp_filename, LOGTCP_DEFAULTNAME, sizeof(logtcp_filename));
				printf("No filename specified for logtcp.  Using default: %s\n", logtcp_filename);
			}
			else
			{
				printf("Logging to %s\n", logtcp_filename);
			}
			bLogTCP = true;
		}
		else if (!strnicmp(argv[i] + 1, "generateserverlist", 18))
		{
			DG_strlcpy(serverListGenerationPath, argv[i+1], sizeof(serverListGenerationPath));
			if (!DG_strlen(serverListGenerationPath) || (serverListGenerationPath[0] == '-'))
			{
				serverListGenerationPath[0] = '\0';
				bGenerateServerList = false;
				printf("No folder path specified for generating server lists.\n");
			}
			else
			{
				printf("Writing server lists to %s\n", serverListGenerationPath);
				bGenerateServerList = true;
			}
		}
		else if (!strnicmp(argv[i] + 1, "cwd", 3))
		{
			if (chdir(argv[i+1]))
			{
				switch (errno)
				{
					case ENOENT:
						printf("Unable to locate the directory: %s\n", argv[i+1]);
						break;
					case EINVAL:
						printf("Invalid buffer.\n");
						break;
					default:
						printf("Unknown error.\n");
				}
			}
			else
			{
				char buff[250];
				getcwd(buff, sizeof(buff)-1);
				printf("CWD set to %s\n", buff);
#ifdef _WIN32
				AddToMessageLog(TEXT(buff));
#endif
			}
		}
	}
}

//
// This stuff plus a modified service.c and service.h
// from the Microsoft examples allows this application to be
// installed as a Windows service.
//
#ifdef _WIN32
void ServiceCtrlHandler (DWORD Opcode)
{
	switch (Opcode)
	{
		case SERVICE_CONTROL_STOP:
			// Kill the server loop.
			runmode = SRV_STOP; // zero the loop control

			while (runmode == SRV_STOP)	//give loop time to die
			{
				static int i = 0;

				msleep(500);	// SCM times out in 3 secs.
				i++;		// we check twice per sec.

				if (i >= 6)	// hopefully we beat the SCM timer
				{
					break;	// still no return? rats, terminate anyway
				}
			}

			ExitNicely();

			MyServiceStatus.dwWin32ExitCode = 0;
			MyServiceStatus.dwCurrentState = SERVICE_STOPPED;
			MyServiceStatus.dwCheckPoint = 0;
			MyServiceStatus.dwWaitHint = 0;

			if (MyServiceStatusHandle)
			{
				SetServiceStatus(MyServiceStatusHandle, &MyServiceStatus);
			}

			return;
	}
	// Send current status.
	SetServiceStatus (MyServiceStatusHandle, &MyServiceStatus);
}

void ServiceStart (DWORD argc, LPTSTR *argv)
{
	ParseCommandLine(argc, argv); // we call it here and in gsmaster_main

	MyServiceStatus.dwServiceType        = SERVICE_WIN32;
	MyServiceStatus.dwCurrentState       = SERVICE_START_PENDING;
	MyServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP;
	MyServiceStatus.dwWin32ExitCode      = 0;
	MyServiceStatus.dwServiceSpecificExitCode = 0;
	MyServiceStatus.dwCheckPoint         = 0;
	MyServiceStatus.dwWaitHint           = 0;

	MyServiceStatusHandle = (SERVICE_STATUS_HANDLE)0;

	if (!debug)
	{
		MyServiceStatusHandle = RegisterServiceCtrlHandler(argv[0],
			(LPHANDLER_FUNCTION)ServiceCtrlHandler);

		if (MyServiceStatusHandle == (SERVICE_STATUS_HANDLE)0)
		{
			printf("%s not started.\n", SZSERVICEDISPLAYNAME);
			return;
		}
	}

	// Initialization complete - report running status.
	MyServiceStatus.dwCurrentState       = SERVICE_RUNNING;
	MyServiceStatus.dwCheckPoint         = 0;  //-V1048
	MyServiceStatus.dwWaitHint           = 0;  //-V1048

	if (!debug)
	{
		SetServiceStatus(MyServiceStatusHandle, &MyServiceStatus);
	}

	gsmaster_main(argc, &argv[0]);
}

void ServiceStop (void)
{
	ServiceCtrlHandler(SERVICE_CONTROL_STOP);
}

/*
* This sets the registry keys in "HKLM/Software/Q2MasterServer" so we can tell
* the service what IP address or port to bind to when starting up. If it's not preset
* the service will bind to 0.0.0.0:27900. Not critical on most Windows boxes
* but it can be a pain if you want to use multiple IP's on a NIC and don't want the
* service listening on all of them. Not as painful on Linux, we do the -ip switch
* in the command line.
*/
void SetGSMasterRegKey (const char* name, const char *value)
{
	HKEY	hKey;
	DWORD	Disposition;
	LRESULT	status;

	status = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
		REGKEY_GSMASTERSERVER,
		0, //always 0
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS,
		NULL,
		&hKey,
		&Disposition);
	if (status)
	{
		Con_DPrintf("Error creating registry key for %s\n", SZSERVICEDISPLAYNAME);
	}

	status = RegSetValueEx(hKey, name, 0, REG_SZ, (unsigned char*)value, (DWORD)DG_strlen(value));
	if (status)
	{
		Con_DPrintf("Registry key not set for IP: %s\n", bind_ip);
	}

	RegCloseKey(hKey);
}

//
// As as Service, get the key and use the IP address stored there.
// If the key doesn't exist, it will be created.
// The user can add the Bind_IP or Bind_Port value
// by hand or use the -ip x.x.x.x command line switch.
//
void GetGSMasterRegKey (const char* name, const char *value)
{
	HKEY	hKey;
	DWORD	Disposition;
	LRESULT	status;
	DWORD	size = KEY_LEN;	// expected max size of the bind_ip or bind_port array

	// check value, create it if it doesn't exist
	status = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
		REGKEY_GSMASTERSERVER,
		0, //always 0
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_READ,
		NULL,
		&hKey,
		&Disposition);
	if (status)
	{
		Con_DPrintf("Registry key not found\n");
	}

	status = RegQueryValueEx(hKey, name, NULL, NULL, (unsigned char*)value, &size);
	if (status)
	{
		Con_DPrintf("Registry value not found %s\\%s\n", REGKEY_GSMASTERSERVER, name);
	}

	RegCloseKey(hKey);
}

#else	// not doing windows

//
// handle Linux and BSD signals
//
void signal_handler (int sig)
{
	switch (sig)
	{
		case SIGHUP:
			break;
		case SIGTERM:
			runmode = SRV_STOP;
			while (runmode == SRV_STOP)	//give loop time to die
			{
				int i = 0;

				msleep(500);	// 500 ms
				i++;		// we check twice per sec.

				if (i >= 6)
				{
					break;	// still no return? rats, terminate anyway
				}
			}

			ExitNicely();
			break;
	}
}

#endif

static void GameSpy_Send_MOTD (char *gamename, struct sockaddr_in *from)
{
	SOCKET motdSocket;
	char motd[MOTD_SIZE];
	struct sockaddr_in addr;
	unsigned short motdGamePort = GameSpy_Get_MOTD_Port(gamename);
	FILE *f;
	long fileSize;
	size_t toEOF = 0;
	size_t fileBufferLen = 0;
	char *fileBuffer = NULL;

	if (!motdGamePort)
	{
		return;
	}

	if ((motdSocket = UDP_OpenSocket(21005)) == INVALID_SOCKET)
	{
		return;
	}

	f = fopen("motd.txt", "r+");

	if (!f)
	{
		Con_DPrintf("[E] Couldn't open motd.txt!\n");
		return;
	}

	fseek(f, 0, SEEK_END);
	fileSize = ftell(f);

	/* FS: If the file size is less than 3 (an emtpy serverlist file) then don't waste time. */
	if (fileSize < 3)
	{
		printf("[E] File 'motd.txt' is emtpy!\n");
		fclose(f);
		return;
	}

	rewind(f);
	fileBuffer = (char *)calloc(1, sizeof(char)*(fileSize+2)); /* FS: In case we have to add a newline terminator */
	if (!fileBuffer)
	{
		printf("[E] Out of memory!\n");
		return;
	}

	toEOF = fread(fileBuffer, sizeof(char), fileSize, f); /* FS: Copy it to a buffer */
	fclose(f);

	if (toEOF <= 0)
	{
		free(fileBuffer);

		printf("[E] Cannot read file 'motd.txt' into memory!\n");
		return;
	}

	/* FS: Add newline terminator for some paranoia */
	fileBuffer[toEOF] = '\n';
	fileBuffer[toEOF+1] = '\0';

	fileBufferLen = DG_strlen(fileBuffer);

	if (fileBufferLen >= MOTD_SIZE)
	{
		printf("[W] 'motd.txt' greater than %d bytes!  Truncating...\n", MOTD_SIZE);
	}

	memcpy(&addr.sin_addr, &from->sin_addr, sizeof(addr.sin_addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(motdGamePort);
	memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

	if (gamename && gamename[0] != 0 && !stricmp(gamename, "quake2")) /* FS: Green Text special for Quake 2*/
		Com_sprintf(motd, sizeof(motd), OOB_SEQ"print\n\x02%s", fileBuffer);
	else
		Com_sprintf(motd, sizeof(motd), OOB_SEQ"print\n%s", fileBuffer);

	sendto(motdSocket, motd, DG_strlen(motd), 0, (struct sockaddr *)&addr, sizeof(addr));

	free(fileBuffer);

	closesocket(motdSocket);
	motdSocket = INVALID_SOCKET; //-V1001
}

static void GameSpy_Parse_List_Request (char *clientName, char *querystring, char *challengeKey, int encType, SOCKET socket, struct sockaddr_in *from)
{
	char *gamename = NULL;
	char *ret = NULL;
	char logBuffer[2048];
	char *queryPtr;
	bool uncompressed = false;

	if (!querystring || !strstr(querystring, "\\list\\") || !strstr(querystring, "\\gamename\\"))
	{
		goto error;
	}

	ret = strstr(querystring, "\\list\\cmp\\gamename\\");
	if (ret)
	{
		if (strlen(ret) < 19)
		{
			goto error;
		}

		ret += 19;
		gamename = DK_strtok_r(ret, "\\\n", &queryPtr);
		uncompressed = false;
		Con_DPrintf("[I] Sending compressed TCP list for %s\n", gamename);
	}
	else /* FS: Older style that sends out "basic" style lists */
	{
		ret = strstr(querystring, "\\list\\");
		if (!ret || strlen(ret) < 6)
		{
			goto error;
		}
		ret += 6;

		ret = strstr(ret, "\\gamename\\");
		if (!ret || strlen(ret) < 10)
		{
			goto error;
		}

		ret += 10;
		gamename = DK_strtok_r(ret, "\\\n", &queryPtr);
		uncompressed = true;
		Con_DPrintf("[I] Sending uncompressed TCP list for %s\n", gamename);
	}

	if (gamename[0] == 0)
	{
error:
		Con_DPrintf("[I] Invalid TCP list request from %s:%d.  Sending %s string.\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port), finalstringerror);
		send(tcpSocket, finalstringerror, DG_strlen(finalstringerror), 0);
		return;
	}

	DK_strlwr(gamename); /* FS: Some games (mainly SiN) send it partially uppercase */

	if (bMotd)
	{
		GameSpy_Send_MOTD(gamename, from); /* FS: Send a MOTD */
	}

	if (bLogTCP)
	{
		Com_sprintf(logBuffer, sizeof(logBuffer), "Sucessful GameSpy request to %s for %s from %s:%d\n", clientName, gamename, inet_ntoa(from->sin_addr), ntohs(from->sin_port));
		Log_Sucessful_TCP_Connections(logBuffer);
	}

	SendGameSpyListToClient(socket, gamename, challengeKey, encType, from, uncompressed);
}

static bool GameSpy_Challenge_Cross_Check (char *challengePacket, char *validatePacket, char *challengeKey, int rawsecurekey, int enctype)
{
	char *ptr = NULL;
	char validateKey[MAX_INFO_STRING];
	char gameKey[MAX_INFO_STRING];
	char *decodedKey = NULL;
	const char *gameSecKey = NULL;

	if (!validation_required)
	{
		Con_DPrintf("[I] Skipping validation checks.\n");
		return true;
	}
	else if (validation_required == GAMESPY_VALIDATION_REQUIRED_CLIENTS_ONLY && rawsecurekey) /* FS: This is an "ack" sent from a heartbeat, dropserver, or addserver */
	{
		Con_DPrintf("[I] Skipping server validation checks.\n");
		return true;
	}
	else if (validation_required == GAMESPY_VALIDATION_REQUIRED_SERVERS_ONLY && !rawsecurekey) /* FS: This is "list" requests sent from clients */
	{
		Con_DPrintf("[I] Skipping client validation checks.\n");
		return true;
	}

	if (rawsecurekey)
	{
		ptr = challengePacket;
	}
	else
	{
		ptr = Info_ValueForKey(challengePacket, "secure");
	}

	if (!ptr)
	{
		Con_DPrintf("[E] Validation failed.  \\secure\\ missing from packet!\n");
		return false;
	}

	DG_strlcpy(challengeKey, ptr, DG_strlen(ptr)+1);

	ptr = Info_ValueForKey(validatePacket, "gamename");
	if (!ptr)
	{
		Con_DPrintf("[E] Validation failed.  \\gamename\\ missing from packet!\n");
		return false;
	}

	DG_strlcpy(gameKey,ptr,sizeof(gameKey));

	if (!strcmp(gameKey,"nolf") && rawsecurekey)
	{
		Con_DPrintf("[I] NOLF does not respond to \\secure\\ from servers.  Skipping Validation.\n");
		return true;
	}

	if (!strcmp(gameKey,"heretic2"))
	{
		Con_DPrintf("[I] Heretic2 does not respond to \\secure\\ from servers.  Skipping validation.\n");
		return true;
	}

	ptr = Info_ValueForKey(validatePacket, "validate");
	if (!ptr)
	{
		Con_DPrintf("[E] Validation failed.  \\validate\\ missing from packet!\n");
		return false;
	}

	DG_strlcpy(validateKey,ptr,sizeof(validateKey));

	gameSecKey = GameSpy_Get_Game_SecKey(gameKey);
	if (!gameSecKey)
	{
		Con_DPrintf("[E] Validation failed.  Game not supported!\n");
		return false;
	}

	decodedKey = (char *)gsseckey(NULL, (unsigned char*)challengeKey, (unsigned char*)gameSecKey, enctype);
	if (decodedKey && decodedKey[0] != '\0' && !strcmp(decodedKey, validateKey))
	{
		Con_DPrintf("[I] Validation passed!\n");
		return true;
	}

	Con_DPrintf("[E] Validation failed.  Incorrect key sent!\n");
	return false;
}

static void GameSpy_Parse_TCP_Packet (SOCKET socket, struct sockaddr_in *from)
{
	int len = 0;
	int lastWSAError = 0;
	int retry = 0;
	int sleepMs;
	int challengeBufferLen = 0;
	int encodetype = 0;
	char *challengeKey;
	char *challengeBuffer = NULL;
	char *enctypeKey = NULL;
	char *clientName = NULL;

	challengeKey = (char *)calloc(1, sizeof(char) * 7);
	if (!challengeKey)
	{
		Con_DPrintf("[E] Couldn't allocate temporary buffer.\n");
		goto closeTcpSocket;
	}

	challengeBuffer = (char *)calloc(1, sizeof(char) * 84);
	if (!challengeBuffer)
	{
		Con_DPrintf("[E] Couldn't allocate temporary buffer.\n");
		goto closeTcpSocket;
	}

	memset(incomingTcpValidate, 0, sizeof(incomingTcpValidate));
	memset(incomingTcpList, 0, sizeof(incomingTcpList));
	GameSpy_Create_Challenge_Key(challengeKey, 6);

	if (Set_Non_Blocking_Socket(socket) == SOCKET_ERROR)
	{
		Con_DPrintf("[E] TCP socket failed to set non-blocking.\n");
		goto closeTcpSocket;
	}

	Con_DPrintf("[I] TCP connection from %s:%d\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port));
	Com_sprintf(challengeBuffer, 83, "%s%s", challengeHeader, challengeKey);
	challengeBufferLen = DG_strlen(challengeBuffer);
	if (challengeBufferLen)
	{
		challengeBuffer[challengeBufferLen] = '\0';
		len = send(socket, challengeBuffer, challengeBufferLen, 0);
	}
	else
	{
		len = SOCKET_ERROR;
	}

	if (len == SOCKET_ERROR)
	{
		Con_DPrintf("[E] Couldn't send challenge to client!\n");
		goto closeTcpSocket;
	}

	retry = 0;
	sleepMs = 50;
retryIncomingTcpValidate:
	len = recv(socket, incomingTcpValidate, sizeof(incomingTcpValidate), 0);

	if (len == SOCKET_ERROR)
	{
		lastWSAError = Get_Last_Error();

		if (lastWSAError == TCP_BLOCKING_ERROR && (retry < totalRetry)) /* FS: Yeah, yeah; this sucks.  But, it works OK for our purpose.  If you don't like this, redo it and send me the code. */
		{
			retry++;
			Con_DPrintf("[W] Retrying GameSpy TCP Validate Request, Attempt %d of %d.\n", retry, totalRetry);
			msleep(sleepMs);
			sleepMs = sleepMs + 10;
			goto retryIncomingTcpValidate;
		}
		else /* FS: give up */
		{
			Close_TCP_Socket_On_Error(socket, from);
			goto closeTcpSocket;
		}
	}

	if (incomingTcpValidate[0] != 0)
	{
		/* FS: Unofficial nastyness in QDOS, Q2DOS, and DK 1.3 -- So I can see if someone out there is a veteran player who happens to run a game search */
		clientName = Info_ValueForKey(incomingTcpValidate, "clientname");
		if (!clientName)
		{
			clientName = strdup("Unknown User");
		}
		else
		{
			clientName = strdup(Info_ValueForKey(incomingTcpValidate, "clientname"));
		}

		Con_DPrintf("[I] Incoming Validate: %s\n", incomingTcpValidate);
	}
	else
	{
		Con_DPrintf("[E] Incoming Validate Packet is NULL!\n");
		goto closeTcpSocket;
	}

	/* FS: Only enctype 0 and 1 are supported. */
	enctypeKey = Info_ValueForKey(incomingTcpValidate, "enctype");
	if (enctypeKey && enctypeKey[0] != 0)
	{
		encodetype = atoi(enctypeKey);
		if (encodetype > GAMESPY_ENCTYPE1)
		{
			Con_DPrintf("[E] Encode Type: %d not supported on this server\n", encodetype);
			goto closeTcpSocket;
		}
	}

	/* FS: Not supported or junk data, bye. */
	if (!GameSpy_Challenge_Cross_Check(challengeBuffer, incomingTcpValidate, challengeKey, 0, encodetype))
	{
		goto closeTcpSocket;
	}

	if (strstr(incomingTcpValidate, "\\gamename\\unreal\\")) /* FS: Special hack for unreal, it doesn't send list */
	{
		strcat(incomingTcpValidate, "\\list\\gamename\\unreal\\");
	}

	/* FS: This is the later version of GameSpy which sent it all as one packet. */
	if (strstr(incomingTcpValidate,"\\list\\"))
	{
		GameSpy_Parse_List_Request(clientName, incomingTcpValidate, challengeKey, encodetype, socket, from);
		goto closeTcpSocket;
	}

	/* FS: If we get here then it must be the "basic" version which sends things a little differently.
	 *     So grab the packet which contains the list request as well as the gamename
	 */
	retry = 0;
	sleepMs = 50;

retryIncomingTcpList:
	len = recv(socket, incomingTcpList, sizeof(incomingTcpList), 0);
	if (len == SOCKET_ERROR)
	{
		if (lastWSAError == TCP_BLOCKING_ERROR && (retry < totalRetry)) /* FS: Yeah, yeah; this sucks.  But, it works OK for our purpose.  If you don't like this, redo it and send me the code. */
		{
			retry++;
			Con_DPrintf("[W] Retrying GameSpy TCP List Request, Attempt %d of %d.\n", retry, totalRetry);
			msleep(sleepMs);
			sleepMs = sleepMs + 10;
			goto retryIncomingTcpList;
		}
		else /* FS: give up */
		{
			Close_TCP_Socket_On_Error(socket, from);
			Con_DPrintf("[E] TCP socket error during select from %s:%d (%s)\n",
				inet_ntoa(from->sin_addr),
				ntohs(from->sin_port),
				NET_ErrorString());
			goto closeTcpSocket;
		}
	}

	if (incomingTcpList[0] != 0)
	{
		Con_DPrintf("[I] Incoming List Request: %s\n", incomingTcpList);
	}
	else
	{
		Con_DPrintf("[E] Incoming List Request Packet is NULL!\n");
		goto closeTcpSocket;
	}

	if (len > 4)
	{
		//parse this packet
		if (strstr(incomingTcpList, "\\list\\") && strstr(incomingTcpList, "\\gamename\\")) /* FS: We must have \\list\\ and \\gamename\\ for this to work */
		{
			GameSpy_Parse_List_Request(clientName, incomingTcpList, challengeKey, encodetype, socket, from);
		}
		else
		{
			Con_DPrintf("[I] Invalid TCP list request from %s:%d.  Sending %s string.\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port), finalstringerror);
			send(socket, finalstringerror, DG_strlen(finalstringerror), 0);
		}
	}
	else
	{
		Con_DPrintf("[W] runt TCP packet from %s:%d\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port));
	}

closeTcpSocket:
	//reset for next packet
	if (clientName)
	{
		free(clientName);
	}

	if (challengeKey)
	{
		free(challengeKey);
	}

	if (challengeBuffer)
	{
		free(challengeBuffer);
	}

	memset(incomingTcpValidate, 0, sizeof(incomingTcpValidate));
	memset(incomingTcpList, 0, sizeof(incomingTcpList));
	closesocket(socket);
	tcpSocket = INVALID_SOCKET;
}

void Add_Servers_From_List (char *filename, char *gamename)
{
	char *fileBuffer = NULL;
	long fileSize;
	FILE *listFile = fopen(filename, "r+");
	size_t toEOF = 0;

	if (!listFile)
	{
		printf("[E] Cannot open file '%s'.\n", filename);
		return;
	}

	fseek(listFile, 0, SEEK_END);
	fileSize = ftell(listFile);

	/* FS: If the file size is less than 3 (an emtpy serverlist file) then don't waste time. */
	if (fileSize < 3)
	{
		printf("[E] File '%s' is emtpy!\n", filename);
		fclose(listFile);
		return;
	}

	rewind(listFile);
	fileBuffer = (char *)calloc(1, sizeof(char)*(fileSize+2)); /* FS: In case we have to add a newline terminator */
	if (!fileBuffer)
	{
		printf("[E] Out of memory!\n");
		return;
	}

	toEOF = fread(fileBuffer, sizeof(char), fileSize, listFile); /* FS: Copy it to a buffer */
	fclose(listFile);

	if (toEOF <= 0)
	{
		free(fileBuffer);
		printf("[E] Cannot read file '%s' into memory!\n", filename);
		return;
	}

	/* FS: Add newline terminator for some paranoia */
	fileBuffer[toEOF] = '\n';
	fileBuffer[toEOF+1] = '\0';

	Parse_ServerList(toEOF, fileBuffer, gamename); /* FS: Break it up divided by newline terminator */

	free(fileBuffer);
}

void AddServers_From_List_Execute (char *fileBuffer, char *gamenameFromHttp)
{
	char *ip = NULL;
	char *listToken = NULL;
	char *listPtr = NULL;
	static const char separators[] = ",:\n";
	unsigned short queryPort = 0;
	struct hostent *remoteHost;
	struct in_addr addr;
	struct sockaddr_in from;
	size_t ipStrLen = 0;

	listToken = DK_strtok_r(fileBuffer, separators, &listPtr); // IP
	if (!listToken)
	{
		return;
	}

	while (1)
	{
		ipStrLen = DG_strlen(listToken)+2;
		ip = (char *)malloc(sizeof(char)*(ipStrLen));
		if (!ip)
		{
			printf("Memory error in AddServers_From_List_Execute!\n");
			break;
		}

		DG_strlcpy(ip, listToken, ipStrLen);
		remoteHost = gethostbyname(ip);
		if (!remoteHost) /* FS: Junk data or doesn't exist. */
		{
			Con_DPrintf("[E] Could not resolve '%s' in server list; skipping.\n", ip);
			break;
		}

		addr.s_addr = *(u_long *) remoteHost->h_addr_list[0];

		listToken = DK_strtok_r(NULL, separators, &listPtr); // Port
		if (!listToken)
		{
			Con_DPrintf("[E] Port not specified for '%s' in server list; skipping.\n", ip);
			break;
		}

		queryPort = (unsigned short)atoi(listToken);
		if (queryPort == 0)
		{
			Con_DPrintf("[E] Invalid Port specified for '%s' in server list; skipping.\n", ip);
			break;
		}

		if (gamenameFromHttp)
		{
			listToken = gamenameFromHttp;
		}
		else
		{
			listToken = DK_strtok_r(NULL, separators, &listPtr); // Gamename
		}

		if (!listToken)
		{
			Con_DPrintf("[E] Gamename not specified for '%s:%u' in server list; skipping.\n", ip, queryPort);
			break;
		}

		memset(&from, 0, sizeof(from));
		from.sin_addr.s_addr = addr.s_addr;
		from.sin_family = AF_INET;
		from.sin_port = htons(queryPort);
		AddServer(&from, SendAcknowledge(listToken), htons(queryPort), listToken, ip);
		break;
	}

	if (ip)
	{
		free(ip);
		ip = NULL;
	}
}

static struct in_addr Hostname_to_IP (struct in_addr *server, char *hostnameIp)
{
	struct hostent *remoteHost;
	struct in_addr addr = {0};

	remoteHost = gethostbyname(hostnameIp);
	if (!remoteHost) /* FS: Can't resolve.  Just default to the old IP previously retained so it can be removed later. */
	{
		Con_DPrintf("Can't resolve %s.  Defaulting to previous known good %s.\n", hostnameIp, inet_ntoa(*server));
		return *server;
	}

	addr.s_addr = *(u_long *) remoteHost->h_addr_list[0];

	return addr;
}

static void Check_Port_Boundaries (void)
{
	int udp = 0;
	int tcp = 0;

	if (bind_port[0] != 0)
	{
		udp = atoi(bind_port);
	}

	if (bind_port_tcp[0] != 0)
	{
		tcp = atoi(bind_port_tcp);
	}

	if (bind_port[0] == 0 || udp < 1)
	{
		printf("[W] UDP Port is 0!  Setting to default value of 27900\n");
		SetGSMasterRegKey(REGKEY_BIND_PORT, "27900");
		DG_strlcpy(bind_port,"27900", 6);
		udp = 27900;
	}
	else if (udp > 65536)
	{
		printf("[W] UDP Port is greater than 65536!  Setting to default value of 27900\n");
		SetGSMasterRegKey(REGKEY_BIND_PORT, "27900");
		DG_strlcpy(bind_port,"27900", 6);
		udp = 27900;
	}

	if (bind_port_tcp[0] == 0 || tcp < 1)
	{
		printf("[W] TCP Port is 0!  Setting to default value of 28900\n");
		SetGSMasterRegKey(REGKEY_BIND_PORT_TCP, "28900");
		DG_strlcpy(bind_port_tcp,"28900", 6);
		tcp = 28900;
	}
	else if (tcp > 65536)
	{
		printf("[W] TCP Port is greater than 65536!  Setting to default value of 28900\n");
		SetGSMasterRegKey(REGKEY_BIND_PORT_TCP, "28900");
		DG_strlcpy(bind_port_tcp,"28900", 6);
		tcp = 28900;
	}

	if (tcp == udp)
	{
		printf("[W] UDP and TCP Ports are the same values!  Setting to defaults.\n");
		SetGSMasterRegKey(REGKEY_BIND_PORT, "27900");
		SetGSMasterRegKey(REGKEY_BIND_PORT_TCP, "28900");
		DG_strlcpy(bind_port,"27900", 6);
		DG_strlcpy(bind_port_tcp,"28900", 6);
	}
}

static void Rcon (struct sockaddr_in *from, char *queryString)
{
	int validated = FALSE;
	char *password;
	char *queryPtr;
	char rconMsg[80];

	password = DK_strtok_r(queryString, " \\n", &queryPtr);

	if (rconPassword[0] != 0)
	{
		if (password && password[0] != 0 && !strcmp(password, rconPassword))
		{
			validated = TRUE;
		}
	}

	if (validated)
	{
		if (!strnicmp (queryPtr, "addservers\\", 11))
		{
			char *key = queryPtr + 10;
			key = DK_strtok_r(key, " \\\n", &queryPtr);

			if (key && (key[0] != 0) && !strcmp(key, "filename"))
			{
				key = DK_strtok_r(NULL, " \\\n", &queryPtr);
					if (key && key[0] != 0)
					{
						Add_Servers_From_List(key, NULL);
					}
					else
					{
						Com_sprintf(rconMsg, sizeof(rconMsg), OOB_SEQ"print\nNo filename specified!\n");
						sendto(listener, rconMsg, DG_strlen(rconMsg), 0, (struct sockaddr *)from, sizeof(*from));
					}
			}
			else
			{
				Com_sprintf(rconMsg, sizeof(rconMsg), OOB_SEQ"print\nAddserver must have \\filename\\file.txt\\!\n");
				sendto(listener, rconMsg, DG_strlen(rconMsg), 0, (struct sockaddr *)from, sizeof(*from));
			}
		}
		else
		{
			Com_sprintf(rconMsg, sizeof(rconMsg), OOB_SEQ"print\nUnknown rcon command!\n");
			sendto(listener, rconMsg, DG_strlen(rconMsg), 0, (struct sockaddr *)from, sizeof(*from));
		}
	}
	else
	{
		Com_sprintf(rconMsg, sizeof(rconMsg), OOB_SEQ"print\nBad rcon password!\n");
		sendto(listener, rconMsg, DG_strlen(rconMsg), 0, (struct sockaddr *)from, sizeof(*from));
	}
}

static void HTTP_DL_List (void)
{
	FILE *f;
	long fileSize;
	char *fileBuffer, *listToken, *listPtr;
	static const char separators[] = " \t\n";
	size_t toEOF;

#ifdef USE_CURL
	if (bHttpEnable && (httpdl_filename[0] != '\0'))
	{
		f = fopen(httpdl_filename, "r+");
		if (!f)
		{
			Con_DPrintf("[E] failed to open %s!\n", httpdl_filename);
			lastMasterListDL = (double)time(NULL);
			return;
		}

		fseek(f, 0, SEEK_END);
		fileSize = ftell(f);
		if (fileSize < 3) /* FS: If the file size is less than 3 (an emtpy serverlist file) then don't waste time. */
		{
			printf("[E] File '%s' is emtpy!\n", httpdl_filename);
			lastMasterListDL = (double)time(NULL);
			fclose(f);
			return;
		}

		rewind(f);

		fileBuffer = (char *)calloc(1, sizeof(char) * (fileSize + 2)); /* FS: In case we have to add a newline terminator */
		if (!fileBuffer)
		{
			printf("[E] Out of memory!\n");
			lastMasterListDL = (double)time(NULL);
			return;
		}

		toEOF = fread(fileBuffer, sizeof(char), fileSize, f); /* FS: Copy it to a buffer */
		fclose(f);

		if (toEOF <= 0)
		{
			printf("[E] Cannot read file '%s' into memory!\n", httpdl_filename);
			lastMasterListDL = (double)time(NULL);
			free(fileBuffer);
			return;
		}

		/* FS: Add newline terminator for some paranoia */
		fileBuffer[toEOF] = '\n';
		fileBuffer[toEOF + 1] = '\0';

		listToken = DK_strtok_r(fileBuffer, separators, &listPtr); // IP
		if (!listToken)
		{
			free(fileBuffer);
			lastMasterListDL = (double)time(NULL);
			return;
		}

		while (listToken)
		{
			char *url, *gamename, *filename;

			url = listToken;
			filename = DK_strtok_r(NULL, separators, &listPtr);
			gamename = DK_strtok_r(NULL, separators, &listPtr);

			if (filename && gamename)
			{
				if (!CURL_HTTP_StartDownload(url, filename, gamename))
				{
					CURL_HTTP_AddToQueue(url, filename, gamename);
				}
			}
			listToken = DK_strtok_r(NULL, separators, &listPtr);
		}

		free(fileBuffer);

		printf("[I] HTTP master server list download sceduled!\n");

		lastMasterListDL = (double)time(NULL);
	}
#endif
}

static void Master_DL_List (char *filename)
{
	char *fileBuffer = NULL;
	char *ip = NULL;
	char *listToken = NULL;
	char *listPtr = NULL;
	static const char separators[] = ",:\n";
	unsigned short queryPort = 0;
	struct hostent *remoteHost;
	struct in_addr addr;
	struct sockaddr_in from;
	size_t ipStrLen = 0;
	long fileSize;
	FILE *listFile = fopen(filename, "r+");
	size_t toEOF = 0;

	Con_DPrintf("[I] UDP master server list download scheduled!\n");
	lastMasterListDL = (double)time(NULL);

	if (!listFile)
	{
		printf("[E] Cannot open file '%s'.\n", filename);
		return;
	}

	fseek(listFile, 0, SEEK_END);
	fileSize = ftell(listFile);

	/* FS: If the file size is less than 3 (an emtpy serverlist file) then don't waste time. */
	if (fileSize < 3)
	{
		printf("[E] File '%s' is emtpy!\n", filename);
		fclose(listFile);
		return;
	}

	rewind(listFile);
	fileBuffer = (char *)calloc(1, sizeof(char)*(fileSize+2)); /* FS: In case we have to add a newline terminator */
	if (!fileBuffer)
	{
		printf("[E] Out of memory!\n");
		return;
	}

	toEOF = fread(fileBuffer, sizeof(char), fileSize, listFile); /* FS: Copy it to a buffer */
	fclose(listFile);

	if (toEOF <= 0)
	{
		printf("[E] Cannot read file '%s' into memory!\n", filename);

		free(fileBuffer);
		return;
	}

	/* FS: Add newline terminator for some paranoia */
	fileBuffer[toEOF] = '\n';
	fileBuffer[toEOF+1] = '\0';

	listToken = DK_strtok_r(fileBuffer, separators, &listPtr); // IP
	if (!listToken)
	{
		free(fileBuffer);
		return;
	}

	while(listToken)
	{
		ipStrLen = DG_strlen(listToken)+2;
		ip = (char *)malloc(sizeof(char)*(ipStrLen));
		if (!ip)
		{
			printf("Memory error in AddServers_From_List_Execute!\n");
			break;
		}

		DG_strlcpy(ip, listToken, ipStrLen);
		remoteHost = gethostbyname(ip);
		if (!remoteHost) /* FS: Junk data or doesn't exist. */
		{
			Con_DPrintf("[E] Could not resolve '%s' in server list; skipping.\n", ip);
			break;
		}

		free(ip);
		ip = NULL;


		addr.s_addr = *(u_long *) remoteHost->h_addr_list[0];

		listToken = DK_strtok_r(NULL, separators, &listPtr); // Port
		if (!listToken)
		{
			Con_DPrintf("[E] Port not specified for '%s' in server list; skipping.\n", ip);
			break;
		}

		queryPort = (unsigned short)atoi(listToken);
		if (queryPort == 0)
		{
			Con_DPrintf("[E] Invalid Port specified for '%s' in server list; skipping.\n", ip);
			break;
		}

		listToken = DK_strtok_r(NULL, separators, &listPtr); // Gamename
		if (!listToken)
		{
			Con_DPrintf("[E] Gamename not specified for '%s:%u' in server list; skipping.\n", ip, queryPort);
			break;
		}

		memset(&from, 0, sizeof(from));
		from.sin_addr.s_addr = addr.s_addr;
		from.sin_family = AF_INET;
		from.sin_port = htons(queryPort);

		if (!strcmp(listToken, "quakeworld"))
		{
			sendto(listener, (char *)qw_msg, sizeof(qw_msg), 0, (struct sockaddr *)&from, sizeof(from));
		}
		else if (!strcmp(listToken, "hexenworld"))
		{
			sendto(listener, (char *)hw_gspy_msg, sizeof(hw_gspy_msg), 0, (struct sockaddr *)&from, sizeof(from));
			sendto(listener, (char *)hw_hwq_msg, sizeof(hw_hwq_msg), 0, (struct sockaddr *)&from, sizeof(from));
		}
		else if (!strcmp(listToken, "quake2"))
		{
			/* FS: This is stupid because there's gloom and other things and yeah... so let's try all options. */
			sendto(listener, (char *)q2_msg, sizeof(q2_msg), 0, (struct sockaddr *)&from, sizeof(from));
			sendto(listener, (char *)q2_msg_alternate, sizeof(q2_msg_alternate), 0, (struct sockaddr *)&from, sizeof(from));
			sendto(listener, (char *)q2_msg_noOOB, sizeof(q2_msg_noOOB), 0, (struct sockaddr *)&from, sizeof(from));
			sendto(listener, (char *)q2_msg_alternate_noOOB, sizeof(q2_msg_alternate_noOOB), 0, (struct sockaddr *)&from, sizeof(from));
		}
		else
		{
			Con_DPrintf("[E] Invalid gamename for Master Server Query: %s!\n", listToken);
		}

		listToken = DK_strtok_r(NULL, separators, &listPtr); /* FS: Play it again, Sam. */
	}

	if (ip)
	{
		free(ip);
	}

	free(fileBuffer);
}

/* FS: Readapted from HWMQuery by sezero */
static void Parse_UDP_MS_List (unsigned char *tmp, char *gamename, int size)
{
	unsigned short port = 0;
	char ip[128];
	struct in_addr addr;
	struct sockaddr_in from;
	struct hostent *remoteHost;

	if (!tmp)
	{
		Con_DPrintf("[E] Parse_UDP_MS_List: No data to parse!\n");
		return;
	}

	if (!gamename)
	{
		Con_DPrintf("[E] Parse_UDP_MS_List: Gamename not specified!\n");
		return;
	}

	if (size < 6)
	{
		Con_DPrintf("[E] Parse_UDP_MS_List: Invalid packet size!\n");
		return;
	}

	/* each address is 4 bytes (ip) + 2 bytes (port) == 6 bytes */
	if (size % 6 != 0)
		printf("Warning: not counting truncated last entry %d\n", size);

	while (size >= 6)
	{
		port = ntohs(tmp[4] + (tmp[5] << 8));

		Com_sprintf(ip, sizeof(ip), "%u.%u.%u.%u", tmp[0], tmp[1], tmp[2], tmp[3]);

		remoteHost = gethostbyname(ip);
		if (!remoteHost) /* FS: Junk data or doesn't exist. */
		{
			Con_DPrintf("[E] Parse_UDP_MS_List: Could not resolve ip: %s.  Skipping!\n", ip);
			tmp += 6;
			size -= 6;
			continue;
		}

		addr.s_addr = *(u_long *) remoteHost->h_addr_list[0];

		memset(&from, 0, sizeof(from));
		from.sin_addr.s_addr = addr.s_addr;
		from.sin_family = AF_INET;
		from.sin_port = htons(port);
		AddServer(&from, SendAcknowledge(gamename), htons(port), gamename, ip);

		tmp += 6;
		size -= 6;
	}
}

static void Parse_UDP_Packet (SOCKET connection, int len, struct sockaddr_in *from)
{
	char serverName[64];
	char packetData[64];

	if (len > 4)
	{
		//parse this packet
		ParseResponse(from, incoming, len);
	}
	else
	{
		if (!memcmp(incoming, hw_hwq_msg, sizeof(hw_hwq_msg)))
		{
			Con_DPrintf("[I] HexenWorld hwmquery master server query.\n");
			SendUDPServerListToClient(from, "hexenworld");
		}
		else if (!memcmp(incoming, hw_gspy_msg, sizeof(hw_gspy_msg)))
		{
			Con_DPrintf("[I] HexenWorld GameSpy master server query.\n");
			SendUDPServerListToClient(from, "hexenworld");
		}
		else if (!memcmp(incoming, qw_msg, sizeof(qw_msg)))
		{
			Con_DPrintf("[I] QuakeSpy master server query.\n");
			SendUDPServerListToClient(from, "quakeworld");
		}
		else if (!memcmp(incoming, hw_server_msg, sizeof(hw_server_msg)))
		{
			Con_DPrintf("[I] HexenWorld Server sending a ping.\n");
			Com_sprintf(serverName, sizeof(serverName), "%s:%d,hexenworld\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port));
			AddServers_From_List_Execute(serverName, 0);
		}
		else if (!memcmp(incoming, qw_server_msg, sizeof(qw_server_msg)))
		{
			Con_DPrintf("[I] QuakeWorld Server sending a ping.\n");
			Com_sprintf(serverName, sizeof(serverName), "%s:%d,quakeworld\n",inet_ntoa(from->sin_addr), ntohs(from->sin_port));
			AddServers_From_List_Execute(serverName, 0);
		}
		else if (!memcmp(incoming, hw_server_shutdown, sizeof(hw_server_shutdown)))
		{
			Com_sprintf(packetData, sizeof(packetData), "heartbeat\\%d\\gamename\\hexenworld\\statechanged\\%s", ntohs(from->sin_port), GAMESPY_STATE_SHUTDOWN_STRING);
			HeartBeat(from, packetData);
		}
		else if (!memcmp(incoming, qw_server_shutdown, sizeof(qw_server_shutdown)))
		{
			Com_sprintf(packetData, sizeof(packetData), "heartbeat\\%d\\gamename\\quakeworld\\statechanged\\%s", ntohs(from->sin_port), GAMESPY_STATE_SHUTDOWN_STRING);
			HeartBeat(from, packetData);
		}
		else if (!memcmp(incoming, qspy_req_msg, sizeof(qspy_req_msg))) /* FS: QuakeSpy just wants something sent back to know it's alive on startup */
		{
			Con_DPrintf("[I] QuakeSpy master server verify.\n");
			sendto(connection, (char *)qspy_req_msg, sizeof(qspy_req_msg), 0, (struct sockaddr *)from, sizeof(*from));
		}
		else
		{
			Con_DPrintf("[W] runt packet from %s:%d\n", inet_ntoa(from->sin_addr), ntohs(from->sin_port));
			Con_DPrintf("[W] contents: %s\n", incoming);
		}
	}
}

void GenerateServersList (void)
{
	server_t	*server = &servers;
	char serverFile[MAX_PATH] = {0};
	char tempStr[23] = {0};
	int i = 0;
	FILE *listFile;

	lastServerListGeneration = (double)time(NULL);
	if (!bGenerateServerList || (serverListGenerationPath[0] == '\0'))
		return;

	Con_DPrintf("[I] Generating Server Lists...\n");

	while (gameTable[i].gamename != NULL)
	{
		server = &servers;
		Com_sprintf(serverFile, sizeof(serverFile), "%s\\%s.txt", serverListGenerationPath, gameTable[i].gamename);
		listFile = fopen(serverFile, "w");
		if (!listFile)
		{
			Con_DPrintf("[E] Failed to open %s for writing!\n", serverFile);
			i++;
			continue;
		}

		while (server->next)
		{
			server = server->next;

			if (server->heartbeats >= minimumHeartbeats && !server->shutdown_issued && server->validated && !strcmp(server->gamename, gameTable[i].gamename) && server->ip.sin_port && server->port != 0)
			{
				Com_sprintf(tempStr, sizeof(tempStr), "%s:%d\n", inet_ntoa(server->ip.sin_addr), ntohs(server->port));
				fwrite((char *)tempStr, 1, DG_strlen(tempStr), listFile);
			}
		}

		fflush(listFile);
		fclose(listFile);
		i++;
	}
}

void GenerateMasterDBBlob (void)
{
	FILE *dbFile;
	server_t *server = &servers;
	unsigned char *buff;
	unsigned short temp;
	static const unsigned short version = GSPY_DB_VERSION;
	size_t buflen = 0;
	size_t maxsize = (8 * numservers) + 2; /* FS: 6+2 for IP and port.  2 for version number. */

	dbFile = fopen("gsmaster.db", "wb");
	if (!dbFile)
	{
		Con_DPrintf("[E] Failed to open gsmaster.db for writing!\n");
		return;
	}

	buff = calloc(1, maxsize); // 6 bytes for game server ip and port, 2 byte for the gamename
	if (!buff)
	{
		Con_DPrintf("[E] Failed to allocate temporary buffer for database!\n");
		fclose(dbFile);
		return;
	}

	memcpy(buff + buflen, &version, 2);
	buflen += 2;

	while (server->next)
	{
		server = server->next;

		memcpy(buff + buflen, &server->ip.sin_addr, 4);
		buflen += 4;

		memcpy(buff + buflen, &server->port, 2);
		buflen += 2;

		temp = GameSpy_Get_Table_Number(server->gamename);
		memcpy(buff + buflen, &temp, 2);
		buflen += 2;
	}

	fwrite((unsigned char *)buff, buflen, 1, dbFile);
	fflush(dbFile);
	fclose(dbFile);
	free(buff);
}

void ReadMasterDBBlob (void)
{
	FILE *dbFile;
	long fileSize;
	long fileSizeParsed = 0;
	unsigned char *buff;
	char gamename[128];
	char ip[128];
	unsigned short port;
	unsigned short game;
	unsigned short version;
	struct in_addr addr;
	struct sockaddr_in from;
	struct hostent *remoteHost;

	dbFile = fopen("gsmaster.db", "rb");
	if (!dbFile)
	{
		Con_DPrintf("[E] Failed to open gsmaster.db for reading!\n");
		return;
	}

	fseek(dbFile, 0, SEEK_END);
	fileSize = ftell(dbFile);
	if (fileSize < 3) /* FS: Don't waste time if it's blank or just the header and no servers. */
	{
		fclose(dbFile);
		return;
	}

	rewind(dbFile);

	fread(&version, 2, 1, dbFile);
	if (version != GSPY_DB_VERSION)
	{
		Con_DPrintf("[E] Invalid version number for database!  Returned %u expected %d.\n", version, GSPY_DB_VERSION);
		fclose(dbFile);
		return;
	}

	fileSizeParsed += 2;

	if (fileSize < 8)
	{
		Con_DPrintf("[W] Empty gsmaster.db file.  Aborting.\n");
		fclose(dbFile);
		return;
	}

	buff = calloc(1, (6) * sizeof(unsigned char));
	if (!buff)
	{
		Con_DPrintf("[E] Failed to allocate temporary buffer for database!\n");
		fclose(dbFile);
		return;
	}

	while (fileSizeParsed < fileSize)
	{
		fread(buff, 6, 1, dbFile);
		fileSizeParsed += 6;
		port = ntohs(buff[4] + (buff[5] << 8));
		Com_sprintf(ip, sizeof(ip), "%u.%u.%u.%u", buff[0], buff[1], buff[2], buff[3]);

		remoteHost = gethostbyname(ip);
		if (!remoteHost) /* FS: Junk data or doesn't exist. */
		{
			Con_DPrintf("[E] Parse_UDP_MS_List: Could not resolve ip: %s.  Skipping!\n", ip);
			fread(&game, 2, 1, dbFile);
			fileSizeParsed += 2;
			continue;
		}

		addr.s_addr = *(u_long *)remoteHost->h_addr_list[0];
		fread(&game, 2, 1, dbFile);
		fileSizeParsed += 2;

		DG_strlcpy(gamename, gameTable[game].gamename, sizeof(gamename));

		memset(&from, 0, sizeof(from));
		from.sin_addr.s_addr = addr.s_addr;
		from.sin_family = AF_INET;
		from.sin_port = htons(port);
		AddServer(&from, SendAcknowledge(gamename), htons(port), gamename, ip);
	}

	free(buff);
}
