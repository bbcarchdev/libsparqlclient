/* SPARQL RDF Datastore - PUT interface
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2017 BBC
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

/* Perform a SPARQL PUT to the RESTful endpoint */
int
sparql_put(SPARQL *connection, const char *graph, const char *triples, size_t length)
{
	CURL *ch;
	struct curl_slist *headers;
	char *buf, *t;
	size_t buflen;
	int r;

	if(!connection->data_uri)
	{
		/* TODO: perform a normal graph replace (DROP/CLEAR then INSERT) */
		sparql_set_error_(connection, SPARQLSTATE_NO_DATASTORE, "cannot PUT to a server without a RESTful data endpoint");
		return -1;
	}
	buflen = sparql_urlencode_size_(graph);
	buf = (char *) malloc(strlen(connection->data_uri) + buflen + 16);
	sprintf(buf, "%s?graph=", connection->data_uri);
	t = strchr(buf, 0);
	sparql_urlencode_(graph, t, buflen);
	sparql_logf_(connection, LOG_DEBUG, "SPARQL: performing PUT to %s\n", buf);
	r = -1;
	ch = sparql_curl_create_(connection, buf);
	if(!ch)
	{
		return -1;
	}
	curl_easy_setopt(ch, CURLOPT_POST, 1);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDS, triples);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, length);
	curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, "PUT");
	headers = curl_slist_append(NULL, "Content-type: text/turtle; charset=utf-8");
	curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
	r = sparql_curl_perform_(ch);
	free(buf);
	curl_slist_free_all(headers);
	curl_easy_cleanup(ch);
	return r;
}
