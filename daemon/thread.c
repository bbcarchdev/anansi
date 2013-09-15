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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_crawld.h"

void *
thread_handler(void *arg)
{
	CRAWL *crawler;
	CRAWLDATA *data;
	
	(void) arg;
	
	data = (CRAWLDATA *) calloc(1, sizeof(CRAWLDATA));
	if(!data)
	{
		return NULL;
	}
	data->crawler_id = 1;
	data->cache_id = 1;
	data->ncrawlers = 1;
	data->ncaches = 1;
	crawler = crawl_create();
	crawl_set_userdata(crawler, (void *) data);
	crawl_set_verbose(crawler, 1);
	processor_init_crawler(crawler, data);
	queue_init_crawler(crawler, data);
	policy_init_crawler(crawler);
	
	crawl_perform(crawler);

	queue_cleanup_crawler(crawler, data);
	processor_cleanup_crawler(crawler, data);
	crawl_destroy(crawler);
	return NULL;
}