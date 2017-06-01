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

#ifndef P_LIBSPIDER_H_
# define P_LIBSPIDER_H_                 1

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <ctype.h>
# include <errno.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <stdarg.h>
# include <syslog.h>
# include <signal.h>
# include <pthread.h>
# include <uuid/uuid.h>

/* The context object is implemented by libspider internally (see below) */
# define SPIDER_STRUCT_DEFINED         1
/* The maximum number of policy handlers which may be associated with a
 * spider.
 */
# define SPIDER_MAX_POLICIES           8

# include "libcrawl.h"
# include "libspider-internal.h"
# include "liburi.h"

/* This structure specifies the private data belonging to instances of SPIDER
 * that are created by spider_create(). In theory an application could provide
 * its own implementation which was entirely compatible but had completely
 * differently-structured private data, although this is unlikely - however,
 * this is the approach employed by queues, processors and policy handlers,
 * and so spiders have been structured consistently.
 */
struct spider_struct
{
	struct spider_api_struct *api;
	pthread_rwlock_t lock;
	unsigned long refcount;
	CRAWL *crawl;
	int attached:1;
	int terminated:1;
	int oneshot:1;
	int suspended:1;
	int base;
	int total_threads;
	int local_index;
	int prev_local_index;
	PROCESSOR *processor;
	QUEUE *queue;
	size_t cfgbuflen;
	char *cfgbuf;
	CLUSTER *cluster;
	void *userdata;
	SPIDERCALLBACKS cb;
	SPIDERPOLICY *policies[SPIDER_MAX_POLICIES];
	size_t npolicies;
};

/* Processors */
int spider_processor_attach_(SPIDER *spider, PROCESSOR *processor);
PROCESSOR *spider_processor_rdf_create_(SPIDER *spider);
PROCESSOR *spider_processor_lod_create_(SPIDER *spider);

/* Queue handling */
int spider_queue_attach_(SPIDER *spider, QUEUE *queue);
QUEUE *spider_queue_db_create_(SPIDER *ctx, URI *uri);

/* Policies */
int spider_policy_attach_(SPIDER *spider, SPIDERPOLICY *policy);
SPIDERPOLICY *spider_policy_schemes_create_(SPIDER *spider);
SPIDERPOLICY *spider_policy_contenttypes_create_(SPIDER *spider);

#endif /*!P_LIBSPIDER_H_*/
