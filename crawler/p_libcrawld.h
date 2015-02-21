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

#ifndef P_LIBCRAWLD_H_
# define P_LIBCRAWLD_H_                 1

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
# include <uuid/uuid.h>

# define CONTEXT_STRUCT_DEFINED         1

# include "libcrawl.h"
# include "libcrawld.h"
# include "libsupport.h"
# include "liburi.h"

struct context_struct
{
	struct context_api_struct *api;
	pthread_rwlock_t lock;
	unsigned long refcount;
	CRAWL *crawl;
	int thread_offset;
	int thread_base;
	int crawler_id;
	int thread_id;
	int cache_id;
	int thread_count;
	PROCESSOR *processor;
	QUEUE *queue;
	size_t cfgbuflen;
	char *cfgbuf;
	int terminate;
};

int policy_uri_(CRAWL *crawl, URI *uri, const char *uristr, void *userdata);

/* Processor constructors */
PROCESSOR *rdf_create(CRAWL *crawler);
PROCESSOR *lod_create(CRAWL *crawler);

/* Queue constructors */
QUEUE *db_create(CONTEXT *ctx);

#endif /*!P_LIBCRAWLD_H_*/
