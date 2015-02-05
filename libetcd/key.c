/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2015 BBC
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

#include "p_libetcd.h"

#define XDIGIT(c) ((c < 10) ? '0' + c : 'a' + (c - 10))

int
etcd_key_set(ETCD *etcd, ETCDDIR *dir, const char *name, const char *value)
{
	return etcd_key_set_data_ttl(etcd, dir, name, (const unsigned char *) value, strlen(value), 0);
}


int
etcd_key_set_ttl(ETCD *etcd, ETCDDIR *dir, const char *name, const char *value, int ttl)
{
	return etcd_key_set_data_ttl(etcd, dir, name, (const unsigned char *) value, strlen(value), ttl);
}

int
etcd_key_set_data_ttl(ETCD *etcd, ETCDDIR *dir, const char *name, const unsigned char *data, size_t len, int ttl)
{
	URI *uri;
	CURL *ch;
	char *encoded, *p;
	const unsigned char *dp;
	size_t enclen, l;
	int status, c;

	while(*name == '/')
	{
		name++;
	}
	enclen = 6 + len * 3;
	if(ttl)
	{
		enclen += 32;
	}
	enclen++;
	encoded = (char *) calloc(1, enclen);
	if(!encoded)
	{
		return -1;
	}
	strcpy(encoded, "value=");
	p = encoded + 6;
	l = 0;
	for(dp = data; l < len; dp++, l++)
	{
		c = *dp;
		if(isprint(c) && c != '&' && c != '=')
		{
			*p = (char) c;
			p++;
			continue;
		}
		*p = '%';
		p++;
		*p = XDIGIT((c >> 4));
		p++;
		*p = XDIGIT((c & 0x0f));
		p++;
	}
	if(ttl)
	{
		sprintf(p, "&ttl=%d", ttl);
	}
	else
	{
		*p = 0;
	}
	uri = uri_create_str(name, dir ? dir->base : etcd->uri);
	if(!uri)
	{
		free(encoded);
		return -1;
	}
	ch = etcd_curl_put_(etcd, uri, encoded);
	status = etcd_curl_perform_(ch);
	etcd_curl_done_(ch);

	free(encoded);
	uri_destroy(uri);
	return status;
}

