/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2016 BBC
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
static int lod_init_list(PROCESSOR *me, char ***list, const char *section, const char *key);
static int lod_list_cb(const char *key, const char *value, void *userdata);
static int lod_valid_license(PROCESSOR *me, const char *uri);

struct list_data_struct
{
	PROCESSOR *me;
	char **list;
	size_t size;
	size_t count;
};

PROCESSOR *
spider_processor_lod_create_(SPIDER *spider)
{
	PROCESSOR *p;

	p = spider_processor_rdf_create_(spider);
	if(!p)
	{
		return NULL;
	}
	rdf_set_filter(p, lod_rdf_filter);
	lod_init_list(p, &(p->license_whitelist), "lod:licenses", "whitelist");
	lod_init_list(p, &(p->license_blacklist), "lod:licenses", "blacklist");
	lod_init_list(p, &(p->license_predicates), "lod:licenses", "predicate");
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
		log_printf(LOG_INFO, MSG_I_LOD_REJECTED " <%s>: (LOD: no suitable licensing triple)\n", uri);
		return COS_REJECTED;
	}
	log_printf(LOG_DEBUG, "LOD: suitable licensing triple located\n");
	return COS_ACCEPTED;
}

static int
lod_init_list(PROCESSOR *me, char ***list, const char *section, const char *key)
{
	struct list_data_struct data;
	size_t c;

	memset(&data, 0, sizeof(data));
	data.me = me;
	if(config_get_all(section, key, lod_list_cb, &data) < 0)
	{
		for(c = 0; c < data.count; c++)
		{
			crawl_free(me->crawl, data.list[c]);
		}
		crawl_free(me->crawl, data.list);
		return -1;
	}
	*list = data.list;
	return 0;
}

static int
lod_list_cb(const char *key, const char *value, void *userdata)
{
	struct list_data_struct *data;
	char **p;

	(void) key;

	data = (struct list_data_struct *) userdata;
	if(data->count + 1 >= data->size)
	{
		p = (char **) crawl_realloc(data->me->crawl, data->list, sizeof(char *) * (data->size + 8));
		if(!p)
		{
			return -1;
		}
		data->list = p;
		data->size += 8;
		memset(&(data->list[data->count]), 0, sizeof(char *) * (data->size - data->count));
	}
	data->list[data->count] = crawl_strdup(data->me->crawl, value);
	if(!data->list[data->count])
	{
		return -1;
	}
	data->count++;
	return 0;
}

static int
lod_check_licenses(PROCESSOR *me, librdf_model *model, const char *subject)
{
	size_t c;
	librdf_world *world;
	librdf_stream *stream;
	librdf_statement *query, *st;
	librdf_node *pnode, *onode;
	librdf_uri *puri, *ouri;
	const char *predicate, *object;

	if(!me->license_predicates)
	{
		log_printf(LOG_DEBUG, "LOD: no licensing predicates configured, will not check licensing\n");
		return 0;
	}
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
			for(c = 0; me->license_predicates[c]; c++)
			{
				if(!strcmp(me->license_predicates[c], predicate))
				{
					log_printf(LOG_DEBUG, "LOD: found predicate <%s>\n", predicate);
					onode = librdf_statement_get_object(st);
					if(librdf_node_is_resource(onode) &&
					   (ouri = librdf_node_get_uri(onode)) &&
					   (object = (const char *) librdf_uri_as_string(ouri)))
					{
						if(lod_valid_license(me, object))
						{
								log_printf(LOG_DEBUG, "LOD: found license <%s>\n", object);
								librdf_free_stream(stream);
								librdf_free_statement(query);
								return 1;
						}
						log_printf(LOG_DEBUG, "LOD: license <%s> is not acceptable\n", object);
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

static int
lod_valid_license(PROCESSOR *me, const char *uri)
{
	size_t c;

	if(me->license_blacklist)
	{
		for(c = 0; me->license_blacklist[c]; c++)
		{
			if(!strcmp(me->license_blacklist[c], uri))
			{
				/* License is blacklisted */
				return 0;
			}
		}
	}
	if(me->license_whitelist)
	{
		for(c = 0; me->license_whitelist[c]; c++)
		{
			if(!strcmp(me->license_whitelist[c], uri))
			{
				/* License is whitelisted */
				return 1;
			}
		}
		/* License is not whitelisted */
		return 0;
	}
	/* License is not blacklisted, and there is no whitelist */
	return 1;
}
