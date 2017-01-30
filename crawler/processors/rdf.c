/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2015 BBC
 */

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

/* crawld module for processing RDF resources */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_processors.h"

static unsigned long rdf_addref(PROCESSOR *me);
static unsigned long rdf_release(PROCESSOR *me);
static CRAWLSTATE rdf_process(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type);
static int rdf_process_headers(PROCESSOR *me, CRAWLOBJ *obj);
static int rdf_process_link(PROCESSOR *me, CRAWLOBJ *obj, const char *value, librdf_uri *resource);
static CRAWLSTATE rdf_preprocess(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type);
static int rdf_postprocess(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type);
static CRAWLSTATE rdf_process_obj(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type, struct curl_slist **subjects);
static int rdf_process_node(PROCESSOR *me, CRAWLOBJ *obj, librdf_node *node, struct curl_slist **subjects);

static struct processor_api_struct rdf_api = {
	NULL,
	rdf_addref,
	rdf_release,
	rdf_process
};

PROCESSOR *
rdf_create(CRAWL *crawler)
{
	PROCESSOR *p;
	
	p = (PROCESSOR *) crawl_alloc(crawler, sizeof(PROCESSOR));
	if(!p)
	{
		return NULL;
	}
	p->api = &rdf_api;
	p->refcount = 1;
	p->crawl = crawler;
	p->world = librdf_new_world();
	p->storage = librdf_new_storage(p->world, "memory", NULL, NULL);
	if(!p->storage)
	{
		librdf_free_world(p->world);
		crawl_free(crawler, p);
		return NULL;
	}
	crawl_set_accept(crawler, "application/rdf+xml;q=1.0, text/rdf;q=0.6, application/n-triples;q=1.0, text/plain;q=0.1, text/turtle;q=1.0, application/x-turtle;q=1.0, application/turtle;q=1.0, text/n3;q=0.3, text/rdf+n3;q=0.3, application/rdf+n3;q=0.3, application/x-trig;q=1.0, text/x-nquads;q=1.0, */*;q=0.1");
	return p;
}

static unsigned long
rdf_addref(PROCESSOR *me)
{
	me->refcount++;
	return me->refcount;
}

static unsigned long
rdf_release(PROCESSOR *me)
{
	size_t c;

	me->refcount--;
	if(!me->refcount)
	{
		if(me->world)
		{
			librdf_free_world(me->world);
		}
		if(me->license_predicates)
		{
			for(c = 0; me->license_predicates[c]; c++)
			{
				crawl_free(me->crawl, me->license_predicates[c]);
			}
			crawl_free(me->crawl, me->license_predicates);
		}
		if(me->license_whitelist)
		{
			for(c = 0; me->license_whitelist[c]; c++)
			{
				crawl_free(me->crawl, me->license_whitelist[c]);
			}
			crawl_free(me->crawl, me->license_whitelist);
		}
		if(me->license_blacklist)
		{
			for(c = 0; me->license_blacklist[c]; c++)
			{
				crawl_free(me->crawl, me->license_blacklist[c]);
			}
			crawl_free(me->crawl, me->license_blacklist);
		}
		crawl_free(me->crawl, me->content_type);		
		crawl_free(me->crawl, me);
		return 0;
	}
	return me->refcount;
}

static CRAWLSTATE
rdf_process(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type)
{
	CRAWLSTATE r;
	struct curl_slist *subjects, *p;
	
	subjects = NULL;
	r = rdf_preprocess(me, obj, uri, content_type);
	if(r == COS_ACCEPTED)
	{
		r = rdf_process_obj(me, obj, uri, content_type, &subjects);
		if(r == COS_ACCEPTED)
		{
			for(p = subjects; p; p = p->next)
			{
				queue_add_uristr(me->crawl, p->data);
			}
		}
		curl_slist_free_all(subjects);
	}
	rdf_postprocess(me, obj, uri, content_type);
	return r;
}

static CRAWLSTATE
rdf_preprocess(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type)
{
	int status;
	char *t;
	
	(void) obj;
	
	status = crawl_obj_status(obj);

	/* RESDATA-1059 Anansi sometimes fails to follow 303s.
	 * Changing the end of this scope to 303 instead of 304
	 * means we'll follow 303s rather than skipping them.
	 * Why this doesn't work, I don't know, but a process of
	 * elimination and hackery determined that this was one
	 * of the blocker points for crawling a site using 303s.
	 */
	if(status > 300 && status < 303)
	{
		/* Don't process the payload of redirects */
		log_printf(LOG_INFO, MSG_I_RDF_SKIPPED_REDIRECT " <%s> (RDF: HTTP status %d)\n", uri, status);
		return COS_SKIPPED;
	}
	if(status < 200 || status > 299)
	{
		/* Don't bother processing failed responses */
		errno = EINVAL;
		log_printf(LOG_INFO, MSG_I_RDF_FAILED_HTTP " <%s> (RDF: HTTP status %d)\n", uri, status);
		return COS_SKIPPED;
	}
	if(!content_type)
	{
		/* We can't parse if we don't know what it is */
		log_printf(LOG_INFO, MSG_I_RDF_FAILED_NOTYPE " <%s>\n", uri);
		errno = EINVAL;
		return COS_SKIPPED;
	}
	me->content_type = crawl_strdup(me->crawl, content_type);
	t = strchr(me->content_type, ';');
	if(t)
	{
		*t = 0;
		t--;
		while(t > me->content_type)
		{
			if(!isspace(*t))
			{
				break;
			}
			*t = 0;
			t--;
		}
	}
	me->model = librdf_new_model(me->world, me->storage, NULL);
	if(!me->model)
	{
		return COS_ERR;
	}
	me->uri = librdf_new_uri(me->world, (const unsigned char *) uri);
	if(!me->uri)
	{
		return COS_ERR;
	}
	me->parser_type = NULL;
	if(!strcmp(me->content_type, "text/turtle"))
	{
		me->parser_type = "turtle";
	}
	else if(!strcmp(me->content_type, "application/rdf+xml"))
	{
		me->parser_type = "rdfxml";
	}
	else if(!strcmp(me->content_type, "text/n3"))
	{
		me->parser_type = "turtle";
	}
	else if(!strcmp(me->content_type, "text/plain"))
	{
		me->parser_type = "ntriples";
	}
	else if(!strcmp(me->content_type, "application/n-triples"))
	{
		me->parser_type = "ntriples";
	}
	else if(!strcmp(me->content_type, "text/x-nquads") || !strcmp(me->content_type, "application/n-quads"))
	{
		me->parser_type = "nquads";
	}
	log_printf(LOG_DEBUG, "rdf_preprocess: content_type='%s', parser_type='%s'\n", me->content_type, me->parser_type);
	if(!me->parser_type)
	{
		
		log_printf(LOG_INFO, MSG_I_RDF_REJECTED_TYPE " <%s> (RDF: no parser found for '%s')\n", uri, me->content_type);
		return COS_SKIPPED;
	}
	return COS_ACCEPTED;
}

static int
rdf_postprocess(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type)
{
	(void) obj;
	(void) uri;
	(void) content_type;
	
	if(me->fobj)
	{
		fclose(me->fobj);
		me->fobj = NULL;
	}
	if(me->uri)
	{
		librdf_free_uri(me->uri);
		me->uri = NULL;
	}
	if(me->model)
	{
		librdf_free_model(me->model);
		me->model = NULL;
	}
	crawl_free(me->crawl, me->content_type);
	me->content_type = NULL;
	return 0;
}

static CRAWLSTATE
rdf_process_obj(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type, struct curl_slist **subjects)
{
	librdf_parser *parser;
	librdf_stream *stream;
	librdf_statement *st;
	CRAWLSTATE r;

	(void) uri;
	(void) content_type;

	log_printf(LOG_DEBUG, "RDF: processing <%s>\n", uri);
	parser = librdf_new_parser(me->world, me->parser_type, NULL, NULL);
	if(!parser)
	{
		return COS_ERR;
	}
	me->fobj = crawl_obj_open(obj);
	if(!me->fobj)
	{
		log_printf(LOG_ERR, MSG_E_RDF_ERROR_PAYLOAD " <%s>\n", uri);
		librdf_free_parser(parser);
		return COS_ERR;
	}
	if(librdf_parser_parse_file_handle_into_model(parser, me->fobj, 0, me->uri, me->model))
	{
		log_printf(LOG_INFO, MSG_I_RDF_FAILED_PARSE " <%s> (RDF: failed to parse '%s' as '%s')\n", uri, content_type, me->parser_type);
		librdf_free_parser(parser);
		return COS_ERR;
	}
	librdf_free_parser(parser);
	rdf_process_headers(me, obj);
	if(me->filter)
	{
		r = me->filter(me, obj, uri, me->model);
		if(r != COS_ACCEPTED)
		{
			log_printf(LOG_DEBUG, "RDF: filter declined further processing of this resource\n");
			return r;
		}
	}
	stream = librdf_model_as_stream(me->model);
	if(!stream)
	{
		return COS_ERR;
	}
	while(!librdf_stream_end(stream))
	{
		st = librdf_stream_get_object(stream);
		
		rdf_process_node(me, obj, librdf_statement_get_subject(st), subjects);
		rdf_process_node(me, obj, librdf_statement_get_predicate(st), subjects);
		rdf_process_node(me, obj, librdf_statement_get_object(st), subjects);
		
		librdf_stream_next(stream);
	}
	librdf_free_stream(stream);
	return COS_ACCEPTED;
}

/* Debian Wheezy ships with libjansson 2.3, which doesn't include
 * json_array_foreach()
 */
#ifndef json_array_foreach
# define json_array_foreach(array, index, value) \
	for(index = 0; index < json_array_size(array) && (value = json_array_get(array, index)); index++)
#endif

static int
rdf_process_headers(PROCESSOR *me, CRAWLOBJ *obj)
{
	json_t *headers, *values, *header;
	size_t d;
	const char *value, *loc, *key;
	librdf_uri *resource;

	loc = crawl_obj_content_location(obj);
	if(!loc)
	{
		loc = crawl_obj_uristr(obj);
	}
	log_printf(LOG_DEBUG, "RDF: Content-Location is <%s>\n", loc);
	resource = librdf_new_uri(me->world, (const unsigned char *) loc);	
	headers = crawl_obj_headers(obj, 0);
	json_object_foreach(headers, key, values)
	{
		if(!strcasecmp(key, "link"))
		{
			log_printf(LOG_DEBUG, "RDF: Found header \"%s\"\n", key);
			json_array_foreach(values, d, header)
			{
				value = json_string_value(header);
				if(value)
				{
					rdf_process_link(me, obj, value, resource);
				}
			}
		}
	}
	json_decref(headers);
	librdf_free_uri(resource);
	return 0;
}

static int
rdf_process_link(PROCESSOR *me, CRAWLOBJ *obj, const char *value, librdf_uri *resource)
{
	static const char *relbase = "http://www.w3.org/1999/xhtml/vocab#";
	const char *t, *pend, *vstart, *s;
	char *anchorstr, *uristr, *relstr, *p;
	librdf_uri *anchor, *uri, *rel;
	int q, abs;
	librdf_node *subject, *predicate, *object;
	librdf_statement *st;

	(void) obj;

	rel = NULL;
	while(*value)
	{
		anchorstr = NULL;
		uristr = NULL;
		relstr = NULL;
		t = value;
		while(*t && isspace(*t))
		{
			t++;
		}
		if(*t != '<')
		{
			log_printf(LOG_INFO, MSG_I_RDF_MALFORMEDLINK " ('%s')\n", value);
			return -1;
		}
		value = t + 1;
		while(*t && *t != '>')
		{
			t++;
		}
		if(!*t)
		{
			log_printf(LOG_INFO, MSG_I_RDF_MALFORMEDLINK " ('%s')\n", value);
			return -1;
		}
		uristr = (char *) crawl_alloc(me->crawl, t - value + 1);
		if(!uristr)
		{
			log_printf(LOG_ERR, "RDF: failed to allocate memory for Link URI\n");
			return -1;
		}
		strncpy(uristr, value, t - value);
		uristr[t - value] = 0;
		value = t + 1;		
		while(*value && *value != ',')
		{
			vstart = NULL;
			q = 0;
			while(*value == ' ' || *value == '\t')
			{
				value++;
			}
			if(!*value)
			{
				break;
			}
			t = value;
			/* Parse a single parameter */
			while(*t)
			{
				if(*t == '=' || *t == ';')
				{
					break;
				}
				if(*t == ' ' || *t == '\t')
				{
					log_printf(LOG_NOTICE, MSG_I_RDF_MALFORMEDLINKPARAM " ('%s')\n", value);
					return -1;
				}
				t++;
			}
			if(!*t || *t == ',')
			{
				break;
			}
			if(*t == ';')
			{
				t++;
				value = t;
				continue;
			}
			pend = t;
			t++;
			while(*t == ' ' || *t == '\t')
			{
				t++;
			}
			vstart = t;
			while(*t)
			{
				if(q)
				{
					if(*t == q)
					{
						q = 0;
					}
					t++;
					continue;				
				}
				if(*t == '"')
				{
					q = *t;
					t++;
					continue;
				}
				if(*t == ';')
				{
					break;
				}
				if(*t == ',')
				{
					break;
				}
				t++;
			}		
			/* Parse a 'rel' parameter */
			if(!relstr && pend - value == 3 && !strncmp(value, "rel", 3))
			{
				/* If the relation is not something that looks like a URI,
				 * create one by concatenating it to relbase; otherwise,
				 * just parse the relation as a URI.
				 */
				relstr = (char *) crawl_alloc(me->crawl, t - vstart + strlen(relbase) + 1);
				p = relstr;
				abs = 0;
				for(s = vstart; s < t; s++)
				{
					if(*s == ':' || *s == '/')
					{
						abs = 1;
						break;
					}
				}
				if(!abs)
				{
					strcpy(relstr, relbase);
					p = strchr(relstr, 0);
				}
				for(s = vstart; s < t; s++)
				{
					if(*s == '"')
					{
						continue;
					}
					*p = *s;
					p++;
				}
				*p = 0;
			}
			else if(!anchorstr && pend - value == 6 && !strncmp(value, "anchor", 6))
			{
				anchorstr = (char *) crawl_alloc(me->crawl, t - vstart + 1);
				p = anchorstr;
				for(s = vstart; s < t; s++)
				{
					if(*s == '"')
					{
						continue;
					}
					*p = *s;
					p++;
				}
				*p = 0;
			}
			value = t;
			if(!*value || *value == ',')
			{
				break;
			}
			value++;
		}
		/* We have now parsed all parameters */
		anchor = NULL;
		rel = NULL;
		uri = NULL;
		if(anchorstr)
		{
			anchor = librdf_new_uri_relative_to_base(resource, (const unsigned char *) anchorstr);
		}
		else
		{
			anchor = resource;
		}
		if(relstr)
		{
			uri = librdf_new_uri_relative_to_base(anchor, (const unsigned char *) uristr);
			
			/* Only process links which actually have a relation */
			rel = librdf_new_uri(me->world, (const unsigned char *) relstr);
			log_printf(LOG_DEBUG, "RDF: Link <%s> <%s> <%s>\n",
					   (const char *) librdf_uri_to_string(anchor),
					   (const char *) librdf_uri_to_string(rel),
					   (const char *) librdf_uri_to_string(uri));
			/* Create a new triple (content-location, relation, target) */
			subject = librdf_new_node_from_uri(me->world, anchor);
			predicate = librdf_new_node_from_uri(me->world, rel);
			object = librdf_new_node_from_uri(me->world, uri);
			st = librdf_new_statement_from_nodes(me->world, subject, predicate, object);
			/* Add the triple to the model */
			librdf_model_add_statement(me->model, st);
			librdf_free_statement(st);
			librdf_free_uri(rel);
			librdf_free_uri(uri);
		}
		if(anchor && anchor != resource)
		{
			librdf_free_uri(anchor);
		}
		crawl_free(me->crawl, anchorstr);
		anchorstr = NULL;
		crawl_free(me->crawl, relstr);
		relstr = NULL;
		crawl_free(me->crawl, uristr);
		uristr = NULL;
		
		if(*value)
		{
			value++;
		}
	}
	return 0;
}

static int
rdf_process_node(PROCESSOR *me, CRAWLOBJ *obj, librdf_node *node, struct curl_slist **subjects)
{
	librdf_uri *uri;
	struct curl_slist *p;
	const char *s;

	(void) me;
	(void) obj;
	
	if(!librdf_node_is_resource(node))
	{
		return 0;
	}
	uri = librdf_node_get_uri(node);
	if(!uri)
	{
		return -1;
	}
	s = (const char *) librdf_uri_as_string(uri);
	if(!s)
	{
		return -1;
	}
	for(p = *subjects; p; p = p->next)
	{
		if(!strcmp(s, p->data))
		{
			return 0;
		}
	}
	log_printf(LOG_DEBUG, "RDF: adding <%s>\n", s);
	*subjects = curl_slist_append(*subjects, s);
	return 0;
}

int
rdf_set_filter(PROCESSOR *me, rdf_filter_cb filter)
{
	me->filter = filter;

	return 0;
}

librdf_world *
rdf_world(PROCESSOR *me)
{
	return me->world;
}
