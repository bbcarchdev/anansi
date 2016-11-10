/* Anansi plug-in
 *
 * Author: Christophe Gueret <christophe.gueret@bbc.co.uk>
 *
 * Copyright (c) 2016 BBC
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libcrawl.h"
#include "libsupport.h"

#define CRAWL_PLUGIN_NAME "logger"

static int log_uri(CRAWL *restrict context, const char *restrict uristr, void *data);

/* Anansi plug-in entry-point */
int
anansi_entry(CRAWL *context, CRAWLPLUGINENTRYTYPE type, void *handle)
{
	(void) handle;

	switch(type)
	{
	case CRAWL_ATTACHED:
		log_printf(LOG_DEBUG, CRAWL_PLUGIN_NAME " plug-in: initialising\n");
		// Register to signal
		crawl_plugin_attach_signal(context, QUEUE_URI_ADDED, "logger", log_uri, NULL);
		break;
	case CRAWL_DETACHED:
		// De-register
		break;
	}
	return 0;
}

static int log_uri(CRAWL *restrict context, const char *restrict uristr, void *data)
{
	log_printf(LOG_INFO, CRAWL_PLUGIN_NAME " plug-in: Added %s to the queue !\n", uristr);
	return 0;
}
