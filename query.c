/*
 * SPARQL client: query interface
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

struct sparql_query_context_struct
{
	SPARQL *connection;
	SPARQLQUERY *query;
	SPARQLRES *results;
	SPARQLROW *row;
	int has_results;
	int has_boolean;
};

static int sparql_query_variable_(SPARQLQUERY *query, const char *name, void *data);
static int sparql_query_link_(SPARQLQUERY *query, const char *href, void *data);
static int sparql_query_beginresults_(SPARQLQUERY *query, void *data);
static int sparql_query_beginresult_(SPARQLQUERY *query, void *data);
static int sparql_query_endresult_(SPARQLQUERY *query, void *data);
static int sparql_query_literal_(SPARQLQUERY *query, const char *name, const char *language, const char *datatype, const char *text, void *data);
static int sparql_query_uri_(SPARQLQUERY *query, const char *name, const char *uri, void *data);
static int sparql_query_bnode_(SPARQLQUERY *query, const char *name, const char *ref, void *data);
static int sparql_query_boolean_(SPARQLQUERY *query, int value, void *data);

/* Perform a query, returning a result-set */
SPARQLRES *
sparql_query(SPARQL *connection, const char *querybuf, size_t length)
{
	struct sparql_query_context_struct context;
	
	memset(&context, 0, sizeof(context));
	context.connection = connection;
	context.query = sparql_query_create_(connection);
	if(!context.query)
	{
		sparql_logf_(connection, LOG_CRIT, "failed to create SPARQL query structure\n");
		return NULL;
	}
	context.results = sparqlres_create_(connection);
	if(!context.results)
	{
		sparql_logf_(connection, LOG_CRIT, "failed to create SPARQL result-set structure\n");
		sparql_query_destroy_(context.query);
		return NULL;
	}
	sparql_query_set_data_(context.query, (void *) &context);
	sparql_query_set_variable_(context.query, sparql_query_variable_);
	sparql_query_set_link_(context.query, sparql_query_link_);
	sparql_query_set_beginresults_(context.query, sparql_query_beginresults_);
	sparql_query_set_beginresult_(context.query, sparql_query_beginresult_);
	sparql_query_set_endresult_(context.query, sparql_query_endresult_);
	sparql_query_set_literal_(context.query, sparql_query_literal_);
	sparql_query_set_bnode_(context.query, sparql_query_bnode_);
	sparql_query_set_uri_(context.query, sparql_query_uri_);
	sparql_query_set_boolean_(context.query, sparql_query_boolean_);
	if(sparql_query_perform_(context.query, querybuf, length))
	{
		sparqlres_destroy(context.results);
		sparql_query_destroy_(context.query);
		return NULL;
	}
	sparql_query_destroy_(context.query);
	return context.results;
}

static int 
sparql_query_variable_(SPARQLQUERY *query, const char *name, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;

	if(sparqlres_add_variable_(context->results, name))
	{
		sparql_logf_(context->connection, LOG_ERR, "failed to add variable '%s' to result-set\n");
		return -1;
	}
	return 0;
}

static int 
sparql_query_link_(SPARQLQUERY *query, const char *href, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;

	if(sparqlres_add_link_(context->results, href))
	{
		sparql_logf_(context->connection, LOG_ERR, "failed to add link <%s> to result-set\n", href);
		return -1;
	}
	return 0;
}

static int 
sparql_query_beginresults_(SPARQLQUERY *query, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;

	if(context->has_boolean)
	{
		sparql_logf_(context->connection, LOG_ERR, "result-set includes both <results> and <boolean>\n");
		return -1;
	}
	context->has_results = 1;
	return 0;
}

static int
sparql_query_beginresult_(SPARQLQUERY *query, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;

	context->row = sparqlrow_create_(context->results);
	if(!context->row)
	{
		sparql_logf_(context->connection, LOG_CRIT, "failed to create new result-set row\n");
		return -1;
	}
	return 0;
}

static int
sparql_query_endresult_(SPARQLQUERY *query, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;

	context->row = NULL;
	return 0;
}

static int
sparql_query_literal_(SPARQLQUERY *query, const char *name, const char *language, const char *datatype, const char *text, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;

	sparqlrow_set_literal_(context->row, name, language, datatype, text);
	return 0;
}

static int
sparql_query_uri_(SPARQLQUERY *query, const char *name, const char *uri, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;

	sparqlrow_set_uri_(context->row, name, uri);
	return 0;
}

static int
sparql_query_bnode_(SPARQLQUERY *query, const char *name, const char *ref, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;

	sparqlrow_set_bnode_(context->row, name, ref);
	return 0;
}

static int
sparql_query_boolean_(SPARQLQUERY *query, int value, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;

	if(context->has_results)
	{
		sparql_logf_(context->connection, LOG_ERR, "result-set includes both <results> and <boolean>\n");
		return -1;
	}
	context->has_boolean = 1;
	sparqlres_set_boolean_(context->results, value);
	return 0;
}

