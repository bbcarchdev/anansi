/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2017 BBC
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

#ifndef P_PROCESSORS_H_
# define P_PROCESSORS_H_               1

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <errno.h>
# include <ctype.h>
# include <unistd.h>
# include <syslog.h>

# define PROCESSOR_STRUCT_DEFINED      1

# include "libsupport.h"
# include "libspider.h"
# include <curl/curl.h>
# include <librdf.h>

/* Log messages */

/* RDF (3000-3099) */

# define MSG_I_RDF_SKIPPED_REDIRECT     "%%ANANSI-I-3000: SKIPPED (redirect)"
# define MSG_I_RDF_FAILED_HTTP          "%%ANANSI-I-3001: FAILED (error response)"
# define MSG_I_RDF_FAILED_NOTYPE        "%%ANANSI-I-3002: FAILED (no Content-Type in response)"
# define MSG_I_RDF_REJECTED_TYPE        "%%ANANSI-I-3003: REJECTED (unsupported content type)"
# define MSG_E_RDF_ERROR_PAYLOAD        "%%ANANSI-E-3004: ERROR (cannot open payload for processing)"
# define MSG_I_RDF_FAILED_PARSE         "%%ANANSI-I-3005: FAILED (unable to parse payload)"
# define MSG_I_RDF_MALFORMEDLINK        "%%ANANSI-I-3006: ignoring malford Link header in response"
# define MSG_I_RDF_MALFORMEDLINKPARAM   "%%ANANSI-I-3007: ignoring Link header in response with malformed parameters"

/* LOD (3100-3199) */

# define MSG_I_LOD_REJECTED             "%%ANANSI-I-3100: REJECTED"


typedef CRAWLSTATE (*rdf_filter_cb)(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, librdf_model *model);

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
	char **license_predicates;
	char **license_whitelist;
	char **license_blacklist;
};

extern PROCESSOR *spider_processor_rdf_create_(SPIDER *spider);
extern PROCESSOR *spider_processor_lod_create_(SPIDER *spider);

extern librdf_world *rdf_world(PROCESSOR *me);
extern int rdf_set_filter(PROCESSOR *me, rdf_filter_cb filter);

#endif /*!P_PROCESSORS_H_*/
