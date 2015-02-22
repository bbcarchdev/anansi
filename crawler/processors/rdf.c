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
static int rdf_process(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type);
static int rdf_process_headers(PROCESSOR *me, CRAWLOBJ *obj);
static int rdf_process_link(PROCESSOR *me, CRAWLOBJ *obj, const char *value, librdf_uri *resource);
static int rdf_preprocess(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type);
static int rdf_postprocess(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type);
static int rdf_process_obj(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type, struct curl_slist **subjects);
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
	
	p = (PROCESSOR *) calloc(1, sizeof(PROCESSOR));
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
		free(p);
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
				free(me->license_predicates[c]);
			}
			free(me->license_predicates);
		}
		if(me->license_whitelist)
		{
			for(c = 0; me->license_whitelist[c]; c++)
			{
				free(me->license_whitelist[c]);
			}
			free(me->license_whitelist);
		}
		if(me->license_blacklist)
		{
			for(c = 0; me->license_blacklist[c]; c++)
			{
				free(me->license_blacklist[c]);
			}
			free(me->license_blacklist);
		}
		free(me->content_type);
		free(me);
		return 0;
	}
	return me->refcount;
}

static int
rdf_process(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type)
{
	int r;
	struct curl_slist *subjects, *p;
	
	subjects = NULL;
	r = rdf_preprocess(me, obj, uri, content_type);
	if(r > 0)
	{
		r = rdf_process_obj(me, obj, uri, content_type, &subjects);
		if(r > 0)
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

static int
rdf_preprocess(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type)
{
	int status;
	char *t;
	
	(void) obj;
	
	status = crawl_obj_status(obj);
	if(status > 300 && status < 304)
	{
		/* Don't process the payload of redirects */
		return 0;
	}
	if(status < 200 || status > 299)
	{
		/* Don't bother processing failed responses */
		errno = EINVAL;
		return -1;
	}
	if(!content_type)
	{
		/* We can't parse if we don't know what it is */
		errno = EINVAL;
		return -1;
	}
	me->content_type = strdup(content_type);
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
		return -1;
	}
	me->uri = librdf_new_uri(me->world, (const unsigned char *) uri);
	if(!me->uri)
	{
		return -1;
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
		log_printf(LOG_WARNING, "RDF: No suitable parser type found for '%s'\n", me->content_type);
		return 0;
	}
	return 1;
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
	free(me->content_type);
	me->content_type = NULL;
	return 0;
}

static int
rdf_process_obj(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type, struct curl_slist **subjects)
{
	librdf_parser *parser;
	librdf_stream *stream;
	librdf_statement *st;
	
	(void) uri;
	(void) content_type;

	log_printf(LOG_DEBUG, "RDF: processing <%s>\n", uri);
	parser = librdf_new_parser(me->world, me->parser_type, NULL, NULL);
	if(!parser)
	{
		return -1;
	}
	me->fobj = crawl_obj_open(obj);
	if(!me->fobj)
	{
		log_printf(LOG_ERR, "RDF: failed to open payload for processing\n");
		librdf_free_parser(parser);
		return -1;
	}
	if(librdf_parser_parse_file_handle_into_model(parser, me->fobj, 0, me->uri, me->model))
	{
		log_printf(LOG_ERR, "RDF: failed to parse '%s' (%s) as '%s'\n", uri, content_type, me->parser_type);
		librdf_free_parser(parser);
		return -1;		
	}
	librdf_free_parser(parser);
	rdf_process_headers(me, obj);
	if(me->filter)
	{
		if(!me->filter(me, obj, uri, me->model))
		{
			log_printf(LOG_DEBUG, "RDF: filter declined further processing of this resource\n");
			return 0;
		}
	}
	stream = librdf_model_as_stream(me->model);
	if(!stream)
	{
		return -1;
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
	return 1;
}

static int
rdf_process_headers(PROCESSOR *me, CRAWLOBJ *obj)
{
	jd_var headers = JD_INIT, keys = JD_INIT, *ks, *array, *str;
	size_t c, d, num, anum;
	const char *value, *loc;
	librdf_uri *resource;

	loc = crawl_obj_content_location(obj);
	if(!loc)
	{
		loc = crawl_obj_uristr(obj);
	}
	log_printf(LOG_DEBUG, "RDF: Content-Location is <%s>\n", loc);
	resource = librdf_new_uri(me->world, (const unsigned char *) loc);
	
	JD_SCOPE
	{
		crawl_obj_headers(obj, &headers, 0);
				jd_keys(&keys, &headers);
		num = jd_count(&keys);
		for(c = 0; c < num; c++)
		{
			ks = jd_get_idx(&keys, c);
			if(!ks || ks->type != STRING)
			{
				continue;
			}
			if(!strcasecmp(jd_bytes(ks, NULL), "link"))
			{
				array = jd_get_key(&headers, ks, 0);
				if(array && array->type == ARRAY)
				{
					anum = jd_count(array);
					for(d = 0; d < anum; d++)
					{
						str = jd_get_idx(array, d);
						if(!str || str->type != STRING)
						{
							continue;
						}
						value = jd_bytes(str, NULL);
						rdf_process_link(me, obj, value, resource);
					}
				}
			}
		}
	}
	librdf_free_uri(resource);
	return 0;
}

static int
rdf_process_link(PROCESSOR *me, CRAWLOBJ *obj, const char *value, librdf_uri *resource)
{
	static const char *relbase = "http://www.w3.org/1999/xhtml/vocab#";
	const char *t, *pend, *vstart, *s;
	char *relstr, *p;
	librdf_uri *uri, *rel;
	int q, abs;
	librdf_node *subject, *predicate, *object;
	librdf_statement *st;

	(void) obj;

	t = value;
	while(*t && isspace(*t))
	{
		t++;
	}
	if(*t != '<')
	{
		log_printf(LOG_NOTICE, "RDF: ignoring malformed Link header (%s)\n", value);
		return -1;
	}
	value = t;
	while(*t && *t != '>')
	{
		t++;
	}
	if(!*t)
	{
		log_printf(LOG_NOTICE, "RDF: ignoring malformed Link header (%s)\n", value);
		return -1;
	}
	uri = librdf_new_uri2(me->world, (const unsigned char *) (value + 1), (size_t) (t - value - 1));
	if(!uri)
	{
		log_printf(LOG_ERR, "RDF: failed to parse URI: <%s>\n", (const char *) librdf_uri_to_string(uri));
		return -1;
	}
	log_printf(LOG_DEBUG, "RDF: parsed Link URI: <%s>\n", (const char *) librdf_uri_to_string(uri));
	value = t + 1;
	rel = NULL;
	while(*value)
	{
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
			while(*t)
			{
				if(*t == '=' || *t == ';')
				{
					break;
				}
				if(*t == ' ' || *t == '\t')
				{
					log_printf(LOG_NOTICE, "RDF: ignoring link relation with malformed parameters ('%s')\n", value);
					librdf_free_uri(uri);
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
			if(pend - value == 3 && !strncmp(value, "rel", 3))
			{
				/* If the relation is not something that looks like a URI,
				 * create one by concatenating it to relbase; otherwise,
				 * just parse the relation as a URI.
				 */
				relstr = (char *) malloc(t - vstart + strlen(relbase) + 1);
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
				rel = librdf_new_uri(me->world, (const unsigned char *) relstr);
				log_printf(LOG_DEBUG, "RDF: parsed link relation URI: <%s>\n", (const char *) librdf_uri_to_string(rel));
				free(relstr);
				/* Create a new triple (content-location, relation, target) */
				subject = librdf_new_node_from_uri(me->world, resource);
				predicate = librdf_new_node_from_uri(me->world, rel);
				object = librdf_new_node_from_uri(me->world, uri);
				st = librdf_new_statement_from_nodes(me->world, subject, predicate, object);
				/* Add the triple to the model */
				librdf_model_add_statement(me->model, st);
				librdf_free_statement(st);
				librdf_free_uri(rel);
			}
			value = t;
			if(!*value || *value == ',')
			{
				break;
			}
			value++;
		}
		if(*value)
		{
			value++;
		}
	}
	librdf_free_uri(uri);
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
