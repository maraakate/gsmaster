#ifndef SHARED_H
#define SHARED_H

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#ifdef _MSC_VER
#include "win32\msinttypes\stdint.h"
#endif
#define DG_MISC_NO_GNU_SOURCE
#include "dg_misc.h" /* FS: Caedes special safe string stuff */
#include "curl_dl.h"
#include "osinfo.h"

#define SRV_RUN		1
#define SRV_STOP	0
#define SRV_STOPPED	-1

#define VERSION "0.7"

#ifndef NULL
#define NULL 0
#endif

#ifndef bool
typedef enum {false, true} bool;
#endif

#define KEY_LEN 32	// give us some space
#define MAXPENDING 16 /* FS: Max pending TCP connections */
#define MAX_INCOMING_LEN 65536 /* FS: made this a #define.  GameSpy doesnt send anything larger than 1024; but other servers do.  Max I've seen is about ~4000 from large lists.  A real issue with the old UDP QW/HW/Q2 queries. */
#define MAX_GAMENAME_LEN 16 /* FS: Max gamename length used for game table and server structs */
#define DEFAULTHEARTBEAT (5*60) /* FS: 5 minutes */
#define DEFAULTSERVERLISTGENERATIONTIME (2.0*60.0) /* FS: 2 minutes */
#define MASTERLISTDLTIME (15.0*60.0) /* FS: 15 minutes */
#define DBSAVETIME (5.0*60.0) /* FS: 5 minutes */

#define GAMESPY_VALIDATION_REQUIRED_OFF 0
#define GAMESPY_VALIDATION_REQUIRED_CLIENTS_ONLY 1
#define GAMESPY_VALIDATION_REQUIRED_SERVERS_ONLY 2
#define GAMESPY_VALIDATION_REQUIRED_ALL 3

#define GAMESPY_STATE_UPDATE_STRING "1" /* FS: Map or game name change */
#define GAMESPY_STATE_SHUTDOWN_STRING "2" /* FS: Actually shutting down */

#define LOGTCP_DEFAULTNAME "gspytcp.log"

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

#define MAXPRINTMSG 16384
#define MAX_INFO_STRING 64
#define MAX_DNS_NAMELEN 254
#define MAX_PORT_LEN 5
#define MAX_SERVERLIST_LINE (MAX_DNS_NAMELEN+1+MAX_PORT_LEN+1+MAX_GAMENAME_LEN) /* FS: 1 == ',' separator */
#define MAX_GSPY_VAL 89 /* FS: See gsmalg.c */

#define MOTD_SIZE 1024

#define MAX_QUERY_SOCKETS 30

#define GAMESPY_ENCTYPE0 0
#define GAMESPY_ENCTYPE1 1
#define GAMESPY_ENCTYPE2 2

/* FS: From HoT: For ioctl sockets */
#ifdef __DJGPP__
#define	IOCTLARG_T	(char*)
#else
#define IOCTLARG_T
#endif

//
// These are Windows specific but need to be defined here so GCC won't barf
//
#define REGKEY_GSMASTERSERVER "SOFTWARE\\GSMasterServer" // Our config data goes here
#define REGKEY_BIND_IP "Bind_IP"
#define REGKEY_BIND_PORT "Bind_Port"
#define REGKEY_BIND_PORT_TCP "Bind_Port_TCP" /* FS: For GameSpy TCP port */

// Out of band data preamble
#define OOB_SEQ "\xff\xff\xff\xff" //32 bit integer (-1) as string sequence for out of band data

extern int debug;
extern int timestamp;
#ifdef _MSC_VER /* FS: Not on mingw. */
extern bool bMinidumpAutogen;
#endif

// Knightmare 05/27/12- buffer-safe variant of vsprintf
// This may be different on different platforms, so it's abstracted
#ifdef _MSC_VER	// _WIN32
__inline int GSM_vsnprintf (char *Dest, size_t Count, const char *Format, va_list Args) {
	int ret = _vsnprintf(Dest, Count, Format, Args);
	Dest[Count-1] = 0;	// null terminate
	return ret;
}
#else
#define GSM_vsnprintf vsnprintf
#endif // _MSC_VER

#endif // SHARED_H
