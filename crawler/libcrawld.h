/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2015 BBC
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

#ifndef LIBCRAWLD_H_
# define LIBCRAWLD_H_                   1

# include "libcrawl.h"

/* libcrawld is a wrapper around libcrawl for providing crawler daemons */

typedef struct context_struct CONTEXT;
typedef struct processor_struct PROCESSOR;
typedef struct queue_struct QUEUE;

#ifndef CONTEXT_STRUCT_DEFINED
struct context_struct
{
	struct context_api_struct *api;
};
#endif

struct context_api_struct
{
	void *reserved;
	unsigned long (*addref)(CONTEXT *me);
	unsigned long (*release)(CONTEXT *me);
	int (*crawler_id)(CONTEXT *me);
	int (*cache_id)(CONTEXT *me);
	int (*thread_id)(CONTEXT *me);
	int (*threads)(CONTEXT *me);
	int (*caches)(CONTEXT *me);
	CRAWL *(*crawler)(CONTEXT *me);	
	QUEUE *(*queue)(CONTEXT *me);
	PROCESSOR *(*processor)(CONTEXT *me);
	const char *(*config_get)(CONTEXT *me, const char *key, const char *defval);
	int (*config_get_int)(CONTEXT *me, const char *key, int defval);
	int (*set_base)(CONTEXT *me, int base);
	int (*set_threads)(CONTEXT *me, int threads);
	int (*terminate)(CONTEXT *me);
	int (*terminated)(CONTEXT *me);
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
	int (*next)(QUEUE *me, URI **next, CRAWLSTATE *state);
	int (*add)(QUEUE *me, URI *uri, const char *uristr);
	int (*updated_uri)(QUEUE *me, URI *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
	int (*updated_uristr)(QUEUE *me, const char *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
	int (*unchanged_uri)(QUEUE *me, URI *uri, int error);
	int (*unchanged_uristr)(QUEUE *me, const char *uri, int error);
	int (*force_add)(QUEUE *me, URI *uri, const char *uristr);
	int (*set_crawlers)(QUEUE *me, int count);
	int (*set_caches)(QUEUE *me, int count);
	int (*set_crawler)(QUEUE *me, int id);
	int (*set_cache)(QUEUE *me, int id);
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

CONTEXT *context_create(int crawler_offset);

int processor_init(void);
int processor_cleanup(void);
int processor_init_context(CONTEXT *data);

int queue_init(void);
int queue_cleanup(void);
int queue_init_context(CONTEXT *data);

int queue_add_uristr(CRAWL *crawler, const char *str);
int queue_add_uri(CRAWL *crawler, URI *uri);

int queue_updated_uri(CRAWL *crawl, URI *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
int queue_updated_uristr(CRAWL *crawl, const char *uristr, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
int queue_unchanged_uri(CRAWL *crawl, URI *uri, int error);
int queue_unchanged_uristr(CRAWL *crawl, const char *uristr, int error);

int policy_init(void);
int policy_cleanup(void);
int policy_init_context(CONTEXT *data);

#endif /*!LIBCRAWLD_H_*/
