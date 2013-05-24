/*	$Id: cgi.c,v 1.7 2006/05/24 18:20:37 dhartmei Exp $ */

/*
 * Copyright (c) 2004 Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

static const char rcsid[] = "$Id: cgi.c,v 1.7 2006/05/24 18:20:37 dhartmei Exp $";

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cgi.h"

struct query_param {
	char			*key;
	char			*val;
	struct query_param	*next;
};

/*
 * decode s to d, writing at most l characters
 * d may be NULL, in which case only the return value is defined
 * guarantees d's nul-termination (truncating, if necessary)
 * returns the number of characters required (including nul byte),
 * which may be larger than l
 */
unsigned
url_decode(const char *s, char *d, unsigned l)
{
	static const char *hex = "0123456789abcdef";
	unsigned used = 1;

	while (*s) {
		if (*s == '%') {
			char *i;

			s++;
			if (*s && (i = strchr(hex, tolower(*s))) != NULL) {
				if (d != NULL && used < l)
					*d = (i - hex) * 16;
				s++;
				if (*s && (i = strchr(hex, tolower(*s))) !=
				    NULL) {
					if (d != NULL && used + 1 < l)
						*d++ += i - hex;
					used++;
					s++;
				}
			}
		} else {
			if (d != NULL && used < l)
				*d++ = *s == '+' ? ' ' : *s;
			used++;
			s++;
		}
	}
	if (d != NULL)
		*d = 0;
	return (used);
}

/*
 * encode s to d, writing at most l characters
 * d may be NULL, in which case only the return value is defined
 * guarantees d's nul-termination (truncating, if necessary)
 * returns the number of characters required (including nul byte),
 * may be larger than l
 */
unsigned
url_encode(const char *s, char *d, size_t l)
{
	static const char *res = "%;/?:@&=+$";
	static const char *hex = "0123456789abcdef";
	unsigned used = 1;

	while (*s) {
		if (*s == ' ') {
			if (d != NULL && used < l)
				*d++ = '+';
			used++;
		} else if (!isalnum(*s) || strchr(res, *s) != NULL) {
			if (d != NULL && used + 3 < l) {
				*d++ = '%';
				*d++ = hex[*s / 16];
				*d++ = hex[*s % 16];
			}
			used += 3;
		} else {
			if (d != NULL && used < l)
				*d++ = *s;
			used++;
		}
		s++;
	}
	if (d != NULL)
		*d = 0;
	return (used);
}

void
tokenize_query_params(char *q, struct query_param **p)
{
	char *r, *s;
	char t;
	unsigned l;
	struct query_param *n;

	while (*q) {
		if (*q == '&' || *q == ';') {
			q++;
			continue;
		}
		s = NULL;
		for (r = q + 1; *r; ++r) {
			if (*r == '&' || *r == ';')
				break;
			if (*r == '=' && s == NULL)
				s = r;
		}
		if (s == NULL)
			break;
		if ((n = malloc(sizeof(*n))) == NULL)
			break;
		t = *s;
		*s = 0;
		l = url_decode(q, NULL, 0);
		if ((n->key = malloc(l + 1)) == NULL) {
			free(n);
			break;
		}
		url_decode(q, n->key, l + 1);
		*s++ = t;
		t = *r;
		*r = 0;
		l = url_decode(s, NULL, 0);
		if ((n->val = malloc(l + 1)) == NULL) {
			free(n->key);
			free(n);
			break;
		}
		url_decode(s, n->val, l + 1);
		n->next = *p;
		*p = n;
		*r = t;
		q = r;
	}
}

struct query *
get_query(void)
{
	struct query *q;
	const char *e;

	q = malloc(sizeof(*q));
	if (q == NULL)
		return (NULL);
	memset(q, 0, sizeof(*q));
	if ((e = getenv("REQUEST_METHOD")) != NULL)
		q->request_method = strdup(e);
	if ((e = getenv("QUERY_STRING")) != NULL) {
		q->query_string = strdup(e);
		tokenize_query_params(q->query_string, &q->params);
	}
	if ((e = getenv("REMOTE_ADDR")) != NULL)
		q->remote_addr = strdup(e);
	if ((e = getenv("HTTP_USER_AGENT")) != NULL)
		q->user_agent = strdup(e);
	if ((e = getenv("HTTP_REFERER")) != NULL)
		q->referer = strdup(e);
	return (q);
}

void
free_query(struct query *q)
{
	struct query_param *p;

	if (q->request_method != NULL)
		free(q->request_method);
	if (q->query_string != NULL)
		free(q->query_string);
	if (q->remote_addr != NULL)
		free(q->remote_addr);
	if (q->user_agent != NULL)
		free(q->user_agent);
	if (q->referer != NULL)
		free(q->referer);
	while (q->params != NULL) {
		p = q->params;
		q->params = q->params->next;
		free(p->key);
		free(p->val);
		free(p);
		
	}
	free(q);
}

const char *
get_query_param(const struct query *q, const char *key)
{
	const struct query_param *p;

	for (p = q->params; p != NULL; p = p->next)
		if (!strcasecmp(p->key, key))
			return (p->val);
	return (NULL);
}
