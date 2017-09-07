/* SPARQL client library
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

#ifndef P_LIBSPARQLCLIENT_H_
# define P_LIBSPARQLCLIENT_H_           1

# include <stddef.h>
# include <stdlib.h>
# include <stdarg.h>
# include <string.h>
# include <ctype.h>
# include <errno.h>
# include <syslog.h>
# include <assert.h>
# include <curl/curl.h>
# include <libxml/parser.h>
# include <liburi.h>

# include "libsparqlclient.h"

# if _STDC_VERSION__ < 199901L && !defined(restrict)
#  define restrict                      /* */
# endif

# define SPARQLSTATE_URI_PARSE          "U0001"
# define SPARQLSTATE_URI_INFO           "U0002"
# define SPARQLSTATE_URI_QUERY          "U0003"

# define SPARQLSTATE_NO_DATASTORE       "X0001"
# define SPARQLSTATE_RS_NOTEMPTY        "X0002"
# define SPARQLSTATE_CREATE_WORLD       "X0003"
# define SPARQLSTATE_CREATE_URI         "X0004"
# define SPARQLSTATE_CREATE_NODE        "X0005"
# define SPARQLSTATE_CREATE_STREAM      "X0006"
# define SPARQLSTATE_BIND_INVALID       "X0007"
# define SPARQLSTATE_SERIALISE          "X0008"

# define SPARQLSTATE_INDEX_BOUNDS       "W0001"
# define SPARQLSTATE_RESET_BOOL         "W0002"
# define SPARQLSTATE_FETCH_BOOL         "W0003"

typedef struct sparql_query_struct SPARQLQUERY;
typedef enum sparql_parse_state SPARQLSTATE;

enum sparql_parse_state
{
	SQS_ERROR = -1,
	SQS_ROOT = 0,
	SQS_SPARQL,
	SQS_HEAD,
	SQS_LINK,
	SQS_VARIABLE,
	SQS_RESULTS,
	SQS_RESULT,
	SQS_BINDING,
	SQS_URI,
	SQS_LITERAL,
	SQS_BNODE,
	SQS_BOOLEAN,
	SQS_CAPTURE
};

struct sparql_capture_struct
{
	char *buf;
	size_t size;
	size_t pos;
};

struct sparql_connection_struct
{
	URI *base;
	char *query_uri;
	char *update_uri;
	char *data_uri;
	int verbose;
	sparql_logger_fn logger;
	librdf_world *world;
	int world_alloc;
	char state[16];
	char *error;
	struct sparql_capture_struct capture;
};

size_t sparql_urlencode_size_(const char *src);
size_t sparql_urlencode_lsize_(const char *src, size_t srclen);
int sparql_urlencode_(const char *src, char *dest, size_t destlen);
int sparql_urlencode_l_(const char *src, size_t srclen, char *dest, size_t destlen);

void sparql_set_error_(SPARQL *connection, const char *state, const char *error);
void sparql_set_nerror_(SPARQL *connection, int status, const char *error);

void sparql_logf_(SPARQL *connection, int priority, const char *format, ...);

SPARQLQUERY *sparql_query_create_(SPARQL *connection);
int sparql_query_destroy_(SPARQLQUERY *query);
int sparql_query_set_data_(SPARQLQUERY *query, void *data);
int sparql_query_set_variable_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, const char *name, void *data));
int sparql_query_set_link_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, const char *href, void *data));
int sparql_query_set_beginresults_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, void *data));
int sparql_query_set_endresults_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, void *data));
int sparql_query_set_beginresult_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, void *data));
int sparql_query_set_endresult_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, void *data));
int sparql_query_set_literal_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, const char *name, const char *language, const char *datatype, const char *text, void *data));
int sparql_query_set_uri_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, const char *name, const char *uri, void *data));
int sparql_query_set_bnode_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, const char *name, const char *ref, void *data));
int sparql_query_set_boolean_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, int value, void *data));
int sparql_query_set_complete_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, void *data));
int sparql_query_set_error_(SPARQLQUERY *query, int (*callback)(SPARQLQUERY *query, void *data));
int sparql_query_perform_(SPARQLQUERY *query, const char *statement, size_t length);

SPARQLRES *sparqlres_create_(SPARQL *connection);
int sparqlres_set_boolean_(SPARQLRES *res, int value);
int sparqlres_add_variable_(SPARQLRES *res, const char *name);
int sparqlres_add_link_(SPARQLRES *res, const char *href);

SPARQLROW *sparqlrow_create_(SPARQLRES *res);
int sparqlrow_set_uri_(SPARQLRES *res, SPARQLROW *row, const char *binding, const char *uri);
int sparqlrow_set_literal_(SPARQLRES *res, SPARQLROW *row, const char *binding, const char *language, const char *datatype, const char *value);
int sparqlrow_set_bnode_(SPARQLRES *res, SPARQLROW *row, const char *binding, const char *ref);

int sparql_vasprintf_(SPARQL *restrict connection, char *restrict *ptr, const char *restrict format_string, va_list vargs);

CURL *sparql_curl_create_(SPARQL *connection, const char *url);
int sparql_curl_perform_(CURL *ch);
size_t sparql_curl_dummy_write_(char *ptr, size_t size, size_t nemb, void *userdata);

#endif /*!P_LIBSPARQLCLIENT_H_*/
