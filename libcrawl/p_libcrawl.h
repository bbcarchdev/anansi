/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2015 BBC
 */

/*
 * Copyright 2013 Mo McRoberts.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef P_LIBCRAWL_H_
# define P_LIBCRAWL_H_                  1

# define _BSD_SOURCE                    1
# define _FILE_OFFSET_BITS              64

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <ctype.h>
# include <errno.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <syslog.h>

# include <curl/curl.h>
# include <openssl/sha.h>

# include "libcrawl.h"

/* Cached object has two parts:
 *
 * JSON metadata blob <key>.json, containing:
 *
 * 'updated':       Unix timestamp of last fetch
 * 'status':        HTTP status code
 * 'redirect':      received Location header in the case of a redirect, if any
 * 'type':          received Content-Type header, if any
 * 'headers':{ }    parsed HTTP headers (the status line has a key of ':';
 *                  all other values are arrays containing at least one value)
 * 
 * e.g.:
 * {
 *  "headers":{
 *    ":":"HTTP/1.1 301 Moved Permanently",
 *    "Content-Length":["230"],
 *    "Content-Type":["text/html; charset=iso-8859-1"],
 *    "Date":["Sun, 23 Jun 2013 13:32:06 GMT"],
 *    "Location":["http://example.com/"],
 *    "Server":["Apache/2.2.22 (Unix) DAV/2 PHP/5.3.15 with Suhosin-Patch mod_ssl/2.2.22 OpenSSL/0.9.8x"]
 *  },
 *  "redirect":"http://example.com/",
 *  "status":301,
 *  "type":"text/html; charset=iso-8859-1",
 *  "updated":1371997296
 * }
 *
 * Accompanying the .json file is a .payload file containing the recieved body, if any.
 */

# define HEADER_ALLOC_BLOCK            128
# define OBJ_READ_BLOCK                1024
# define CACHE_INFO_SUFFIX             "json"
# define CACHE_PAYLOAD_SUFFIX          ""
# define CACHE_TMP_SUFFIX              ".tmp"

/* Log messages - libcrawl */
# define MSG_C_NOMEM                    "%%ANANSI-C-1000: Memory allocation failure"
# define MSG_N_NONEXT                   "%%ANANSI-N-1001: crawl_perform(): no 'next resource' handler has been registered"

/* disk cache */
# define MSG_E_DISK_PAYLOADREAD         "%%ANANSI-E-4000: disk: failed to open payload for reading"
# define MSG_E_DISK_INFOREAD            "%%ANANSI-E-4001: disk: failed to open sidecar for reading"
# define MSG_E_DISK_PAYLOADCOMMIT       "%%ANANSI-E-4002: disk: failed to rename temporary payload file"
# define MSG_E_DISK_INFOCOMMIT          "%%ANANSI-E-4003: disk: failed to rename temporary sidecar file"
# define MSG_E_DISK_PAYLOADRM           "%%ANANSI-E-4004: disk: failed to roll back payload"
# define MSG_E_DISK_INFORM              "%%ANANSI-E-4005: disk: failed to roll back sidecar"
# define MSG_E_DISK_PAYLOADWRITE        "%%ANANSI-E-4006: disk: failed to open temporary payload file for writing"
# define MSG_E_DISK_MKDIR               "%%ANANSI-E-4007: disk: failed to create cache directory"
# define MSG_E_DISK_DIRSTAT             "%%ANANSI-E-4008: disk: failed to stat cache directory"

/* S3 cache */
# define MSG_E_S3_TMPFILE               "%%ANANSI-E-4100: S3: failed to create temporary file"
# define MSG_E_S3_HTTP                  "%%ANANSI-E-4101: S3: failed to retrieve object from cache"

struct crawl_struct
{
	void *userdata;
	CRAWLCACHE cache;
	URI *cacheuri;
	URI_INFO *uri;
	char *cachepath;
	char *cachefile;
	char *cachetmp;
	size_t cachefile_len;
	char *accept;
	char *ua;
	int verbose;
	time_t cache_min;
	crawl_uri_policy_cb uri_policy;
	crawl_updated_cb updated;
	crawl_next_cb next;
	crawl_failed_cb failed;
	crawl_checkpoint_cb checkpoint;
	crawl_unchanged_cb unchanged;
	crawl_prefetch_cb prefetch;
	void (*logger)(int priority, const char *format, va_list ap);
};

struct crawl_object_struct
{
	CRAWL *crawl;
	CACHEKEY key;
	int fresh;
	time_t updated;
	int status;
	jd_var info;
	URI *uri;
	char *uristr;
	char *payload;
	uint64_t size;
};

struct crawl_fetch_data_struct
{
	CRAWL *crawl;
	CRAWLOBJ *obj;
	CURL *ch;
	int rollback;
	time_t now;
	char *headers;
	size_t headers_size;
	size_t headers_len;
	time_t cachetime;
	FILE *info;
	FILE *payload;
	long status;
	int have_size;
	uint64_t size;
	int generated_info;
	int checkpoint_invoked;
};

void crawl_log_(CRAWL *obj, int priority, const char *format, ...);

CRAWLOBJ *crawl_obj_create_(CRAWL *crawl, URI *uri);
int crawl_obj_locate_(CRAWLOBJ *obj);
int crawl_obj_replace_(CRAWLOBJ *obj, jd_var *dict);

int crawl_cache_init_(CRAWL *crawl);
int crawl_cache_key_(CRAWL *crawl, CACHEKEY dest, const char *uri);
char *cache_uri_(CRAWL *crawl, const CACHEKEY key);
FILE *cache_open_payload_write_(CRAWL *crawl, const CACHEKEY key);
int cache_close_payload_rollback_(CRAWL *crawl, const CACHEKEY key, FILE *f);
int cache_close_payload_commit_(CRAWL *crawl, const CACHEKEY key, FILE *f, CRAWLOBJ *obj);

int cache_info_read_(CRAWL *crawl, const CACHEKEY key, jd_var *dict);
int cache_info_write_(CRAWL *crawl, const CACHEKEY key, jd_var *dict);

#endif /*!P_LIBCRAWL_H_*/
