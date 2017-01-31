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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libcrawld.h"

/* Implements a libcrawl policy handler which implements URI scheme and
 * content type white/blacklisting.
 */

static int policy_checkpoint_(CRAWL *crawl, CRAWLOBJ *obj, int *status, void *userdata);
static char **policy_create_list_(const char *name, const char *defval);
static int policy_destroy_list_(char **list);

static char **types_whitelist;
static char **types_blacklist;
static char **schemes_whitelist;
static char **schemes_blacklist;

int
policy_init(void)
{
	types_whitelist = policy_create_list_("policy:content-types:whitelist", NULL);
	types_blacklist = policy_create_list_("policy:content-types:blacklist", NULL);
	schemes_whitelist = policy_create_list_("policy:schemes:whitelist", NULL);
	schemes_blacklist = policy_create_list_("policy:schemes:blacklist", NULL);
	return 0;
}

int
policy_cleanup(void)
{
	policy_destroy_list_(types_whitelist);
	types_whitelist = NULL;
	policy_destroy_list_(types_blacklist);
	types_blacklist = NULL;
	policy_destroy_list_(schemes_whitelist);
	schemes_whitelist = NULL;
	policy_destroy_list_(schemes_blacklist);
	schemes_blacklist = NULL;	
	return 0;
}

int
policy_init_context(CONTEXT *context)
{
	CRAWL *crawl;

	crawl = context->api->crawler(context);
	crawl_set_uri_policy(crawl, policy_uri_);
	crawl_set_checkpoint(crawl, policy_checkpoint_);
	return 0;
}

/* policy_uri_() is installed as the CRAWL object's uri_policy callback by
 * policy_init_context(), above.
 *
 * The uri_policy callback is invoked before a network fetch is attempted
 * by libcrawl. This function is also invoked directly by the libcrawld
 * queue logic to avoid adding URIs to the queue which do not meet the
 * policy.
 *
 * It should return a valid CRAWLSTATE value based upon its determination.
 * A state of COS_ACCEPTED will cause processing to proceed, any other value
 * will cause failure.
 *
 * As a special case, COS_NEW (not generally a valid return value) will
 * be coerced to COS_FAILED.
 */
CRAWLSTATE
policy_uri_(CRAWL *crawl, URI *uri, const char *uristr, void *userdata)
{
	char scheme[64];
	size_t c;
	int n;
	
	(void) userdata;
	(void) crawl;
	
	uri_scheme(uri, scheme, sizeof(scheme));
	if(schemes_whitelist && schemes_whitelist[0])
	{
		n = 0;
		for(c = 0; schemes_whitelist[c]; c++)
		{
			if(!strcasecmp(schemes_whitelist[c], scheme))
			{
				n = 1;
				break;
			}
		}
		if(!n)
		{
			log_printf(LOG_INFO, MSG_I_CRAWL_SCHEMENOTWHITE ": <%s> (%s)\n", uristr, scheme);
			return COS_SKIPPED;
		}
	}
	if(schemes_blacklist)
	{
		for(c = 0; schemes_blacklist[c]; c++)
		{
			if(!strcasecmp(schemes_blacklist[c], scheme))
			{
				log_printf(LOG_INFO, MSG_I_CRAWL_SCHEMEBLACK ": <%s> (%s)\n", uristr, scheme);
				return COS_REJECTED;
			}
		}
	}
	return COS_ACCEPTED;
}

static char **
policy_create_list_(const char *key, const char *defval)
{
	char **list;
	char *s, *t, *p;
	size_t count;
	int prevspace;
	
	if(!defval)
	{
		defval = "";
	}
	s = config_geta(key, defval);
	if(!s)
	{
		return NULL;
	}
	count = 0;
	prevspace = 1;
	for(t = s; *t; t++)
	{
		if(isspace(*t) || *t == ';' || *t == ',')
		{
			if(!prevspace)
			{
				count++;
			}
			prevspace = 1;
			continue;
		}
		prevspace = 0;
	}
	if(!prevspace)
	{
		count++;
	}
	list = (char **) crawl_alloc(NULL, (count + 1) * sizeof(char *));
	if(!list)
	{
		return NULL;
	}
	count = 0;
	p = NULL;
	for(t = s; *t; t++)
	{
		if(isspace(*t) || *t == ';' || *t == ',')
		{
			*t = 0;
			if(p)
			{
				list[count] = crawl_strdup(NULL, p);
				if(!list[count])
				{
					policy_destroy_list_(list);
					return NULL;
				}
				count++;
				p = NULL;
			}
			continue;
		}
		if(!p)
		{
			p = t;
		}
	}
	if(p)
	{
		list[count] = crawl_strdup(NULL, p);
		if(!list[count])
		{
			policy_destroy_list_(list);
			return NULL;
		}		
		count++;
	}
	list[count] = NULL;
	for(count = 0; list[count]; count++)
	{
		log_printf(LOG_DEBUG, "%s => '%s'\n", key, list[count]);
	}
	return list;
}

static int
policy_destroy_list_(char **list)
{
	size_t c;
	
	for(c = 0; list && list[c]; c++)
	{
		crawl_free(NULL, list[c]);
	}
	crawl_free(NULL, list);
	return 0;
}

/* policy_uri_() is installed as the CRAWL object's checkpoint callback by
 * policy_init_context(), above.
 *
 * The checkpoint callback is invoked during the fetch in order to confirm
 * that it should proceed before any (signifcant amounts of) data are
 * downloaded.
 *
 * It should return a valid CRAWLSTATE value based upon its determination.
 * A state of COS_ACCEPTED will cause processing to proceed, any other value
 * will cause failure.
 *
 * As a special case, COS_NEW (not generally a valid return value) will
 * be coerced to COS_FAILED.
 */
static CRAWLSTATE
policy_checkpoint_(CRAWL *crawl, CRAWLOBJ *obj, int *status, void *userdata)
{
	int n, c;
	const char *type;
	char *s, *t;
	
	(void) crawl;
	(void) userdata;
	
	if(*status >= 300 && *status < 400)
	{
		return COS_SKIPPED_COMMIT;
	}
	type = crawl_obj_type(obj);
	if(!type)
	{
		type = "";
	}
	s = crawl_strdup(crawl, type);
	if(!s)
	{
		return COS_ERR;
	}
	t = strchr(s, ';');
	if(t)
	{
		*t = 0;
		t--;
		while(t > s)
		{
			if(!isspace(*t))
			{
				break;
			}
			*t = 0;
			t--;
		}
	}
	log_printf(LOG_DEBUG, "Policy: content type is '%s', status is '%d'\n", s, *status);
	if(types_whitelist && types_whitelist[0])
	{
		n = 0;
		for(c = 0; types_whitelist[c]; c++)
		{
			if(!strcasecmp(types_whitelist[c], s))
			{
				n = c;
				break;
			}
		}
		if(!n)
		{
			log_printf(LOG_DEBUG, "Policy: type '%s' not matched by whitelist\n", s);
			crawl_free(crawl, s);
			*status = 406;
			return COS_SKIPPED;
		}
	}
	if(types_blacklist)
	{
		for(c = 0; types_blacklist[c]; c++)
		{
			if(!strcasecmp(types_blacklist[c], s))
			{
				log_printf(LOG_DEBUG, "Policy: type '%s' is blacklisted\n", s);
				crawl_free(crawl, s);
				*status = 406;
				return COS_REJECTED;
			}
		}
	}
	crawl_free(crawl, s);
	log_printf(LOG_DEBUG, "Policy: checkpoint complete\n");
	return COS_ACCEPTED;
}
