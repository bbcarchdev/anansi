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

#define PROCESSOR_STRUCT_DEFINED       1

#include "p_processors.h"

static unsigned long rdf_addref(PROCESSOR *me);
static unsigned long rdf_release(PROCESSOR *me);
static int rdf_process(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type);

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

struct processor_struct
{
	struct processor_api_struct *api;
	unsigned long refcount;
	CRAWL *crawl;
	librdf_world *world;
	librdf_storage *storage;
	librdf_model *model;
	librdf_uri *uri;
	char *content_type;
	const char *parser_type;
	FILE *fobj;
	rdf_filter_cb filter;
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
	me->refcount--;
	if(!me->refcount)
	{
		if(me->world)
		{
			librdf_free_world(me->world);
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