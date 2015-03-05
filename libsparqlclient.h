/* SPARQL client
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

#ifndef LIBSPARQLCLIENT_H_
# define LIBSPARQLCLIENT_H_             1

# include <stddef.h>
# include <stdarg.h>
# include <librdf.h>

typedef struct sparql_connection_struct SPARQL;
typedef struct sparql_results_struct SPARQLRES;
typedef struct sparql_row_struct SPARQLROW;

# ifdef __cplusplus
extern "C" {
# endif

typedef void (*sparql_logger_fn)(int priority, const char *format, va_list args);

SPARQL *sparql_create(const char *baseuri);
int sparql_destroy(SPARQL *connection);
int sparql_set_query_uri(SPARQL *connection, const char *uri);
int sparql_set_data_uri(SPARQL *connection, const char *uri);
int sparql_set_update_uri(SPARQL *connection, const char *uri);
int sparql_set_logger(SPARQL *connection, sparql_logger_fn logger);
int sparql_set_verbose(SPARQL *connection, int verbose);
int sparql_set_world(SPARQL *connection, librdf_world *world);
librdf_world *sparql_world(SPARQL *connection);
librdf_storage *sparql_storage(SPARQL *connection);

const char *sparql_state(SPARQL *connection);
const char *sparql_error(SPARQL *connection);

SPARQLRES *sparql_query(SPARQL *connection, const char *query, size_t length);
SPARQLRES *sparql_vqueryf(SPARQL *connection, const char *format, va_list ap);
SPARQLRES *sparql_queryf(SPARQL *connection, const char *format, ...);

int sparql_query_model(SPARQL *connection, const char *querybuf, size_t length, librdf_model *model);
int sparql_vqueryf_model(SPARQL *connection, librdf_model *model, const char *format, va_list ap);
int sparql_queryf_model(SPARQL *connection, librdf_model *model, const char *format, ...);

int sparql_update(SPARQL *connection, const char *statement, size_t length);
int sparql_vupdatef(SPARQL *connection, const char *format, va_list ap);
int sparql_updatef(SPARQL *connection, const char *format, ...);

int sparql_put(SPARQL *connection, const char *graph, const char *turtle, size_t length);
int sparql_insert(SPARQL *connection, const char *triples, size_t len, const char *graphuri);
int sparql_insert_stream(SPARQL *connection, librdf_stream *stream, const char *graphuri);
int sparql_insert_model(SPARQL *connection, librdf_model *model);

int sparqlres_is_boolean(SPARQLRES *res);
int sparqlres_boolean(SPARQLRES *res);
size_t sparqlres_variables(SPARQLRES *res);
const char *sparqlres_variable(SPARQLRES *res, size_t index);
ssize_t sparqlres_variable_index(SPARQLRES *res, const char *name);
size_t sparqlres_links(SPARQLRES *res);
const char *sparqlres_link(SPARQLRES *res, size_t index);
int sparqlres_reset(SPARQLRES *res);
SPARQLROW *sparqlres_next(SPARQLRES *res);
int sparqlres_destroy(SPARQLRES *res);
size_t sparqlres_width(SPARQLRES *res, size_t index);
size_t sparqlres_rows(SPARQLRES *res);

size_t sparqlrow_bindings(SPARQLROW *row);
librdf_node *sparqlrow_binding(SPARQLROW *row, size_t index);
size_t sparqlrow_value(SPARQLROW *row, size_t index, char *buf, size_t bufsize);

# ifdef __cplusplus
}
# endif

#endif /*!LIBSPARQLCLIENT_H_*/
