/* SPARQL client: connection structure
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

static int sparql_librdf_logger_(void *data, librdf_log_message *message);

SPARQL *
sparql_create(const char *base)
{
	SPARQL *p;
	size_t l;

	p = (SPARQL *) calloc(1, sizeof(SPARQL));
	if(!p)
	{
		return NULL;
	}
	if(base && base[0])
	{
		l = strlen(base);
		p->query_uri = (char *) calloc(1, l + 32);
		p->update_uri = (char *) calloc(1, l + 32);
		p->data_uri = (char *) calloc(1, l + 32);
		if(!p->query_uri || !p->update_uri || !p->data_uri)
		{
			sparql_destroy(p);
			return NULL;
		}
		if(base[l - 1] == '/')
		{
			l--;
		}
		strcpy(p->query_uri, base);
		strcpy(&(p->query_uri[l]), "/sparql/");
		strcpy(p->update_uri, base);
		strcpy(&(p->update_uri[l]), "/update/");
		strcpy(p->data_uri, base);
		strcpy(&(p->data_uri[l]), "/data/");
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
	free(connection->error);
	free(connection->capture.buf);
	free(connection);
	return 0;
}

const char *
sparql_state(SPARQL *connection)
{
	return connection->state;
}

const char *
sparql_error(SPARQL *connection)
{
	if(connection->error)
	{
		return connection->error;
	}
	return "Unknown error";
}

void
sparql_set_error_(SPARQL *connection, const char *state, const char *error)
{
	char *s;
	
	if(error)
	{
		s = strdup(error);
	}
	else
	{
		s = NULL;
	}
	if(state)
	{
		strncpy(connection->state, state, 5);
	}
	if(connection->error)
	{
		free(connection->error);
	}
	connection->error = s;	
}

void
sparql_set_nerror_(SPARQL *connection, int status, const char *error)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%05d", status);
	sparql_set_error_(connection, buf, error);
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

	if(priority == LOG_DEBUG && !(connection->verbose))
	{
		return;
	}		   
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

