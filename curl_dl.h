#ifndef __curl_dl_h
#define __curl_dl_h

#define HTTP_OK 200
#define HTTP_REST 206
#define HTTP_UNAUTHORIZED 401
#define HTTP_FORBIDDEN 403
#define HTTP_NOT_FOUND 404
void CURL_HTTP_Init (void);
void CURL_HTTP_Shutdown (void);
int CURL_HTTP_StartDownload (const char *url, const char *filename, const char *gamename);
void CURL_HTTP_Update (void);
void CURL_HTTP_Reset (void *);
void CURL_HTTP_AddToQueue (const char *url, const char *filename, const char *gamename);

#endif//__curl_dl_h
