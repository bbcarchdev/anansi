/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2015 BBC.
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
 *  See the License for the specific language governing permissions and23
 *  limitations under the License.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libcrawl.h"

void *
crawl_alloc(CRAWL *restrict crawl, size_t nbytes)
{
	void *p;

	p = calloc(1, nbytes);
	if(!p)
	{
		crawl_log_(crawl, LOG_CRIT, MSG_C_NOMEM ": failed to allocate %lu bytes\n", (unsigned long) nbytes);
		abort();
	}
	return p;
}

char *
crawl_strdup(CRAWL *restrict crawl, const char *src)
{
	char *p;

	p = strdup(src);
	if(!p)
	{
		crawl_log_(crawl, LOG_CRIT, MSG_C_NOMEM ": failed to duplicate string of %lu bytes\n", (unsigned long) strlen(src) + 1);
		abort();
	}
	return p;
}

void *
crawl_realloc(CRAWL *restrict crawl, void *restrict ptr, size_t nbytes)
{
	void *p;

	p = realloc(ptr, nbytes);
	if(!p)
	{
		crawl_log_(crawl, LOG_CRIT, MSG_C_NOMEM ": failed to resize buffer to %lu bytes\n", (unsigned long) nbytes);
		abort();
	}
	return p;
}

void
crawl_free(CRAWL *restrict crawl, void *restrict ptr)
{
	(void) crawl;

	free(ptr);
}

