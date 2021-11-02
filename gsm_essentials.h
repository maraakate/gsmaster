#ifndef _GSM_ESSENTIALS_H
#define _GSM_ESSENTIALS_H

#include "shared.h"

void Com_sprintf (char *dest, size_t size, const char *fmt, ...);
char *Con_Timestamp (const char *msg);
void Con_DPrintf (const char *fmt, ...);
char *GSM_strtok_r (char *s, const char *delim, char **last);
char *Info_ValueForKey (const char *s, const char *key); /* FS: From Quake 2 */
char *GSM_strlwr (char *s); /* FS: Some compilers may not have this */
void Parse_ServerList (size_t fileSize, char *fileBuffer, char *gamenameFromHttp);
void AddServers_From_List_Execute (char *fileBuffer, char *gamenameFromHttp); /* FS: From Quake 2 */

/* FS: Aluigi.org stuff */
unsigned char *gsseckey (unsigned char *dst, unsigned char *src, unsigned char *key, int enctype);
int create_enctype1_buffer (const char *validate_key, char *input, int inputLen, char *output);

#endif // _GSM_ESSENTIALS_H
