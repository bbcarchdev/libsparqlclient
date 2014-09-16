/* Twine: SPARQL updates
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

int
sparql_update(SPARQL *connection, const char *statement, size_t length)
{
	CURL *ch;
	char *buf;
	size_t buflen;

	ch = curl_easy_init();
	if(!ch)
	{
		sparql_logf_(connection, LOG_CRIT, "SPARQL: failed to initialise cURL handle\n");
		return -1;
	}
	buflen = sparql_urlencode_lsize_(statement, length);
	buf = (char *) malloc(16 + buflen);
	if(!buf)
	{
		sparql_logf_(connection, LOG_CRIT, "SPARQL: failed to allocate %u bytes\n", (unsigned) length + 16);
		curl_easy_cleanup(ch);
		return -1;
	}
	strcpy(buf, "update=");
	sparql_urlencode_l_(statement, length, buf + 7, buflen);
	sparql_logf_(connection, LOG_DEBUG, "SPARQL: %.*s\n", length, statement);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, connection->verbose);
	curl_easy_setopt(ch, CURLOPT_URL, connection->update_uri);	
	curl_easy_setopt(ch, CURLOPT_POST, 1);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDS, buf);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, strlen(buf));
	curl_easy_setopt(ch, CURLOPT_NOBODY, 1);
	curl_easy_perform(ch);
	curl_easy_cleanup(ch);
	free(buf);

	return 0;
}

int
sparql_vupdatef(SPARQL *connection, const char *format, va_list ap)
{
	char *buf;
	int r;

	if(sparql_vasprintf_(connection, &buf, format, ap) < 0)
	{
		return -1;
	}
	r = sparql_update(connection, buf, strlen(buf));
	free(buf);
	return r;
}

int
sparql_updatef(SPARQL *connection, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	return sparql_vupdatef(connection, format, ap);
}

int
sparql_insert(SPARQL *connection, const char *triples, size_t len, const char *graphuri)
{
	size_t l;
	char *buf, *bp;
	int r;
	
	if(!len)
	{
		return 0;
	}
	l = len + 20;
	if(graphuri)
	{
		l += strlen(graphuri) + 15;
	}
	buf = (char *) malloc(l);
	if(!buf)
	{
		sparql_logf_(connection, LOG_CRIT, "SPARQL: failed to allocate %u bytes for insert operation\n", (unsigned) (len + 20));
		return -1;
	}
	strcpy(buf, "INSERT DATA { ");
	bp = buf + 14;
	if(graphuri)
	{
		bp += sprintf(bp, "GRAPH <%s> { ", graphuri);
	}
	strncpy(bp, triples, len);
	bp += len;
	if(graphuri)
	{
		bp[0] = '}';
		bp++;
	}
	bp[0] = '}';
	bp[1] = 0;
	r = sparql_update(connection, buf, strlen(buf));
	free(buf);
	return r;
}

int
sparql_insert_stream(SPARQL *connection, librdf_stream *stream, const char *graphuri)
{
	librdf_world *world;
	librdf_serializer *serializer;
	char *buf;
	size_t buflen;
	int r;

	world = sparql_world(connection);
	if(!world)
	{
		return -1;
	}
	serializer = librdf_new_serializer(world, "ntriples", NULL, NULL);
	if(!serializer)
	{
		sparql_logf_(connection, LOG_ERR, "SPARQL: failed to create ntriples serializer\n");
		return -1;
	}
	buflen = 0;
	buf = (char *) librdf_serializer_serialize_stream_to_counted_string(serializer, NULL, stream, &buflen);
	if(!buf)
	{
		librdf_free_serializer(serializer);
		sparql_logf_(connection, LOG_ERR, "SPARQL: failed to serialise buffer\n");
		return -1;
	}
	r = sparql_insert(connection, buf, buflen, graphuri);
	librdf_free_memory(buf);
	librdf_free_serializer(serializer);
	return r;
}

int
sparql_insert_model(SPARQL *connection, librdf_model *model)
{
	librdf_iterator *iter;
	librdf_node *node;
	librdf_uri *uri;
	librdf_stream *stream;
	const char *uristr;
	int r;

	if(!librdf_model_size(model))
	{
		return 0;
	}
	if(!librdf_model_supports_contexts(model))
	{
		if(!(stream = librdf_model_as_stream(model)))
		{
			sparql_logf_(connection, LOG_CRIT, "SPARQL: failed to obtain stream for model\n");
			return -1;
		}
		r = sparql_insert_stream(connection, stream, NULL);
		librdf_free_stream(stream);
		return r;
	}
	iter = librdf_model_get_contexts(model);
	if(!iter)
	{
		sparql_logf_(connection, LOG_CRIT, "SPARQL: failed to obtain model context iterator\n");
		return -1;
	}	
	while(!librdf_iterator_end(iter))
	{
		node = librdf_iterator_get_object(iter);
		if(node &&
		   librdf_node_is_resource(node) &&
		   (uri = librdf_node_get_uri(node)) &&
		   (uristr = (const char *) librdf_uri_as_string(uri)))
		{
			if(!(stream = librdf_model_context_as_stream(model, node)))
			{
				sparql_logf_(connection, LOG_CRIT, "SPARQL: failed to obtain stream for context <%s>\n", uristr);
				librdf_free_iterator(iter);
				return -1;
			}
			r = sparql_insert_stream(connection, stream, uristr);
			librdf_free_stream(stream);
			if(r)
			{
				librdf_free_iterator(iter);
				return -1;
			}
		}
		librdf_iterator_next(iter);
	}
	librdf_free_iterator(iter);
	return 0;
}
