/* Crawl: Plug-in handling
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2016 BBC
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

#include "p_libcrawl.h"
# include "libsupport.h"

#define PLUGINDIR                       LIBDIR "/" PACKAGE_TARNAME "/"


/* Load a plug-in and invoke its initialiser callback */
int
crawl_plugin_load_cb(const char *plugin, const char *pathname, void *context)
{
	void *handle;
	CRAWLPLUGINENTRYFN entry;
	char *fnbuf;
	size_t len;
	int r;

	log_printf(LOG_DEBUG, "loading plug-in %s\n", pathname);
	if(strchr(pathname, '/'))
	{
		fnbuf = NULL;
	}
	else
	{
		len = strlen(pathname) + strlen(PLUGINDIR) + 1;
		fnbuf = (char *) malloc(len);
		if(!fnbuf)
		{
			log_printf(LOG_CRIT, "failed to allocate %u bytes\n", (unsigned) len);
			return -1;
		}
		strcpy(fnbuf, PLUGINDIR);
		strcat(fnbuf, pathname);
		pathname = fnbuf;
	}
	handle = dlopen(pathname, RTLD_NOW);
	if(!handle)
	{
		log_printf(LOG_ERR, "failed to load %s: %s\n", pathname, dlerror());
		free(fnbuf);
		return -1;
	}
	entry = (CRAWLPLUGINENTRYFN) dlsym(handle, "anansi_entry");
	r = entry(context, CRAWL_ATTACHED, handle);
	if(r)
	{
		log_printf(LOG_ERR, "initialisation of plug-in %s failed\n", pathname);
		return -1;
	}
	log_printf(LOG_DEBUG, "loaded plug-in %s\n", pathname);
	free(fnbuf);
	return 0;
}

/* Public: Register a signal handler */
int
crawl_plugin_attach_signal(CRAWL *context, CRAWLSIGNALNAME signal, const char *description, CRAWLSIGNALCBFN fn, void *userdata)
{
	// Create the structure
	struct plugin_callback_struct p;

	// Copy the description
	p.desc = strdup(description);
	if(!p.desc)
	{
		log_printf(LOG_ERR, "failed to allocate memory\n");
		return -1;
	}

	// Set the function, the signal name and the user data
	p.fn = fn;
	p.signal = signal;
	p.data = userdata;

	// Adjust the size of the array of callbacks
	struct plugin_callback_struct *callbacks;
	callbacks = (struct plugin_callback_struct *) realloc(context->callbacks, sizeof(struct plugin_callback_struct) * (context->cbcount + 1));
	if(!callbacks)
	{
		log_printf(LOG_ERR, "failed to allocate memory\n");
		return NULL;
	}
	context->callbacks = callbacks;
	context->callbacks[context->cbcount] = p;
	context->cbcount++;

	log_printf(LOG_INFO, "attached signal %d to %p from callback '%s'\n", p.signal, p.fn, p.desc);

	return 0;
}

int crawl_plugin_signal(CRAWL *crawl, CRAWLSIGNALNAME signal, const char *uristr)
{
	struct plugin_callback_struct *p;

	log_printf(LOG_DEBUG, "Sending signal %d to %d callbacks\n", signal, crawl->cbcount);

	for (size_t i=0; i < crawl->cbcount; i++)
	{
		p = &(crawl->callbacks[i]);
		log_printf(LOG_INFO, "Checking %d -> %d\n", i, p->signal);
		if (p->signal == signal)
		{
			log_printf(LOG_INFO, "Call back %d matching signal at %p\n", i, p->fn);
			p->fn(crawl, uristr, p->data);
		}
	}

	return 0;
}


