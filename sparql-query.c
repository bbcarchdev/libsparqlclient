/* SPARQL client library: simple command-line query tool
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>

#include "libsparqlclient.h"

static char *node_to_string(SPARQL *sparql, librdf_node *node);

void
logger(int priority, const char *format, va_list ap)
{
	if(priority > LOG_INFO)
	{
		return;
	}
	fprintf(stderr, "SPARQL<%d>: ", priority);
	vfprintf(stderr, format, ap);
}

int
main(int argc, char **argv)
{
	SPARQL *sparql;
	SPARQLRES *result;
	SPARQLROW *row;
	size_t index, count;
	librdf_node *node;
	
	if(argc != 3)
	{
		fprintf(stderr, "Usage: %s URI QUERY\n", argv[0]);
		return 1;
	}
	sparql = sparql_create(argv[1]);
	sparql_set_logger(sparql, logger);
	sparql_set_verbose(sparql, 1);
	result = sparql_query(sparql, argv[2], strlen(argv[2]));
	if(!result)
	{
		fprintf(stderr, "query failed\n");
		sparql_destroy(sparql);
		return 1;
	}
	if(sparqlres_is_boolean(result))
	{
		if(sparqlres_boolean(result))
		{
			puts("true");
		}
		else
		{
			puts("false");
		}
		sparqlres_destroy(result);
		sparql_destroy(sparql);
		return 0;
	}
	count = sparqlres_variables(result);
	for(row = sparqlres_next(result); row; row = sparqlres_next(result))
	{
		for(index = 0; index < count; index++)
		{
			node = sparqlrow_binding(row, index);
			if(node)
			{
				printf("%s: %s\n", sparqlres_variable(result, index), node_to_string(sparql, node));
			}
			else
			{
				printf("%s: NULL\n", sparqlres_variable(result, index));
			}
		}
		printf("\n");
	}
	sparqlres_destroy(result);
	sparql_destroy(sparql);
	return 0;
}

static char *
node_to_string(SPARQL *sparql, librdf_node *node)
{
	raptor_world *world;
	raptor_iostream *stream;
	char *buf;
	int r;

	world = librdf_world_get_raptor(sparql_world(sparql));
	buf = NULL;
	stream = raptor_new_iostream_to_string(world, (void **) &buf, NULL, malloc);
	if(!stream)
	{
		return NULL;
	}
	r = librdf_node_write(node, stream);
	raptor_free_iostream(stream);
	if(r)
	{
		free(buf);
		return NULL;
	}
	return buf;
}
