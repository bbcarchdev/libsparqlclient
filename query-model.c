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
	librdf_world *world;
	librdf_model *model;
	librdf_node *g;
	librdf_statement *statement;
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
int
sparql_query_model(SPARQL *connection, const char *querybuf, size_t length, librdf_model *model)
{
	struct sparql_query_context_struct context;
	int r;

	memset(&context, 0, sizeof(context));
	context.connection = connection;
	context.query = sparql_query_create_(connection);
	if(!context.query)
	{
		sparql_logf_(connection, LOG_CRIT, "failed to create SPARQL query structure\n");
		return -1;
	}
	context.world = sparql_world(connection);
	if(!context.world)
	{
		return -1;
	}
	context.model = model;
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
	r = sparql_query_perform_(context.query, querybuf, length);
	sparql_query_destroy_(context.query);
	if(context.statement)
	{
		librdf_free_statement(context.statement);
	}
	if(context.g)
	{
		librdf_free_node(context.g);
	}
	return r;
}

static int 
sparql_query_variable_(SPARQLQUERY *query, const char *name, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;

	if(strcmp(name, "g") && strcmp(name, "s") && strcmp(name, "p") &&
	   strcmp(name, "o"))
	{
		sparql_logf_(context->connection, LOG_ERR, "unexpected variable '%s' (expected one of ?g, ?s, ?p, ?o)\n", name);
		return -1;
	}
	return 0;
}

static int 
sparql_query_link_(SPARQLQUERY *query, const char *href, void *data)
{
	(void) query;
	(void) href;
	(void) data;

	return 0;
}

static int 
sparql_query_beginresults_(SPARQLQUERY *query, void *data)
{
	(void) query;
	(void) data;

	return 0;
}

static int
sparql_query_beginresult_(SPARQLQUERY *query, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;

	context->statement = librdf_new_statement(context->world);
	if(!context->statement)
	{
		sparql_logf_(context->connection, LOG_CRIT, "failed to create new librdf statement\n");
		return -1;
	}
	return 0;
}

static int
sparql_query_endresult_(SPARQLQUERY *query, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;

	if(!librdf_statement_is_complete(context->statement))
	{
		sparql_logf_(context->connection, LOG_ERR, "result row does not contain a complete statement\n");
		return -1;
	}
	if(context->g)
	{
		librdf_model_context_add_statement(context->model, context->g, context->statement);
	}
	else
	{
		librdf_model_add_statement(context->model, context->statement);
	}
	librdf_free_statement(context->statement);
	context->statement = NULL;
	if(context->g)
	{
		librdf_free_node(context->g);
		context->g = NULL;
	}
	return 0;
}

static int
sparql_query_literal_(SPARQLQUERY *query, const char *name, const char *language, const char *datatype, const char *text, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;
	librdf_node *node;
	librdf_uri *typeuri;

	(void) query;

	if(datatype)
	{
		typeuri = librdf_new_uri(context->world, (const unsigned char *) datatype);
	}
	else
	{
		typeuri = NULL;
	}
	node = librdf_new_node_from_typed_literal(context->world, (const unsigned char *) text, language, typeuri);
	if(!strcmp(name, "o"))
	{
		librdf_statement_set_object(context->statement, node);
		return 0;
	}
	if(typeuri)
	{
		librdf_free_uri(typeuri);
	}
	librdf_free_node(node);
	sparql_logf_(context->connection, LOG_ERR, "unexpected literal value bound to '%s'\n", name);
	return -1;	
}

static int
sparql_query_uri_(SPARQLQUERY *query, const char *name, const char *uri, void *data)
{
	librdf_node *node;	
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;

	node = librdf_new_node_from_uri_string(context->world, (const unsigned char *) uri);
	if(!strcmp(name, "g"))
	{
		context->g = node;
	}
	else if(!strcmp(name, "s"))
	{
		librdf_statement_set_subject(context->statement, node);
	}
	else if(!strcmp(name, "p"))
	{
		librdf_statement_set_predicate(context->statement, node);
	}
	else if(!strcmp(name, "o"))
	{
		librdf_statement_set_object(context->statement, node);
	}
	return 0;
}

static int
sparql_query_bnode_(SPARQLQUERY *query, const char *name, const char *ref, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;
	librdf_node *node;

	(void) query;

	node = librdf_new_node_from_blank_identifier(context->world, (const unsigned char *) ref);
	if(!strcmp(name, "g"))
	{
		context->g = node;
	}
	else if(!strcmp(name, "s"))
	{
		librdf_statement_set_subject(context->statement, node);
	}
	else if(!strcmp(name, "p"))
	{
		librdf_statement_set_predicate(context->statement, node);
	}
	else if(!strcmp(name, "o"))
	{
		librdf_statement_set_object(context->statement, node);
	}
	return 0;
}

static int
sparql_query_boolean_(SPARQLQUERY *query, int value, void *data)
{
	struct sparql_query_context_struct *context = (struct sparql_query_context_struct *) data;

	(void) query;
	(void) value;

	sparql_logf_(context->connection, LOG_ERR, "unexpected boolean result from SPARQL query\n");
	return -1;
}

