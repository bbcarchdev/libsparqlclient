/* SPARQL RDF Datastore - cURL wrappers
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2017 BBC
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

#include "p_libsparqlclient.h"

CURL *
sparql_curl_create_(SPARQL *connection, const char *url)
{
	CURL *ch;

	ch = curl_easy_init();
	if(!ch)
	{
		sparql_logf_(connection, LOG_ERR, "SPARQL: failed to create new cURL handle\n");
		return NULL;
	}
	curl_easy_setopt(ch, CURLOPT_VERBOSE, connection->verbose);
	curl_easy_setopt(ch, CURLOPT_FAILONERROR, 0);
	curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) &(connection->capture));
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, sparql_curl_dummy_write_);
	curl_easy_setopt(ch, CURLOPT_PRIVATE, (void *) connection);
	if(url)
	{
		curl_easy_setopt(ch, CURLOPT_URL, url);
	}
	free(connection->capture.buf);
	memset(&(connection->capture), 0, sizeof(struct sparql_capture_struct));
	return ch;
}

int
sparql_curl_perform_(CURL *ch)
{
	CURLcode e;
	SPARQL *connection;
	long status;
	struct timeval tv;
	unsigned long long start;
	int ms;

	gettimeofday(&tv, NULL);
	start = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);	
	e = curl_easy_perform(ch);
	gettimeofday(&tv, NULL);
	ms = (int) (((tv.tv_sec * 1000) + (tv.tv_usec / 1000)) - start);
	connection = NULL;
	curl_easy_getinfo(ch, CURLINFO_PRIVATE, (char **) (&connection));
	curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
	if(status > 299)
	{
		if(connection)
		{
			sparql_set_nerror_(connection, status, connection->capture.buf);
		}
		return -1;
	}
	if(e == CURLE_OK)
	{
		sparql_logf_(connection, LOG_DEBUG, "SPARQL: query completed in %dms\n", ms);
		if(connection)
		{
			sparql_set_nerror_(connection, 0, NULL);
		}
		return 0;
	}
	if(connection)
	{
		sparql_set_nerror_(connection, 1000, curl_easy_strerror(e));
		sparql_logf_(connection, LOG_ERR, "SPARQL: cURL request failed: %s\n", curl_easy_strerror(e));
	}
	return -1;
}

size_t
sparql_curl_dummy_write_(char *ptr, size_t size, size_t nemb, void *userdata)
{
	struct sparql_capture_struct *data;
	char *p;

	data = (struct sparql_capture_struct *) userdata;

	size *= nemb;
	if(data->pos + size >= data->size)
	{
		if(data->size > 16384)
		{
			/* Swallow any remaining incoming data */
			return size;
		}
		p = (char *) realloc(data->buf, data->size + size + 1);
		if(!p)
		{
			return 0;
		}
		data->buf = p;
		data->size += size;
	}
	memcpy(&(data->buf[data->pos]), ptr, size);
	data->pos += size;
	data->buf[data->pos] = 0;
	return size;
}	
