/* SPARQL client: connection structure
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

#include "p_libsparqlclient.h"

static int sparql_librdf_logger_(void *data, librdf_log_message *message);
static char *sparql_derive_uri_(SPARQL *connection, const URI *base, URI_INFO *info, const char *key, const char *defuri);

/* Create a new SPARQL client connection */
SPARQL *
sparql_create(const char *base)
{
	SPARQL *p;

	p = (SPARQL *) calloc(1, sizeof(SPARQL));
	if(!p)
	{
		return NULL;
	}
	if(base)
	{
		if(sparql_set_base(p, base))
		{
			sparql_destroy(p);
			return NULL;
		}
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
	if(connection->base)
	{
		uri_destroy(connection->base);
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
	else
	{
		strcpy(connection->state, "00000");
	}
	if(connection->error)
	{
		free(connection->error);
	}
	connection->error = s;	
	if(s)
	{
		sparql_logf_(connection, LOG_ERR, "SPARQL Error [%s] %s\n", connection->state, s);
	}
	else
	{
		sparql_logf_(connection, LOG_ERR, "SPARQL Error [%s] (unknown error)\n", connection->state);
	}
}

void
sparql_set_nerror_(SPARQL *connection, int status, const char *error)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%05d", status);
	sparql_set_error_(connection, buf, error);
}

/* Set the SPARQL server's base URI
 *
 * We accept URIs in the following forms:
 *
 * sparql+http://server/basepath?OPTIONS
 * sparql+https://server/basepath?OPTIONS
 *
 * For compatibility, the 'sparql+' prefix may be omitted.
 *
 * OPTIONS are any combination of:
 *
 *  query-uri=xxxx      Specify an alternative query URI (defaults to 'sparql')
 *  update-uri=xxxx     Specify an alternative update URI (defaults to 'sparql')
 *  data-uri=xxxx       Specify an alternative PUT/POST data URI (no default)
 *
 * URIs specified in OPTIONS are resolved relative to the base path.
 *
 * The following additional schemes are recognised, which have different
 * defaults:
 *
 * 4store+http[s]://server/basepath
 *     Connect to a 4store server. The default query-uri is /sparql, the default
 *     update-uri is /update, and the default data-uri is /data.
 *
 * Note that invoking this function will replace any existing base, query,
 * update or data URIs and options.
 */

int
sparql_set_base(SPARQL *connection, const char *uri)
{
	URI *base;
	URI_INFO *info;
	char *basestr, *query, *update, *data;
	const char *def_query = "sparql/", *def_update = "sparql/", *def_data = NULL;

	basestr = NULL;
	if(!strncmp(uri, "sparql+http:", 11) || !strncmp(uri, "sparql+https:", 12))
	{
		uri += 7; /* Skip 'sparql+' */
	}
	else if(!strncmp(uri, "sparql:", 7) || !strncmp(uri, "sparqls:", 8))
	{
		basestr = strdup(uri);
		if(uri[6] == ':')
		{
			strcpy(basestr, "http:");
			strcpy(&(basestr[5]), &(uri[7]));
		}
		else
		{
			strcpy(basestr, "https:");
			strcpy(&(basestr[6]), &(uri[8]));
		}
		uri = basestr;
	}
	else if(!strncmp(uri, "4store+http:", 11) || !strncmp(uri, "4store+https:", 12))
	{
		def_update = "update/";
		def_data = "data/";
		uri += 7; /* Skip '4store+' */
	}
	else if(!strncmp(uri, "4store:", 7) || !strncmp(uri, "4stores:", 8))
	{
		basestr = strdup(uri);
		if(uri[6] == ':')
		{
			strcpy(basestr, "http:");
			strcpy(&(basestr[5]), &(uri[7]));
		}
		else
		{
			strcpy(basestr, "https:");
			strcpy(&(basestr[6]), &(uri[8]));
		}
		def_update = "update/";
		def_data = "data/";
		uri = basestr;
	}
	base = uri_create_str(uri, NULL);
	if(!base)
	{
		sparql_set_error_(connection, "U0001", "Failed to parse base URI");
		free(basestr);
		return -1;
	}
	info = uri_info(base);
	if(!info)
	{
		sparql_set_error_(connection, "U0002", "Failed to obtain information from parsed URI");
		uri_destroy(base);
		free(basestr);
		return -1;
	}
	free(basestr);

	query = sparql_derive_uri_(connection, base, info, "query-uri", def_query);
	update = sparql_derive_uri_(connection, base, info, "update-uri", def_update);
	data = sparql_derive_uri_(connection, base, info, "data-uri", def_data);

	uri_info_destroy(info);
	
	if(!query)
	{
		sparql_set_error_(connection, "U0003", "Failed to derived query URI from base URI");
		uri_destroy(base);
		return -1;
	}
	free(connection->query_uri);
	free(connection->update_uri);
	free(connection->data_uri);
	if(connection->base)
	{
		uri_destroy(connection->base);
	}
	connection->base = base;
	connection->query_uri = query;
	connection->update_uri = update;
	connection->data_uri = data;

	return 0;
}

/* DEPRECATED */
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

/* DEPRECATED */
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

/* DEPRECATED */
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

int
sparql_set_logger(SPARQL *connection, sparql_logger_fn logger)
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
	connection->world_alloc = 0;
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
	sparql_logf_(connection, level, "RDF: %s\n", librdf_log_message_message(message));
	return 0;
}

/* Given a query-string parameter name <key>, extract a URI from <info>,
 * defaulting to <defuri> if not provided, resolve it against the
 * <connection>'s base URI, and return a pointer to a newly-allocated
 * string containing the resulting absolute URI.
 *
 */
static char *
sparql_derive_uri_(SPARQL *connection, const URI *base, URI_INFO *info, const char *key, const char *defuri)
{
	const char *uristr;
	URI *uri;
	char *s;

	(void) connection;

	if(info)
	{
		uristr = uri_info_get(info, key, defuri);
		if(!uristr)
		{
			if(defuri)
			{
				/* This shouldn't happen */
			}
			return NULL;	  
		}
	}
	else
	{
		uristr = defuri;
	}
	uri = uri_create_str(uristr, base);
	if(!uri)
	{
		return NULL;
	}
	s = uri_stralloc(uri);
	uri_destroy(uri);
	return s;
}
