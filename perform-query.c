/*
 * SPARQL client: perform a query with a SAX-like callback interface
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

struct sparql_query_struct
{
	SPARQL *connection;
	int result;
	void *data;
	CURL *ch;
	xmlParserCtxtPtr ctx;
	xmlDocPtr doc;
	xmlSAXHandler sax;
	char *buf;
	char *name;
	char *datatype;
	char *language;
	size_t buflen;
	size_t bufsize;
	int bound;
	SPARQLSTATE state;
	int (*variable)(SPARQLQUERY *query, const char *name, void *data);
	int (*link)(SPARQLQUERY *query, const char *href, void *data);
	int (*beginresults)(SPARQLQUERY *query, void *data);
	int (*endresults)(SPARQLQUERY *query, void *data);
	int (*beginresult)(SPARQLQUERY *query, void *data);
	int (*endresult)(SPARQLQUERY *query, void *data);
	int (*literal)(SPARQLQUERY *query, const char *name, const char *language, const char *datatype, const char *text, void *data);
	int (*uri)(SPARQLQUERY *query, const char *name, const char *uri, void *data);
	int (*bnode)(SPARQLQUERY *query, const char *name, const char *ref, void *data);
	int (*boolean)(SPARQLQUERY *query, int value, void *data);
	int (*complete)(SPARQLQUERY *query, void *data);
	int (*error)(SPARQLQUERY *query, void *data);
};

static size_t sparql_query_write_(char *ptr, size_t size, size_t nemb, void *userdata);
static void sparql_query_sax_startel_(void *ctx, const xmlChar *localname, const xmlChar *prefix, const xmlChar *URI, int nb_namespaces, const xmlChar **namespaces, int nb_attributes, int nb_defaulted, const xmlChar **attributes);
static void sparql_query_sax_endel_(void *ctx, const xmlChar *localname, const xmlChar *prefix, const xmlChar *URI);
static void sparql_query_sax_characters_(void *ctx, const xmlChar *ch, int len);

SPARQLQUERY *
sparql_query_create_(SPARQL *connection)
{
	SPARQLQUERY *p;

	p = (SPARQLQUERY *) calloc(1, sizeof(SPARQLQUERY));
	if(!p)
	{
		return NULL;
	}	
	p->connection = connection;
	p->ch = curl_easy_init();
	if(!p->ch)
	{
		sparql_logf_(connection, LOG_CRIT, "failed to initialise cURL handle\n");
		free(p);
		return NULL;
	}
	p->sax.initialized = XML_SAX2_MAGIC;
	p->sax.startElementNs = sparql_query_sax_startel_;
	p->sax.endElementNs = sparql_query_sax_endel_;
	p->sax.characters = sparql_query_sax_characters_;
	p->ctx = xmlCreatePushParserCtxt(&(p->sax), (void *) p, "", 0, NULL);
	if(!p->ctx)
	{
		sparql_logf_(p->connection, LOG_CRIT, "failed to create XML parsing context\n");
		curl_easy_cleanup(p->ch);
		return NULL;
	}
	xmlCtxtUseOptions(p->ctx, XML_PARSE_NODICT | XML_PARSE_NOENT);
	return p;
}

int
sparql_query_destroy_(SPARQLQUERY *query)
{
	free(query->name);
	free(query->language);
	free(query->datatype);
	free(query->buf);
	curl_easy_cleanup(query->ch);
	if(query->doc)
	{
		xmlFreeDoc(query->doc);
	}
	if(query->ctx)
	{
		xmlFreeParserCtxt(query->ctx);
	}
	free(query);
	return 0;
}

int
sparql_query_set_data_(SPARQLQUERY *query, void *data)
{
	query->data = data;
	return 0;
}

int
sparql_query_set_variable_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, const char *name, void *data))
{
	query->variable = callback;
	return 0;
}

int
sparql_query_set_link_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, const char *href, void *data))
{
	query->link = callback;
	return 0;
}   

int
sparql_query_set_beginresults_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, void *data))
{
	query->beginresults = callback;
	return 0;
}

int
sparql_query_set_endresults_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, void *data))
{
	query->endresults = callback;
	return 0;
}


int
sparql_query_set_beginresult_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, void *data))
{
	query->beginresult = callback;
	return 0;
}

int
sparql_query_set_endresult_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, void *data))
{
	query->endresult = callback;
	return 0;
}

int
sparql_query_set_literal_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, const char *name, const char *language, const char *datatype, const char *text, void *data))
{
	query->literal = callback;
	return 0;
}

int
sparql_query_set_uri_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, const char *name, const char *uri, void *data))
{
	query->uri = callback;
	return 0;
}

int
sparql_query_set_bnode_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, const char *name, const char *ref, void *data))
{
	query->bnode = callback;
	return 0;
}

int
sparql_query_set_boolean_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, int value, void *data))
{
	query->boolean = callback;
	return 0;
}

int
sparql_query_set_complete_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, void *data))
{
	query->complete = callback;
	return 0;
}

int
sparql_query_set_error_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, void *data))
{
	query->error = callback;
	return 0;
}

int
sparql_query_perform_(SPARQLQUERY *query, const char *statement, size_t length)
{
	char *buf, *t;
	size_t buflen;
	struct curl_slist *headers;
	
	buflen = sparql_urlencode_lsize_(statement, length);
	buf = (char *) malloc(strlen(query->connection->query_uri) + buflen + 16);
	sprintf(buf, "%s?query=", query->connection->query_uri);
	t = strchr(buf, 0);
	sparql_urlencode_l_(statement, length, t, buflen);
	sparql_logf_(query->connection, LOG_DEBUG, "performing GET from %s\n", buf);
	curl_easy_setopt(query->ch, CURLOPT_VERBOSE, query->connection->verbose);
	curl_easy_setopt(query->ch, CURLOPT_URL, buf);
	curl_easy_setopt(query->ch, CURLOPT_WRITEDATA, (void *) query);
	curl_easy_setopt(query->ch, CURLOPT_WRITEFUNCTION, sparql_query_write_);
	headers = curl_slist_append(NULL, "Accept: application/sparql-results+xml, text/turtle, application/ntriples");
	curl_easy_setopt(query->ch, CURLOPT_HTTPHEADER, headers);
	query->result = 0;
	query->state = SQS_ROOT;
	curl_easy_perform(query->ch);
	curl_slist_free_all(headers);
	if(sparql_query_write_((char *) "", 0, 0, (void *) query))
	{
		query->result = -1;
	}
	if(query->result)
	{
		if(query->error)
		{
			query->error(query, query->data);
		}
		return -1;
	}
	if(query->complete)
	{
		query->complete(query, query->data);
	}
	return 0;
}

static size_t
sparql_query_write_(char *ptr, size_t size, size_t nemb, void *userdata)
{
	SPARQLQUERY *query = (SPARQLQUERY *) userdata;
	
	if(query->result || query->state == SQS_ERROR)
	{
		return 0;
	}
	if(!size)
	{
		/* End of data */
		xmlParseChunk(query->ctx, ptr, 0, 1);
		if(!query->ctx->wellFormed)
		{
			sparql_logf_(query->connection,  LOG_ERR, "returned XML document was not well-formed\n");
			query->result = -1;
			return -1;
		}
		return 0;
	}
	xmlParseChunk(query->ctx, ptr, nemb * size, 0);
	return nemb * size;
}

static void
sparql_query_sax_startel_(void *ctx, const xmlChar *localname, const xmlChar *prefix, const xmlChar *URI, int nb_namespaces, const xmlChar **namespaces, int nb_attributes, int nb_defaulted, const xmlChar **attributes)
{
	SPARQLQUERY *query = (SPARQLQUERY *) ctx;
	size_t i, l;
	int c;
	char *p;

	(void) prefix;
	(void) nb_namespaces;
	(void) namespaces;
	(void) nb_defaulted;
	
	if(query->result)
	{
		return;
	}
	if(strcmp((const char *) URI, "http://www.w3.org/2005/sparql-results#"))
	{
		sparql_logf_(query->connection, LOG_ERR, "unexpected namespace <%s> in SPARQL results\n", URI);
		query->result = -1;
		return;
	}
	switch(query->state)
	{
	case SQS_ERROR:
		query->result = -1;
		return;
	case SQS_ROOT:
		if(!strcmp((const char *) localname, "sparql"))
		{
			query->state = SQS_SPARQL;
			return;
		}
		sparql_logf_(query->connection, LOG_ERR, "expected: <sparql>, found: <%s>\n", localname);
		query->result = -1;
		break;
	case SQS_SPARQL:
		if(!strcmp((const char *) localname, "head"))
		{
			query->state = SQS_HEAD;
		}
		else if(!strcmp((const char *) localname, "results"))
		{
			query->state = SQS_RESULTS;
			if(query->beginresults)
			{
				if(query->beginresults(query, query->data))
				{
					sparql_logf_(query->connection, LOG_DEBUG, "beginresults callback failed\n");
					query->result = -1;
					return;
				}
			}
		}
		else if(!strcmp((const char *) localname, "boolean"))
		{
			query->state = SQS_BOOLEAN;
		}
		else
		{
			sparql_logf_(query->connection, LOG_ERR, "expected: <head>, <results>, or <boolean>; found: <%s>\n", localname);
			query->result = -1;
			return;
		}
		break;
	case SQS_HEAD:
		if(!strcmp((const char *) localname, "link"))
		{
			query->state = SQS_LINK;
			p = NULL;
			for(c = i = 0; c < nb_attributes; c++)
			{
				if((!attributes[i+2] || !attributes[i+2][0]) &&
				   !strcmp((const char *) attributes[i], "href"))
				{
					l = attributes[i+4] - attributes[i+3];
					p = (char *) malloc(l + 1);
					if(!p)
					{
						sparql_logf_(query->connection, LOG_CRIT, "failed to allocate %u bytes for link URI\n", (unsigned) l + 1);
						query->result = -1;
						return;
					}
					memcpy(p, attributes[i+3], l);
					p[l] = 0;
				}
				i += 5;
			}
			if(p)
			{
				if(query->link)
				{
					if(query->link(query, p, query->data))
					{
						sparql_logf_(query->connection, LOG_DEBUG, "link callback failed\n");
						query->result = -1;
						free(p);
						return;
					}
				}
				free(p);
			}
			else
			{
				sparql_logf_(query->connection, LOG_WARNING, "warning: ignoring <link> with no href attribute\n");
			}
		}
		else if(!strcmp((const char *) localname, "variable"))
		{
			query->state = SQS_VARIABLE;
			p = NULL;
			for(i = c = 0; c < nb_attributes; c++)
			{
				if((!attributes[i+2] || !attributes[i+2][0]) &&
				   !strcmp((const char *) attributes[i], "name"))
				{
					l = attributes[i+4] - attributes[i+3];
					p = (char *) malloc(l + 1);
					if(!p)
					{
						sparql_logf_(query->connection, LOG_CRIT, "failed to allocate %u bytes for variable name\n", (unsigned) l + 1);
						query->result = -1;
						return;
					}
					memcpy(p, attributes[i+3], l);
					p[l] = 0;
				}
				i += 5;
			}
			if(p)
			{
				if(query->variable)
				{
					if(query->variable(query, p, query->data))
					{
						sparql_logf_(query->connection, LOG_DEBUG, "variable callback failed\n");
						query->result = -1;
						free(p);
						return;
					}
				}
				free(p);
			}
			else
			{
				sparql_logf_(query->connection, LOG_WARNING, "warning: ignoring <variable> with no name attribute\n");
			}
		}
		else
		{
			sparql_logf_(query->connection, LOG_ERR, "expected: <variable> or <link>; found: <%s>\n", localname);
			query->result = -1;
		}
		break;
	case SQS_VARIABLE:
		sparql_logf_(query->connection, LOG_ERR, "unexpected child of <variable> found (<%s>)\n", localname);
		query->result = -1;
		break;
	case SQS_RESULTS:
		if(!strcmp((const char *) localname, "result"))
		{
			query->state = SQS_RESULT;
			if(query->beginresult)
			{
				if(query->beginresult(query, query->data))
				{
					sparql_logf_(query->connection, LOG_DEBUG, "beginresult callback failed\n");
					query->result = -1;
					return;
				}
			}
		}
		else
		{
			sparql_logf_(query->connection, LOG_ERR, "expected: <result>, found: <%s>\n", localname);
			query->result = -1;
		}
		break;
	case SQS_RESULT:
		if(!strcmp((const char *) localname, "binding"))
		{
			query->state = SQS_BINDING;
			query->bound = 0;
			for(i = c = 0; c < nb_attributes; c++)
			{
				if((!attributes[i+2] || !attributes[i+2][0]) &&
				   !strcmp((const char *) attributes[i], "name"))
				{
					l = attributes[i+4] - attributes[i+3];
					query->name = (char *) malloc(l + 1);
					if(!query->name)
					{
						sparql_logf_(query->connection, LOG_CRIT, "failed to allocate memory for binding name\n");
						query->result = -1;
						return;
					}
					memcpy(query->name, attributes[i+3], l);
					query->name[l] = 0;
					break;
				}
				i += 5;
			}
			if(!query->name)
			{
				sparql_logf_(query->connection, LOG_ERR, "<binding> does not have a name\n");
				query->result = -1;
			}			
		}
		else
		{
			sparql_logf_(query->connection, LOG_ERR, "expected: <binding>, found: <%s>\n", localname);
			query->result = -1;
		}
		break;
	case SQS_BINDING:
		if(query->bound)
		{
			sparql_logf_(query->connection, LOG_ERR, "multiple values provided for binding to '%s'\n", query->name);
			query->result = -1;
			return;
		}
		if(!strcmp((const char *) localname, "uri"))
		{
			query->state = SQS_URI;
		}
		else if(!strcmp((const char *) localname, "literal"))
		{
			query->state = SQS_LITERAL;
			for(i = c = 0; c < nb_attributes; c++)
			{
				if((!attributes[i+2] || !attributes[i+2][0]) &&
				   !strcmp((const char *) attributes[i], "datatype"))
				{
					l = attributes[i+4] - attributes[i+3];
					query->datatype = (char *) malloc(l + 1);
					if(!query->datatype)
					{
						sparql_logf_(query->connection, LOG_CRIT, "failed to allocate %u bytes for literal datatype URI\n", (unsigned) l + 1);
						query->result = -1;
						return;
					}
					memcpy(query->datatype, attributes[i+3], l);
					query->datatype[l] = 0;
				}
				else if(attributes[i+2] &&
						!strcmp((const char *) attributes[i+2], "http://www.w3.org/XML/1998/namespace") &&
						!strcmp((const char *) attributes[i], "lang"))
				{					
					l = attributes[i+4] - attributes[i+3];
					query->language = (char *) malloc(l + 1);
					if(!query->language)
					{
						sparql_logf_(query->connection, LOG_CRIT, "failed to allocate %u bytes for literal language tag\n", (unsigned) l + 1);
						query->result = -1;
						return;
					}
					memcpy(query->language, attributes[i+3], l);
					query->language[l] = 0;
				}
				i += 5;
			}
		}
		else if(!strcmp((const char *) localname, "bnode"))
		{
			query->state = SQS_BNODE;
		}
		else
		{
			sparql_logf_(query->connection, LOG_ERR, "expected: <uri>, <literal>, or <bnode>, found: <%s>\n", localname);
			query->result = -1;
		}
		break;
	case SQS_URI:
	case SQS_LITERAL:
	case SQS_BNODE:
	case SQS_BOOLEAN:
	case SQS_LINK:
		sparql_logf_(query->connection, LOG_ERR, "unexpected value child found, <%s>\n", localname);
		query->result = -1;
		break;
	}
}

static void
sparql_query_sax_endel_(void *ctx, const xmlChar *localname, const xmlChar *prefix, const xmlChar *URI)
{
	SPARQLQUERY *query = (SPARQLQUERY *) ctx;
	int v;

	(void) localname;
	(void) prefix;
	(void) URI;

	if(query->result)
	{
		return;
	}	
	switch(query->state)
	{
	case SQS_ROOT:
		break;
	case SQS_ERROR:
		query->result = -1;
		break;
	case SQS_SPARQL:
		query->state = SQS_ROOT;
		break;
	case SQS_HEAD:
		query->state = SQS_SPARQL;
		break;
	case SQS_LINK:
		query->state = SQS_HEAD;
		break;
	case SQS_VARIABLE:
		query->state = SQS_HEAD;
		break;
	case SQS_RESULTS:
		query->state = SQS_SPARQL;
		if(query->endresults)
		{
			if(query->endresults(query, query->data))
			{
				sparql_logf_(query->connection, LOG_DEBUG, "endresults callback failed\n");
				query->result = -1;
				return;
			}
		}
		break;
	case SQS_RESULT:
		query->state = SQS_RESULTS;
		if(query->endresult)
		{
			if(query->endresult(query, query->data))
			{
				sparql_logf_(query->connection, LOG_DEBUG, "endresult callback failed\n");
				query->result = -1;
				return;
			}
		}
		break;
	case SQS_BINDING:
		query->state = SQS_RESULT;
		free(query->name);
		query->name = NULL;
		free(query->language);
		query->language = NULL;
		free(query->datatype);
		query->datatype = NULL;
		free(query->buf);
		query->buf = NULL;
		query->buflen = query->bufsize = 0;
		break;		
	case SQS_URI:
		if(query->uri)
		{
			if(query->uri(query, query->name, query->buf, query->data))
			{
				query->result = -1;
				break;
			}
		}
		query->state = SQS_BINDING;
		break;
	case SQS_BNODE:
		if(query->bnode)
		{
			if(query->bnode(query, query->name, query->buf, query->data))
			{
				query->result = -1;
				break;
			}
		}
		query->state = SQS_BINDING;
		break;
	case SQS_LITERAL:
		if(query->literal)
		{
			if(query->literal(query, query->name, query->language, query->datatype, query->buf, query->data))
			{
				query->result = -1;
				break;
			}
		}
		query->state = SQS_BINDING;
		break;
	case SQS_BOOLEAN:
		v = -1;
		if(query->buf)
		{
			if(!strcmp(query->buf, "true"))
			{
				v = 1;
			}
			else if(!strcmp(query->buf, "false"))
			{
				v = 0;
			}
		}
		if(v == -1)
		{
			sparql_logf_(query->connection, LOG_ERR, "expected 'true' or 'false' within <boolean>\n");
			query->result = -1;
			return;
		}
		if(query->boolean)
		{
			if(query->boolean(query, v, query->data))
			{
				query->result = -1;
			}
		}
		break;
	}
}
		

static void 
sparql_query_sax_characters_(void *ctx, const xmlChar *ch, int len)
{
	SPARQLQUERY *query = (SPARQLQUERY *) ctx;
	char *p;
	size_t l;

	if(query->result)
	{
		return;
	}
	switch(query->state)
	{
	case SQS_BOOLEAN:
	case SQS_LITERAL:
	case SQS_URI:
	case SQS_BNODE:
		l = query->buflen + len + 1;
		if(l > query->bufsize)
		{
			l = (((l - 1) / 128) + 1) * 128;
			p = (char *) realloc(query->buf, l);
			if(!p)
			{
				sparql_logf_(query->connection, LOG_CRIT, "failed to reallocate buffer to %u bytes\n", (unsigned) l);
				query->result = -1;
				return;
			}
			query->buf = p;
			query->bufsize = l;
		}
		memcpy(&(query->buf[query->buflen]), ch, len);
		query->buflen += len;
		query->buf[query->buflen] = 0;
		break;
	default:
		for(; len; len--)
		{
			if(!isspace(*ch))
			{
				sparql_logf_(query->connection, LOG_WARNING, "warning: ignored unexpected literal in parser state %d\n", (int) query->state);
				break;
			}
			ch++;
		}
		break;
	}
}
