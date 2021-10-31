/*
Copyright (c) 2015-2021 Frank Sapone <fhsapone@gmail.com>

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifdef USE_CURL
#define CURL_STATICLIB
#define CURL_DISABLE_LDAP
#define CURL_DISABLE_LDAPS
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
	#include <io.h>
	#include "curl/curl.h"
#else
	#include "libcurl/include/curl/curl.h"
#endif // WIN32

#define CURL_ERROR(x)	curl_easy_strerror(x)

#include "shared.h"
#include "master.h"
#include "dk_essentials.h"

#define MAX_CONCURRENT_CURL_HANDLES 5

static int curl_init_error;
static int curl_number_of_active_handles = 0;
static CURL *easy_handles[MAX_CONCURRENT_CURL_HANDLES];
static CURLM *multi_handle;

#define MAX_URLLENGTH	4000 /* FS: See http://boutell.com/newfaq/misc/urllength.html.  Apache is 4000 max. */

typedef struct curl_helper_s
{
	void *downloadHandle;
	char filename[MAX_PATH];
	char gamename[MAX_PATH];
	bool inUse;
} curl_helper_t;

typedef struct curl_queue_s
{
	char url[MAX_URLLENGTH];
	char filename[MAX_PATH];
	char gamename[MAX_PATH];
	struct curl_queue_s *next;
} curl_queue_t;

static curl_helper_t curl_helper[MAX_CONCURRENT_CURL_HANDLES];
static curl_queue_t *curl_queue = NULL;

static curl_queue_t *CURL_HTTP_GetQueue (const char *url, const char *filename, const char *gamename)
{
	curl_queue_t *next = curl_queue;

	if (!url || !filename || !gamename || !curl_queue)
	{
		return NULL;
	}

	while (next)
	{
		if (!stricmp(next->url, url) && !stricmp(next->filename, filename) && !stricmp(next->gamename, gamename))
		{
			return next;
		}

		next = next->next;
	}

	return NULL;
}

void CURL_HTTP_AddToQueue (const char *url, const char *filename, const char *gamename)
{
	curl_queue_t *var;

	if (!url || !filename || !gamename)
	{
		return;
	}

	if (CURL_HTTP_GetQueue(url, filename, gamename))
	{
		return;
	}

	var = calloc(1, sizeof(curl_queue_t));
	if (!var)
	{
		printf("[E] Error allocating memory!\n");
	}

	strncpy(var->url, url, sizeof(var->url)-1);
	strncpy(var->filename, filename, sizeof(var->filename)-1);
	strncpy(var->gamename, gamename, sizeof(var->gamename)-1);

	var->next = curl_queue;
	curl_queue = var;
}

static void CURL_HTTP_ScheduleQueue (void)
{
	curl_queue_t *var;
	curl_queue_t *old;

	if (!curl_queue || (curl_number_of_active_handles >= MAX_CONCURRENT_CURL_HANDLES))
	{
		return;
	}

	var = curl_queue;
	while (var->next)
	{
		var = var->next;
	}

	if (CURL_HTTP_StartDownload(var->url, var->filename, var->gamename))
	{
		old = &curl_queue[0];
		if (curl_queue == var)
		{
			free(curl_queue);
			curl_queue = NULL;
			old = NULL;
		}

		while (curl_queue)
		{
			if (curl_queue->next == var)
			{
				free(curl_queue->next);
				curl_queue->next = NULL;
				break;
			}

			curl_queue = curl_queue->next;
		}

		curl_queue = old;
	}
}

static int http_progress (void *clientp, double dltotal, double dlnow,
			   double ultotal, double uplow)
{
	return 0;	//non-zero = abort
}

static size_t http_write (void *ptr, size_t size, size_t nmemb, void *stream)
{
	return fwrite(ptr, 1, size * nmemb, stream);
}

void CURL_HTTP_Init (void)
{
	if ((curl_init_error = curl_global_init (CURL_GLOBAL_NOTHING)))
	{
		return;
	}

	multi_handle = curl_multi_init();
}

void CURL_HTTP_Shutdown (void)
{
	if (curl_init_error)
	{
		return;
	}

	curl_multi_cleanup(multi_handle);
	curl_global_cleanup();
}

int CURL_HTTP_StartDownload (const char *url, const char *filename, const char *gamename)
{
	char completedURL[MAX_URLLENGTH];
	int i;

	if (!filename)
	{
		printf("[E] CURL_HTTP_StartDownload: Filename is NULL!\n");
		return 0;
	}

	if (!gamename)
	{
		printf ("[E] CURL_HTTP_StartDownload: Gamename is NULL!\n");
		return 0;
	}

	if (!url)
	{
		printf("[E] CURL_HTTP_StartDownload: URL is NULL!\n");
		return 0;
	}

	i = curl_number_of_active_handles;
	if (i >= MAX_CONCURRENT_CURL_HANDLES)
	{
		return 0;
	}

	if (curl_helper[i].inUse)
	{
		if (curl_number_of_active_handles < MAX_CONCURRENT_CURL_HANDLES)
		{
			int bFound = 0;

			for (i = 0; i < MAX_CONCURRENT_CURL_HANDLES; i++)
			{
				if (!curl_helper[i].inUse)
				{
					bFound = 1;
					break;
				}
			}

			if (!bFound)
			{
				return 0;
			}
		}
		else
		{
			return 0;
		}
	}

	memset(&curl_helper[i], 0, sizeof(curl_helper_t));
	strncpy(curl_helper[i].filename, filename, sizeof(curl_helper[i].filename) - 1);
	strncpy(curl_helper[i].gamename, gamename, sizeof(curl_helper[i].gamename) - 1);

	if (!curl_helper[i].downloadHandle)
	{
		curl_helper[i].downloadHandle = fopen(curl_helper[i].filename, "wb");

		if (!curl_helper[i].downloadHandle)
		{
			printf("[E] CURL_HTTP_StartDownload: Failed to open %s\n", curl_helper[i].filename);
			return 0;
		}
	}

	Com_sprintf(completedURL, sizeof(completedURL), "%s", url);
	Con_DPrintf("[I] HTTP Download URL: %s\n", completedURL);
	Con_DPrintf("    Saving to: %s\n", curl_helper[i].filename);

	easy_handles[i] = curl_easy_init();

	curl_easy_setopt(easy_handles[i], CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(easy_handles[i], CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(easy_handles[i], CURLOPT_PROGRESSFUNCTION, http_progress);
	curl_easy_setopt(easy_handles[i], CURLOPT_WRITEFUNCTION, http_write);
	curl_easy_setopt(easy_handles[i], CURLOPT_URL, completedURL);
	curl_easy_setopt(easy_handles[i], CURLOPT_WRITEDATA, curl_helper[i].downloadHandle);
	curl_easy_setopt(easy_handles[i], CURLOPT_PRIVATE, &curl_helper[i]);
#ifdef CURL_GRAB_HEADER
	curl_easy_setopt(easy_handles[i], CURLOPT_WRITEHEADER, dl);
	curl_easy_setopt(easy_handles[i], CURLOPT_HEADERFUNCTION, http_header);
#endif
	curl_multi_add_handle(multi_handle, easy_handles[i]);

	curl_helper[i].inUse = true;
	curl_number_of_active_handles++;

	return 1;
}

void CURL_HTTP_Update (void)
{
	int         running_handles;
	int         messages_in_queue;
	CURLMsg *msg;

	curl_multi_perform(multi_handle, &running_handles);
	while ((msg = curl_multi_info_read(multi_handle, &messages_in_queue)))
	{
		if (msg->msg == CURLMSG_DONE)
		{
			long        response_code;
			curl_helper_t *ptr;

			curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
			curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &ptr);

			Con_DPrintf("HTTP URL response code: %li\n", response_code);

			if ((response_code == HTTP_OK || response_code == HTTP_REST))
			{
				if (!ptr)
				{
					printf("[E] Couldn't extract data pointer from CURL easy_handle!\n");
					continue;
				}

				printf("[I] HTTP Download of %s completed\n", ptr->filename);

				if (ptr->downloadHandle)
				{
					fclose(ptr->downloadHandle);
				}

				ptr->downloadHandle = NULL;
				ptr->inUse = false;
				Add_Servers_From_List(ptr->filename, ptr->gamename);

			}
			else
			{
				printf("[E] HTTP Download Failed: %ld.\n", response_code);

				if (!ptr)
				{
					printf("[E] Couldn't extract data pointer from CURL easy_handle!\n");
					continue;
				}

				if (ptr->downloadHandle)
				{
					fclose(ptr->downloadHandle);
				}

				ptr->downloadHandle = NULL;
				ptr->inUse = false;
			}

			CURL_HTTP_Reset(msg->easy_handle);
		}
	}

	CURL_HTTP_ScheduleQueue();
}

void CURL_HTTP_Reset (void *easy_handle)
{
	CURL *oldHandlePtr;
	int i;

	if (!easy_handle)
	{
		printf("[E] Invalid pointer passed to CURL_HTTP_Reset()!\n");
		return;
	}

	oldHandlePtr = easy_handle;

	curl_multi_remove_handle(multi_handle, (CURL *)easy_handle);
	curl_easy_cleanup(easy_handle);
	easy_handle = 0;
	curl_number_of_active_handles--;
	if (curl_number_of_active_handles < 0)
		curl_number_of_active_handles = 0;

	for (i = 0; i < MAX_CONCURRENT_CURL_HANDLES; i++)
	{
		if (oldHandlePtr == easy_handles[i])
		{
			easy_handles[i] = 0;
			break;
		}
	}
}
#else
void CURL_HTTP_Init (void) {}
void CURL_HTTP_Shutdown (void) {}
int CURL_HTTP_StartDownload (const char *url, const char *filename, const char *gamename) { return 0; }
void CURL_HTTP_Update (void) {}
void CURL_HTTP_Reset (void *) {}
void CURL_HTTP_AddToQueue (const char *url, const char *filename, const char *gamename) {}
#endif // USE_CURL
