/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014 BBC.
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

#ifndef P_CRAWLD_H_
# define P_CRAWLD_H_                   1

# define _BSD_SOURCE                   1
# define _FILE_OFFSET_BITS             64

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
# include <signal.h>
# include <pthread.h>

# include "libcrawl.h"
# include "libsupport.h"
# include "liburi.h"

# define CRAWLER_APP_NAME               "crawler"

typedef struct context_struct CONTEXT;
typedef struct processor_struct PROCESSOR;
typedef struct queue_struct QUEUE;

/* Crawl object states */
typedef enum
{
	/* Has not yet been crawled */
	COS_NEW,
	/* Crawling failed */
	COS_FAILED,
	/* The processor or policy rejected the resource */
	COS_REJECTED,
	/* The resource was processed */
	COS_ACCEPTED,
	/* Never set by crawld itself: external processing has completed */
	COS_COMPLETE
} CRAWLSTATE;

struct context_struct
{
	struct context_api_struct *api;
	/* The below should be considered private by would-be modules */
	unsigned long refcount;
	CRAWL *crawl;
	int crawler_id;
	int thread_id;
	int cache_id;
	PROCESSOR *processor;
	QUEUE *queue;
	size_t cfgbuflen;
	char *cfgbuf;
};

struct context_api_struct
{
	void *reserved;
	unsigned long (*addref)(CONTEXT *me);
	unsigned long (*release)(CONTEXT *me);
	int (*crawler_id)(CONTEXT *me);
	int (*cache_id)(CONTEXT *me);
	CRAWL *(*crawler)(CONTEXT *me);	
	const char *(*config_get)(CONTEXT *me, const char *key, const char *defval);
	int (*config_get_int)(CONTEXT *me, const char *key, int defval);
};

#ifndef QUEUE_STRUCT_DEFINED
struct queue_struct
{
	struct queue_api_struct *api;
};
#endif

struct queue_api_struct
{
	void *reserved;
	unsigned long (*addref)(QUEUE *me);
	unsigned long (*release)(QUEUE *me);
	int (*next)(QUEUE *me, URI **next);
	int (*add)(QUEUE *me, URI *uri, const char *uristr);
	int (*updated_uri)(QUEUE *me, URI *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
	int (*updated_uristr)(QUEUE *me, const char *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
	int (*unchanged_uri)(QUEUE *me, URI *uri, int error);
	int (*unchanged_uristr)(QUEUE *me, const char *uri, int error);
};

#ifndef PROCESSOR_STRUCT_DEFINED
struct processor_struct
{
	struct processor_api_struct *api;
};
#endif

struct processor_api_struct
{
	void *reserved;
	unsigned long (*addref)(PROCESSOR *me);
	unsigned long (*release)(PROCESSOR *me);
	int (*process)(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type);
};

extern volatile int crawld_terminate;

CONTEXT *context_create(int crawler_offset);

int thread_init(void);
int thread_cleanup(void);
int thread_create(int crawler_offset);
void *thread_handler(void *arg);

int processor_init(void);
int processor_cleanup(void);
int processor_init_crawler(CRAWL *crawler, CONTEXT *data);
int processor_cleanup_crawler(CRAWL *crawl, CONTEXT *data);

int queue_init(void);
int queue_cleanup(void);
int queue_init_crawler(CRAWL *crawler, CONTEXT *data);
int queue_cleanup_crawler(CRAWL *crawler, CONTEXT *data);
int queue_add_uristr(CRAWL *crawler, const char *str);
int queue_add_uri(CRAWL *crawler, URI *uri);
int queue_updated_uri(CRAWL *crawl, URI *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
int queue_updated_uristr(CRAWL *crawl, const char *uristr, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
int queue_unchanged_uri(CRAWL *crawl, URI *uri, int error);
int queue_unchanged_uristr(CRAWL *crawl, const char *uristr, int error);

int policy_init(void);
int policy_cleanup(void);
int policy_init_crawler(CRAWL *crawler, CONTEXT *data);
int policy_uri(CRAWL *crawl, URI *uri, const char *uristr, void *userdata);

PROCESSOR *rdf_create(CRAWL *crawler);
PROCESSOR *lod_create(CRAWL *crawler);

QUEUE *db_create(CONTEXT *ctx);

#endif /*!P_CRAWLD_H_*/
