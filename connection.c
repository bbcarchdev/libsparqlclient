/* SPARQL client: connection structure
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

static int sparql_librdf_logger_(void *data, librdf_log_message *message);

SPARQL *
sparql_create(void *reserved)
{
	SPARQL *p;
	
	(void) reserved;

	p = (SPARQL *) calloc(1, sizeof(SPARQL));
	if(!p)
	{
		return NULL;
	}
	
	return p;
}

int
sparql_destroy(SPARQL *connection)
{
	if(connection->world_alloc)
	{
		librdf_free_world(connection->world);
	}
	free(connection->query_uri);
	free(connection->update_uri);
	free(connection->data_uri);
	free(connection);
	return 0;
}

int
sparql_set_query_uri(SPARQL *connection, const char *uri)
{
	char *p;
	
	p = strdup(uri);
	if(!p)
	{
		return -1;
	}
	free(connection->query_uri);
	connection->query_uri = p;
	return 0;
}

int
sparql_set_data_uri(SPARQL *connection, const char *uri)
{
	char *p;
	
	p = strdup(uri);
	if(!p)
	{
		return -1;
	}
	free(connection->data_uri);
	connection->data_uri = p;
	return 0;
}

int
sparql_set_update_uri(SPARQL *connection, const char *uri)
{
	char *p;
	
	p = strdup(uri);
	if(!p)
	{
		return -1;
	}
	free(connection->update_uri);
	connection->update_uri = p;
	return 0;
}

int sparql_set_logger(SPARQL *connection, sparql_logger_fn logger)
{
	connection->logger = logger;
	return 0;
}

int
sparql_set_verbose(SPARQL *connection, int verbose)
{
	connection->verbose = verbose;
	return 0;
}

int
sparql_set_world(SPARQL *connection, librdf_world *world)
{
	if(connection->world_alloc)
	{
		librdf_free_world(connection->world);
	}
	connection->world = world;
	return 0;
}

librdf_world *
sparql_world(SPARQL *connection)
{
	if(!connection->world)
	{
		connection->world_alloc = 1;
		connection->world = librdf_new_world();
		if(!connection->world)
		{
			sparql_logf_(connection, LOG_CRIT, "failed to create new librdf world\n");
			return NULL;
		}
		librdf_world_open(connection->world);
		librdf_world_set_logger(connection->world, (void *) connection, sparql_librdf_logger_);
	}
	return connection->world;
}

void
sparql_logf_(SPARQL *connection, int priority, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	if(connection->logger)
	{
		connection->logger(priority, format, ap);
	}
}

static int
sparql_librdf_logger_(void *data, librdf_log_message *message)
{
	int level;
	SPARQL *connection = (SPARQL *) data;
        
	switch(librdf_log_message_level(message))
	{
	case LIBRDF_LOG_DEBUG:
		level = LOG_DEBUG;
		break;
	case LIBRDF_LOG_INFO:
		level = LOG_INFO;
		break;
	case LIBRDF_LOG_WARN:
		level = LOG_WARNING;
		break;
	case LIBRDF_LOG_ERROR:
		level = LOG_ERR;
		break;
	case LIBRDF_LOG_FATAL:
		level = LOG_CRIT;
		break;
	default:
		level = LOG_NOTICE;
		break;
	}
	sparql_logf_(connection, level, "%s\n", librdf_log_message_message(message));
	return 0;
}

