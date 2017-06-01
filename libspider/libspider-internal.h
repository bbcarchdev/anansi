/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2015-2017 BBC
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

/* This file contains the INTERNAL interface to libspider: that is, the one
 * used by applications, rather than plug-ins
 */

#ifndef LIBSPIDER_INTERNAL_H_
# define LIBSPIDER_INTERNAL_H_         1

# include "libspider.h"

/* For backwards-compatibility */
typedef SPIDER CONTEXT;

SPIDER *context_create(int crawler_offset);

int processor_init(void);
int processor_cleanup(void);
int processor_init_context(SPIDER *data);

int queue_init(void);
int queue_cleanup(void);
int queue_init_context(SPIDER *data);

int queue_updated_uri(CRAWL *crawl, URI *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
int queue_updated_uristr(CRAWL *crawl, const char *uristr, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
int queue_unchanged_uri(CRAWL *crawl, URI *uri, int error);
int queue_unchanged_uristr(CRAWL *crawl, const char *uristr, int error);

int policy_init(void);
int policy_cleanup(void);
int policy_init_context(CONTEXT *data);

#endif /*!LIBSPIDER_INTERNAL_H_*/
