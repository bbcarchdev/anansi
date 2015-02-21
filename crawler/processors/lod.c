/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2015 BBC
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

static int lod_rdf_filter(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, librdf_model *model);
static int lod_check_licenses(PROCESSOR *me, librdf_model *model, const char *subject);

PROCESSOR *
lod_create(CRAWL *crawler)
{
	PROCESSOR *p;

	p = rdf_create(crawler);
	if(!p)
	{
		return NULL;
	}
	rdf_set_filter(p, lod_rdf_filter);
	return p;
}

static int
lod_rdf_filter(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, librdf_model *model)
{
	int found;
	const char *cl;

	log_printf(LOG_DEBUG, "LOD: processing <%s>\n", uri);
	found = 0;
	if(!found && lod_check_licenses(me, model, uri))
	{
		found = 1;
	}
	if(!found)
	{
		cl = crawl_obj_content_location(obj);
		if(cl && strcmp(cl, uri))
		{
			if(lod_check_licenses(me, model, cl))
			{
				found = 1;
			}
		}
	}
	if(!found)
	{			
		log_printf(LOG_DEBUG, "LOD: failed to locate a suitable licensing triple\n");
		return 0;
	}
	log_printf(LOG_DEBUG, "LOD: suitable licensing triple located\n");
	return 1;
}

static int
lod_check_licenses(PROCESSOR *me, librdf_model *model, const char *subject)
{
	static const char *predicates[] = {
		"http://purl.org/dc/terms/rights",
		"http://purl.org/dc/terms/license",
		"http://purl.org/dc/terms/accessRights",
		"http://creativecommons.org/ns#license",
		"http://www.w3.org/1999/xhtml/vocab#license",
		NULL
	};
	
	static const char *licenses[] = {
		"http://creativecommons.org/publicdomain/zero/1.0/",
		"http://id.loc.gov/about/",
		"http://creativecommons.org/licenses/by/4.0/",
		"http://reference.data.gov.uk/id/open-government-licence",
		"http://bbcarchdev.github.io/licences/dps/1.0#id",
		"http://creativecommons.org/licenses/by/1.0/",
		"http://creativecommons.org/licenses/by/2.5/",
		"http://creativecommons.org/licenses/by/3.0/",
		"http://creativecommons.org/licenses/by/3.0/us/",
		"https://www.nationalarchives.gov.uk/doc/open-government-licence/version/2/",
		"https://www.nationalarchives.gov.uk/doc/open-government-licence/version/2/",
		"http://www.nationalarchives.gov.uk/doc/open-government-licence/version/1/",
		"http://www.nationalarchives.gov.uk/doc/open-government-licence/version/1/",
		NULL
	};

	size_t c, d;
	librdf_world *world;
	librdf_stream *stream;
	librdf_statement *query, *st;
	librdf_node *pnode, *onode;
	librdf_uri *puri, *ouri;
	const char *predicate, *object;

	log_printf(LOG_DEBUG, "LOD: examining <%s> for licensing triples\n", subject);
	world = rdf_world(me);
	query = librdf_new_statement(world);
	pnode = librdf_new_node_from_uri_string(world, (const unsigned char *) subject);
	librdf_statement_set_subject(query, pnode);
	stream = librdf_model_find_statements(model, query);

	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		pnode = librdf_statement_get_predicate(st);
		if(librdf_node_is_resource(pnode) &&
		   (puri = librdf_node_get_uri(pnode)) &&
		   (predicate = (const char *) librdf_uri_as_string(puri)))
		{
			for(c = 0; predicates[c]; c++)
			{
				if(!strcmp(predicates[c], predicate))
				{
					log_printf(LOG_DEBUG, "LOD: found predicate <%s>\n", predicate);
					onode = librdf_statement_get_object(st);
					if(librdf_node_is_resource(onode) &&
					   (ouri = librdf_node_get_uri(onode)) &&
					   (object = (const char *) librdf_uri_as_string(ouri)))
					{
						for(d = 0; licenses[d]; d++)
						{
							if(!strcmp(licenses[d], object))
							{
								log_printf(LOG_DEBUG, "LOD: found license <%s>\n", object);
								librdf_free_stream(stream);
								librdf_free_statement(query);
								return 1;
							}
						}
					}
				}
			}
		}
		librdf_stream_next(stream);
	}

	librdf_free_stream(stream);
	librdf_free_statement(query);
	return 0;
}
