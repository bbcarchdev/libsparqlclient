/* SPARQL RDF Datastore - POST interface
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

/* Perform a SPARQL POST to the RESTful endpoint */
int
sparql_post(SPARQL *connection, const char *graph, const char *triples, size_t length)
{
	CURL *ch;
	char *buf, *t;
	size_t buflen;
	int r;

	if(!connection->data_uri)
	{
		sparql_set_error_(connection, SPARQLSTATE_NO_DATASTORE, "cannot POST to a server without a RESTful data endpoint");
		return -1;
	}
	buflen = sparql_urlencode_size_(graph) + sparql_urlencode_lsize_(triples, length) + 64;
	t = buf = (char *) malloc(buflen);
	strcpy(t, "mime-type=application/x-turtle&graph=");
	t = strchr(buf, 0);
	sparql_urlencode_(graph, t, buflen - (t - buf));
	t = strchr(buf, 0);
	strcpy(t, "&data=");
	t = strchr(buf, 0);
	sparql_urlencode_l_(triples, length, t, buflen - (t - buf));
	sparql_logf_(connection, LOG_DEBUG, "SPARQL: performing POST to %s for %s\n", connection->data_uri, graph);
	sparql_logf_(connection, LOG_DEBUG, "SPARQL: %s\n", buf);
	r = -1;
	ch = sparql_curl_create_(connection, connection->data_uri);
	if(!ch)
	{
		return -1;
	}
	curl_easy_setopt(ch, CURLOPT_POST, 1);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDS, buf);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, buflen);
	r = sparql_curl_perform_(ch);
	free(buf);
	curl_easy_cleanup(ch);
	return r;
}
