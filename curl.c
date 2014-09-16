/* SPARQL RDF Datastore - PUT interface
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014 BBC
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

static size_t sparql_curl_dummy_write_(char *ptr, size_t size, size_t nemb, void *userdata);

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
	curl_easy_setopt(ch, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, sparql_curl_dummy_write_);
	curl_easy_setopt(ch, CURLOPT_PRIVATE, (void *) connection);
	if(url)
	{
		curl_easy_setopt(ch, CURLOPT_URL, url);
	}
	return ch;
}

int
sparql_curl_perform_(CURL *ch)
{
	CURLcode e;
	SPARQL *connection;

	e = curl_easy_perform(ch);
	if(e == CURLE_OK)
	{
		return 0;
	}
	connection = NULL;
	curl_easy_getinfo(ch, CURLINFO_PRIVATE, (char **) (&connection));
	if(connection)
	{
		sparql_logf_(connection, LOG_ERR, "SPARQL: cURL request failed: %s\n", curl_easy_strerror(e));
	}
	return -1;
}

static size_t
sparql_curl_dummy_write_(char *ptr, size_t size, size_t nemb, void *userdata)
{
	(void) ptr;
	(void) userdata;

	return nemb * size;
}	
