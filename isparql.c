/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2016 BBC
 */

/*
 * Copyright 2012-2013 Mo McRoberts.
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
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <syslog.h>

#include "histedit.h"
#include "libsparqlclient.h"

#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS                  0
#endif

#ifndef EXIT_FAILURE
# define EXIT_FAILURE                  1
#endif
	 
#define QUERY_BLOCK                    128
#define PQUERY_BLOCK                   4
	 
struct query_struct
{
	char *query;
	int output_mode;
};

static const char *short_program_name, *connect_uri;
static int query_state = 0;
static char *query_buf;
static size_t query_len, query_alloc;
static struct query_struct *pqueries;
static size_t pquery_count, pquery_alloc;
static SPARQL *sparql_conn;

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [URI]\n", short_program_name);
}

static int
check_args(int argc, char **argv)
{
	char *t;
	int c;
	
	connect_uri = NULL;
	t = strrchr(argv[0], '/');
	if(t)
	{
		short_program_name = t + 1;
	}
	else
	{
		short_program_name = t;
	}
	while((c = getopt(argc, argv, "h")) != -1)
	{
		switch(c)
		{
			case 'h':
				usage();
				exit(EXIT_SUCCESS);
			default:
				return -1;
		}
	}
	argc -= optind;
	argv += optind;
	if(argc > 1)
	{
		return -1;
	}
	if(argc == 1)
	{
		connect_uri = argv[0];
	}
	return 0;
}

static char *
prompt(EditLine *el)
{
	(void) el;

	switch(query_state)
	{
	case 0:
		return "SPARQL> ";
	case '\'':
		return "  '> ";
	case '"':
		return "  \"> ";
	}
	return "  -> ";
}

static void
logger(int level, const char *format, va_list ap)
{
	switch(level)
	{
	case LOG_DEBUG:
		printf("[Debug] ");
		break;
	case LOG_WARNING:
		printf("Warning: ");
		break;
	case LOG_ERR:
	case LOG_CRIT:
		printf("Error: ");
		break;
	}
	vprintf(format, ap);
}

static int
show_results(SPARQLRES *rs)
{
	size_t d, max, len;
	size_t *widths;
	unsigned int cols, c;
	const char **fields, *n;
	char *fbuf;	
	SPARQLROW *row;
	librdf_node *node;
	
	cols = sparqlres_variables(rs);
	if(!cols)
	{
		return 0;
	}
	fbuf = NULL;
	max = 0;
	fields = (const char **) calloc(cols, sizeof(const char *));
	widths = (size_t *) calloc(cols, sizeof(size_t));
	putchar('+');
	for(c = 0; c < cols; c++)
	{
		fields[c] = sparqlres_variable(rs, c);
		widths[c] = sparqlres_width(rs, c);
		n = fields[c];
		if(n && strlen(n) > widths[c])
		{
			widths[c] = strlen(n);
		}
		if(widths[c] > max)
		{
			max = widths[c];
		}
		for(d = 0; d < widths[c] + 2; d++)
		{
			putchar('-');
		}
		putchar('+');
	}
	if(max < 4)
	{
		max = 4;
	}
	fbuf = (char *) malloc(max + 1);
	putchar('\n');
	putchar('|');
	for(c = 0; c < cols; c++)
	{
		putchar(' ');
		n = fields[c];
		d = widths[c] - strlen(n);
		fputs(n, stdout);
		for(; d; d--)
		{
			putchar(' ');
		}
		putchar(' ');
		putchar('|');
	}
	putchar('\n');
	putchar('+');
	for(c = 0; c < cols; c++)
	{
		for(d = 0; d < widths[c] + 2; d++)
		{
			putchar('-');
		}
		putchar('+');
	}
	putchar('\n');
	while((row = sparqlres_next(rs)))
	{
		putchar('|');
		for(c = 0; c < cols; c++)
		{
			putchar(' ');
			node = sparqlrow_binding(row, c);
			if(!node)
			{
				strcpy(fbuf, "NULL");
				len = 4;
			}
			else
			{
				len = sparqlrow_value(row, c, fbuf, max + 1);
				/* if len is ((size_t) -1), an error occurred; if len = 0, there
				 * is no value to retrieve; otherwise, the length includes the
				 * terminating NULL byte.
				 */
				if(len == (size_t) - 1 || len == 0)
				{
					len = 0;
				}
				else
				{
					len--;
				}
			}
			for(d = 0; d < widths[c]; d++)
			{
				if(d < len)
				{
					putchar(fbuf[d]);
				}
				else
				{
					putchar(' ');
				}
			}
			putchar(' ');
			putchar('|');
		}
		putchar('\n');
	}
	putchar('+');
	for(c = 0; c < cols; c++)
	{
		for(d = 0; d < widths[c] + 2; d++)
		{
			putchar('-');
		}
		putchar('+');
	}
	putchar('\n');
	free(fbuf);
	free(fields);
	free(widths);
	return 0;
}

static int
show_results_long(SPARQLRES *rs)
{
	unsigned int cols, c;
	unsigned long long nrow;
	SPARQLROW *row;
	librdf_node *node;
	size_t max, w, len, maxname;
	const char **fields, *n;
	char *fbuf;
	char fmt[64];
	
	cols = sparqlres_variables(rs);
	if(!cols)
	{
		return 0;
	}
	fields = (const char **) calloc(cols, sizeof(const char *));
	max = 0;
	maxname = 0;
	for(c = 0; c < cols; c++)
	{
		fields[c] = sparqlres_variable(rs, c);
		w = sparqlres_width(rs, c);
		if(w > max)
		{
			max = w;
		}
		n = fields[c];
		w = strlen(n);
		if(w > maxname)
		{
			maxname = w;
		}
	}
	if(maxname)
	{
		sprintf(fmt, "%%%us: ", (unsigned) maxname);
	}
	else
	{
		strcpy(fmt, "%s: ");
	}
	fbuf = (char *) malloc(max + 1);
	nrow = 0;
	while((row = sparqlres_next(rs)))
	{
		nrow++;
		printf("*************************** %qu. row ***************************\n", nrow);
		for(c = 0; c < cols; c++)
		{
			printf(fmt, fields[c]);
			node = sparqlrow_binding(row, c);
			if(!node)
			{
				puts("NULL");
			}
			else
			{
				len = sparqlrow_value(row, c, fbuf, max + 1);			
				if(len == (size_t) - 1)
				{
					len = 0;
				}
				for(w = 0; w < len; w++)
				{
					putchar(fbuf[w]);
				}
				putchar('\n');
			}
		}
	}
	free(fbuf);
	free(fields);
	return 0;
}

static int
add_query(char *query, int output_mode)
{
	struct query_struct *p;
	
	if(pquery_count + 1 > pquery_alloc)
	{
		pquery_alloc += PQUERY_BLOCK;
		p = (struct query_struct *) realloc(pqueries, sizeof(struct query_struct) * pquery_alloc);
		if(!p)
		{
			fprintf(stderr, "%s: failed to allocate %u bytes for queries\n", short_program_name, (unsigned) (sizeof(struct query_struct) * pquery_alloc));
			exit(EXIT_FAILURE);
		}
		pqueries = p;
	}
	pqueries[pquery_count].query = query;
	pqueries[pquery_count].output_mode = output_mode;
	pquery_count++;
	return 0;
}

static int
parse_query(const char *buf)
{
	size_t l;
	char *p, *qs;
	
	pquery_count = 0;
	if(buf)
	{
		l = strlen(buf);
		if(l + query_len + 1 > query_alloc)
		{
			query_alloc = (((query_len + l + 1) / QUERY_BLOCK) + 1) * QUERY_BLOCK;
			p = (char *) realloc(query_buf, query_alloc);
			if(!p)
			{
				fprintf(stderr, "%s: failed to allocate %u bytes for query buffer\n", short_program_name, (unsigned) query_alloc);
				exit(EXIT_FAILURE);
			}
			query_buf = p;		
		}
		p = &(query_buf[query_len]);
		strcpy(p, buf);
		query_len += l;
	}
	p = query_buf;
	query_state = 0;

	/* The state-keeping algorithm is fairly simplistic; although it only
	 * needs to care about quoting, it should (and doesn't yet) handle
	 * nesting properly, which is why it doesn't attempt to deal with
	 * parentheses at all.
	 */
	while(*p)
	{
		switch(query_state)
		{
		case 0:
			if(isspace(*p) || *p == '\r' || *p == '\n')
			{
				p++;
				break;
			}
			qs = p;
			switch(*p)
			{
				case '\'':
				case '"':
					query_state = *p;
					break;
				case '\\':
					if(p[1] == 'g' || p[1] == 'G')
					{
						p++;
						break;
					}
					qs = p;
					query_state = 2;					
					break;
				case ';':
					break;
				default:
					query_state = 1;
					break;
			}
			p++;
			break;
		case 1:
			if(*p == ';')
			{
				add_query(qs, *p);
				*p = 0;
				query_state = 0;
				p++;
				break;
			}
			if(p[0] == '\\' && p[1] == 'g')
			{
				add_query(qs, p[1]);
				*p = 0;
				query_state = 0;
				p += 2;
				break;
			}
			if(p[0] == '\\' && p[1] == 'G')
			{
				add_query(qs, p[1]);
				*p = 0;
				query_state = 0;
				p += 2;
				break;
			}
			switch(*p)
			{
				case '\'':
				case '"':
				case '`':
				case '{':
				case '[':
					query_state = *p;
					break;
			}
			p++;
			break;
		case 2:
			/* Built-in commands */
			p++;
			break;
		case '\'':
		case '"':
		case '`':
			if(*p == query_state)
			{
				query_state = 1;
			}
			p++;
			break;
		case '{':
			if(*p == '}')
			{
				query_state = 1;
			}
			p++;
			break;
		case '[':
			if(*p == ']')
			{
				query_state = 1;
			}
			p++;
			break;
		}
	}
	if(query_state == 2)
	{
		/* Trim any trailing whitespace before adding to the query list */
		p--;
		while(p > qs)
		{
			if(!isspace(*p) && *p != '\r' && *p != '\n')
			{
				break;
			}
			*p = 0;
			p--;
		}
		add_query(qs, 0);
		query_state = 0;
	}
	if(!query_state)
	{
		query_len = 0;
	}
	return query_state;
}

static int
exec_builtin(SPARQL *conn, History *hist, char *query)
{
	(void) hist;
	
	if(!strncmp(query, "\\q", 2))
	{
		exit(EXIT_SUCCESS);
	}
	if(!strncmp(query, "\\c", 2))
	{
		query += 2;
		while(isspace(*query)) query++;
		if(!*query)
		{
			printf("[08000] Specify \"\\c URI\" to establish a new connection\n");
			return -1;
		}
		conn = sparql_create(query);		
		if(!conn)
		{
			printf("failed to connect to SPARQL service\n");
			return -1;
		}
		sparql_set_logger(conn, logger);
		if(sparql_conn)
		{
			sparql_destroy(sparql_conn);
		}
		sparql_conn = conn;
		return 0;
	}
	printf("[42000] Unknown command '%s'\n", query);
	return -1;
}

static int
query_is_update(const char *query, size_t len)
{
	static const char *updateverbs[] = {
		"INSERT", "DELETE", "LOAD", "CLEAR", "CREATE", "DROP", "COPY",
		"MOVE", "ADD",
		NULL
	};
	const char *verb;
	size_t c, vl;

	for(verb = query; verb < query + len; verb++)
	{
		if(!isspace(*verb))
		{
			break;
		}
	}
	if(verb >= query + len)
	{
		return 0;
	}
	for(vl = 0; verb + vl < query + len; vl++)
	{
		if(isspace(verb[vl]))
		{
			break;
		}
	}
	for(c = 0; updateverbs[c]; c++)
	{
		if(vl == strlen(updateverbs[c]) &&
		   !strncasecmp(verb, updateverbs[c], vl))
		{
			return 1;
		}
	}
	return 0;
}

static int
exec_queries(History *hist)
{
	HistEvent ev;
	size_t c, l;
	SPARQLRES *rs;
	unsigned int cols;
	unsigned long long rows;
	
	for(c = 0; c < pquery_count; c++)
	{
		history(hist, &ev, H_ENTER, pqueries[c].query);
		if(pqueries[c].output_mode == 'G')
		{
			history(hist, &ev, H_ADD, "\\G");
		}
		else
		{
			history(hist, &ev, H_ADD, ";");
		}
		if(pqueries[c].query[0] == '\\')
		{
			exec_builtin(sparql_conn, hist, pqueries[c].query);
			continue;
		}
		if(!sparql_conn)
		{
			printf("[08003] Not connected to SPARQL database server\n");
			continue;
		}
		l = strlen(pqueries[c].query);
		if(query_is_update(pqueries[c].query, l))
		{
			if(sparql_update(sparql_conn, pqueries[c].query, l))
			{
				printf("[%s] %s\n", sparql_state(sparql_conn), sparql_error(sparql_conn));
				return -1;				
			}
			printf("OK\n");
			return 0;
		}
		rs = sparql_query(sparql_conn, pqueries[c].query, l);
		if(!rs)
		{
			printf("[%s] %s\n", sparql_state(sparql_conn), sparql_error(sparql_conn));
			return -1;
		}
		if(sparqlres_is_boolean(rs))
		{
			if(sparqlres_boolean(rs))
			{
				printf("Result: true\n");
			}
			else
			{
				printf("Result: false\n");
			}
			sparqlres_destroy(rs);
			return 0;
		}
		cols = sparqlres_variables(rs);
		if(!cols)
		{
			printf("Completed.\n");
			sparqlres_destroy(rs);
			return 0;
		}
		if(pqueries[c].output_mode == 'G')
		{
			show_results_long(rs);
		}
		else
		{
			show_results(rs);
		}
		rows = sparqlres_rows(rs);
		if(rows == 1)
		{
			printf("1 row in set.\n");
		}
		else
		{
			printf("%qu rows in set.\n", rows);
		}		
		sparqlres_destroy(rs);
	}
	return 0;
}

int
main(int argc, char **argv)
{
	EditLine *el;
	History *hist;
	HistEvent ev;
	const char *buf;
	int num, state;
	
	check_args(argc, argv);
	if(connect_uri)
	{
		sparql_conn = sparql_create(connect_uri);
		if(!sparql_conn)
		{
			fprintf(stderr, "%s: failed to connect to '%s'\n", short_program_name, connect_uri);
			exit(EXIT_FAILURE);
		}
		sparql_set_logger(sparql_conn, logger);
	}
	fprintf(stderr, "%s interactive SPARQL shell (%s)\n\n", PACKAGE, VERSION);
	fprintf(stderr, 
		"Type:  \\c URI to establish a new connection\n"
		"       \\g or ; to execute query\n"
		"       \\G to execute the query showing results in long format\n"
		"       \\q to end the SPARQL session\n"
		"\n"
		);
	hist = history_init();
	history(hist, &ev, H_SETSIZE, 100);
	el = el_init(argv[0], stdin, stdout, stderr);
	el_set(el, EL_EDITOR, "emacs");
	el_set(el, EL_SIGNAL, 1);
	el_set(el, EL_PROMPT, prompt);
	el_set(el, EL_HIST, history, hist);
	el_source(el, NULL);
	while((buf = el_gets(el, &num)) != NULL && num != 0)
	{
		state = parse_query(buf);
		if(state == 0)
		{
			exec_queries(hist);
		}
	}
	return 0;
}
