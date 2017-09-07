/*
 * SPARQL client: result-sets
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2015 BBC
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

struct sparql_results_struct
{
	SPARQL *connection;
	int boolean;
	char **variables;
	size_t varcount;
	size_t varsize;
	size_t *widths;
	SPARQLROW *rows;
	size_t rowcount;
	size_t rowsize;
	char **links;
	size_t linkcount;
	size_t current;
	int reset;
};

struct sparql_row_struct
{
	SPARQLRES *results;
	librdf_node **nodes;
};

static int sparqlrow_set_node_(SPARQLRES *res, SPARQLROW *row, size_t index, librdf_node *node);
static char *sparqlrow_node_string_(SPARQLROW *row, librdf_node *node);

SPARQLRES *
sparqlres_create_(SPARQL *connection)
{
	SPARQLRES *p;
	
	p = (SPARQLRES *) calloc(1, sizeof(SPARQLRES));
	if(!p)
	{
		return NULL;
	}
	p->connection = connection;
	p->boolean = -1;
	p->reset = 1;
	return p;
}

int
sparqlres_is_boolean(SPARQLRES *res)
{
	if(res->boolean != -1)
	{
		return 1;
	}
	return 0;
}

int
sparqlres_set_boolean_(SPARQLRES *res, int value)
{
	res->boolean = value;
	return 0;
}

int
sparqlres_boolean(SPARQLRES *res)
{
	return res->boolean;
}

int
sparqlres_add_variable_(SPARQLRES *res, const char *name)
{
	char **p;
	size_t l;

	if(res->rowcount)
	{
		sparql_set_error_(res->connection, SPARQLSTATE_RS_NOTEMPTY, "cannot add a variable because rows have already been added to the result-set\n");
		return -1;
	}
	if(res->varcount + 1 >= res->varsize)
	{
		l = sizeof(char *) * (res->varsize + 4);
		p = (char **) realloc(res->variables, l);
		if(!p)
		{
			sparql_logf_(res->connection, LOG_CRIT, "failed to reallocate variable list to %u bytes\n", (unsigned) l);
			return -1;
		}
		res->variables = p;
		res->varsize += 4;
	}
	res->variables[res->varcount] = strdup(name);
	if(!res->variables[res->varcount])
	{
		sparql_logf_(res->connection, LOG_CRIT, "failed to allocate memory for variable name\n");
		return -1;
	}
	res->varcount++;
	return 0;
}

int
sparqlres_add_link_(SPARQLRES *res, const char *href)
{
	char **p;
	size_t l;
	
	l = sizeof(char *) * (res->linkcount + 1);
	p = (char **) realloc(res->links, l);
	if(!p)
	{
		sparql_logf_(res->connection, LOG_CRIT, "failed to allocate result-set link array to %u bytes\n", (unsigned) l);
		return -1;
	}
	res->links = p;
	p[res->linkcount] = strdup(href);
	if(!p[res->linkcount])
	{
		sparql_logf_(res->connection, LOG_CRIT, "failed to allocate buffer for link URI\n");
		return -1;
	}
	res->linkcount++;
	return 0;
}

size_t
sparqlres_variables(SPARQLRES *res)
{
	return res->varcount;
}

const char *
sparqlres_variable(SPARQLRES *res, size_t index)
{
	if(index >= res->varcount)
	{
		sparql_set_error_(res->connection, SPARQLSTATE_INDEX_BOUNDS, "attempt to retrieve variable from a result-set with an index out of bounds");
		return NULL;
	}
	return res->variables[index];
}

ssize_t
sparqlres_variable_index(SPARQLRES *res, const char *name)
{
	ssize_t index;

	for(index = 0; (size_t) index < res->varcount; index++)
	{
		if(!strcmp(res->variables[index], name))
		{
			return index;
		}
	}
	return -1;
}

size_t
sparqlres_links(SPARQLRES *res)
{
	return res->linkcount;
}

const char *
sparqlres_link(SPARQLRES *res, size_t index)
{
	if(index >= res->linkcount)
	{
		sparql_set_error_(res->connection, SPARQLSTATE_INDEX_BOUNDS, "attempt to retrieve link from a result-set with an index out of bounds");
		return NULL;
	}
	return res->links[index];
}

int
sparqlres_reset(SPARQLRES *res)
{
	if(res->boolean != -1)
	{
		sparql_set_error_(res->connection, SPARQLSTATE_RESET_BOOL, "attempt to reset the cursor on a boolean result-set");
		return -1;
	}
	res->reset = 1;
	return 0;
}

SPARQLROW *
sparqlres_next(SPARQLRES *res)
{
	if(res->boolean != -1)
	{
		sparql_set_error_(res->connection, SPARQLSTATE_FETCH_BOOL, "attempt to fetch a row from a boolean result-set");
		return NULL;
	}
	if(res->reset)
	{
		res->current = 0;
		res->reset = 0;
	}
	else
	{
		res->current++;
	}
	if(res->current >= res->rowcount)
	{
		return NULL;
	}
	return &(res->rows[res->current]);
}

size_t
sparqlres_rows(SPARQLRES *res)
{
	return res->rowcount;
}

size_t
sparqlres_width(SPARQLRES *res, size_t index)
{
	if(index > res->varcount || !res->widths)
	{
		return 0;
	}
	return res->widths[index];
}

int
sparqlres_destroy(SPARQLRES *res)
{
	size_t n, i;

	for(n = 0; n < res->linkcount; n++)
	{
		free(res->links[n]);
	}
	free(res->links);
	for(n = 0; n < res->varcount; n++)
	{
		free(res->variables[n]);
	}
	free(res->variables);
	for(i = 0; i < res->rowcount; i++)
	{
		for(n = 0; n < res->varcount; n++)
		{
			if(res->rows[i].nodes[n])
			{
				librdf_free_node(res->rows[i].nodes[n]);
			}
		}
		free(res->rows[i].nodes);
	}
	free(res->rows);
	free(res->widths);
	free(res);
	return 0;
}

SPARQLROW *
sparqlrow_create_(SPARQLRES *res)
{
	SPARQLROW *p;
	size_t l;

	if(res->rowcount + 1 >= res->rowsize)
	{
		l = sizeof(SPARQLROW) * (res->rowsize + 8);
		p = (SPARQLROW *) realloc(res->rows, l);
		if(!p)
		{
			sparql_logf_(res->connection, LOG_CRIT, "failed to reallocate row storage to %u bytes\n", (unsigned) l);
			return NULL;
		}
		res->rows = p;
		res->rowsize += 8;
	}
	p = &(res->rows[res->rowcount]);
	res->rowcount++;
	memset(p, 0, sizeof(SPARQLROW));
	p->results = res;
	if(res->varcount)
	{
		p->nodes = (librdf_node **) calloc(res->varcount, sizeof(librdf_node *));
		if(!p->nodes)
		{
			sparql_logf_(res->connection, LOG_CRIT, "failed to allocate memory for RDF nodes\n");
			res->rowcount--;
			return NULL;
		}
	}
	return p;
}

int
sparqlrow_set_uri_(SPARQLRES *res, SPARQLROW *row, const char *binding, const char *uri)
{
	size_t l;
	ssize_t index;
	librdf_world *world;
	librdf_node *node;

	world = sparql_world(row->results->connection);
	if(!world)
	{
		return -1;
	}
	index = sparqlres_variable_index(row->results, binding);
	if(index < 0)
	{
		sparql_logf_(row->results->connection, LOG_ERR, "failed to bind URI to '%s' because the variable does not exist\n", binding);
		sparql_set_error_(row->results->connection, SPARQLSTATE_BIND_INVALID, "failed to bind URI to a variable which does not exist");
		return -1;
	}
	node = librdf_new_node_from_uri_string(world, (const unsigned char *) uri);
	if(!node)
	{
		sparql_set_error_(row->results->connection, SPARQLSTATE_CREATE_NODE, "failed to create new URI node");
		return -1;
	}
	if(sparqlrow_set_node_(res, row, index, node))
	{
		return -1;
	}
	l = strlen(uri) + 2;
	if(res->widths[index] < l)
	{
		res->widths[index] = l;
	}
	return 0;
}

int
sparqlrow_set_bnode_(SPARQLRES *res, SPARQLROW *row, const char *binding, const char *ref)
{
	size_t l;
	ssize_t index;
	librdf_world *world;
	librdf_node *node;

	world = sparql_world(row->results->connection);
	if(!world)
	{
		return -1;
	}
	index = sparqlres_variable_index(row->results, binding);
	if(index < 0)
	{
		sparql_logf_(row->results->connection, LOG_ERR, "failed to bind URI to '%s' because the variable does not exist\n", binding);
		sparql_set_error_(row->results->connection, SPARQLSTATE_BIND_INVALID, "failed to bind URI to a variable which does not exist");
		return -1;
	}
	node = librdf_new_node_from_blank_identifier(world, (const unsigned char *) ref);
	if(!node)
	{
		sparql_set_error_(row->results->connection, SPARQLSTATE_CREATE_NODE, "failed to create new blank node");
		return -1;
	}
	if(sparqlrow_set_node_(res, row, index, node))
	{
		return -1;
	}
	l = strlen(ref) + 3;
	if(res->widths[index] < l)
	{
		res->widths[index] = l;
	}
	return 0;
}

int
sparqlrow_set_literal_(SPARQLRES *res, SPARQLROW *row, const char *binding, const char *language, const char *datatype, const char *value)
{
	size_t l;
	ssize_t index;
	librdf_world *world;
	librdf_node *node;
	librdf_uri *type;
	
	world = sparql_world(row->results->connection);
	if(!world)
	{
		return -1;
	}
	index = sparqlres_variable_index(row->results, binding);
	if(index < 0)
	{
		sparql_logf_(row->results->connection, LOG_ERR, "failed to bind URI to '%s' because the variable does not exist\n", binding);
		sparql_set_error_(row->results->connection, SPARQLSTATE_BIND_INVALID, "failed to bind URI to a variable which does not exist");
		return -1;
	}
	if(datatype)
	{
		type = librdf_new_uri(world, (const unsigned char *) datatype);
		if(!type)
		{
			sparql_set_error_(row->results->connection, SPARQLSTATE_CREATE_URI, "failed to create datatype URI");
			return -1;
		}
	}
	else
	{
		type = NULL;
	}
	node = librdf_new_node_from_typed_literal(world, (const unsigned char *) value, language, type);
	if(type)
	{
		librdf_free_uri(type);
	}
	if(!node)
	{
		sparql_set_error_(row->results->connection, SPARQLSTATE_CREATE_NODE, "failed to create new literal node");
		return -1;
	}
	if(sparqlrow_set_node_(res, row, index, node))
	{
		return -1;
	}
	l = strlen(value) + 2 + (language ? strlen(language) + 1 : 0) + (datatype ? strlen(datatype) + 2 : 0);
	if(res->widths[index] < l)
	{
		res->widths[index] = l;
	}
	return 0;
}

size_t
sparqlrow_bindings(SPARQLROW *row)
{
	return row->results->varcount;
}

librdf_node *
sparqlrow_binding(SPARQLROW *row, size_t index)
{
	if(index >= row->results->varcount)
	{
		sparql_set_error_(row->results->connection, SPARQLSTATE_INDEX_BOUNDS, "cannot retrieve a binding with an index out of range");
		return NULL;
	}
	sparql_set_nerror_(row->results->connection, 0, NULL);
	return row->nodes[index];
}

/* Copy at most buflen bytes of the value of result field index into
 * buf. A null terminator will always be added unless buf is null or buflen
 * is zero.
 *
 * The return value is:
 * -1 if an error occurs (note that this is cast to size_t)
 *  0 if there is no value (shouldn't ordinarily occur)
 *    otherwise, the number of bytes required to hold the value and the
 *    null terminator.
 */
size_t
sparqlrow_value(SPARQLROW *row, size_t index, char *buf, size_t buflen)
{
	char *str;
	size_t l;

	if(index >= row->results->varcount)
	{
		sparql_set_error_(row->results->connection, SPARQLSTATE_INDEX_BOUNDS, "cannot retrieve a value with an index out of range");
		return (size_t) -1;
	}
	str = sparqlrow_node_string_(row, row->nodes[index]);
	if(str)
	{
		l = strlen(str) + 1;
	}
	else
	{
		l = 0;
	}
	if(buf && buflen)
	{
		strncpy(buf, str, buflen - 1);
		buf[buflen - 1] = 0;
	}
	free(str);
	return l;
}

static int
sparqlrow_set_node_(SPARQLRES *res, SPARQLROW *row, size_t index, librdf_node *node)
{
	if(!res->widths)
	{
		res->widths = (size_t *) calloc(res->varcount, sizeof(size_t));
		if(!res->widths)
		{
			return -1;
		}
	}
	if(row->nodes[index])
	{
		librdf_free_node(row->nodes[index]);
	}
	row->nodes[index] = node;
	return 0;
}

static char *
sparqlrow_node_string_(SPARQLROW *row, librdf_node *node)
{
	raptor_world *world;
	raptor_iostream *stream;
	char *buf;
	int r;

	world = librdf_world_get_raptor(row->results->connection->world);
	buf = NULL;
	stream = raptor_new_iostream_to_string(world, (void **) &buf, NULL, malloc);
	if(!stream)
	{
		sparql_set_error_(row->results->connection, SPARQLSTATE_CREATE_STREAM, "failed to create Raptor iostream for node serialisation");
		return NULL;
	}
	r = librdf_node_write(node, stream);
	raptor_free_iostream(stream);
	if(r)
	{
		sparql_set_error_(row->results->connection, SPARQLSTATE_SERIALISE, "failed to serialise node");
		free(buf);
		return NULL;
	}
	return buf;
}
