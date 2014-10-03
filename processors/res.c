/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014 BBC.
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

/* Extension to the RDF processor which applies RES-specific policies */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_processors.h"

static int res_rdf_filter(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, librdf_model *model);

PROCESSOR *
res_create(CRAWL *crawler)
{
	PROCESSOR *p;

	p = rdf_create(crawler);
	if(!p)
	{
		return NULL;
	}
	rdf_set_filter(p, res_rdf_filter);
	return p;
}

static int
res_rdf_filter(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, librdf_model *model)
{
	(void) me;
	(void) obj;
	(void) model;

	log_printf(LOG_DEBUG, "RES: processing <%s>\n", uri);
	
	return 1;
}
