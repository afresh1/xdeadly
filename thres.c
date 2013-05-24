/*	$Id: thres.c,v 1.5 2006/06/19 17:35:06 dhartmei Exp $ */

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

static const char rcsid[] = "$Id: thres.c,v 1.5 2006/06/19 17:35:06 dhartmei Exp $";

#include <stdlib.h>
#include <string.h>

static double val_u = 0.0;
static double val_d = 0.0;
static double val_s = 0.0;
static double val_c = 0.0;

static int	 eval_val(const char **, double *);
static int	 eval_arg(const char **, double *);
static int	 eval_cond(const char **);
static int	 eval_term(const char **);

enum { COND_OP_EQUAL, COND_OP_UNEQUAL, COND_OP_LESS, COND_OP_LESSEQUAL,
    COND_OP_MORE, COND_OP_MOREEQUAL };

static int
eval_val(const char **s, double *d)
{
	while (**s == ' ')
		(*s)++;
	if (**s == '(') {
		(*s)++;
		if (eval_arg(s, d))
			return (-1);
		while (**s == ' ')
			(*s)++;
		if (**s != ')')
			return (-1);
		(*s)++;
		return (0);
	}
	switch (**s) {
	case 'u':
		*d = val_u;
		(*s)++;
		break;
	case 'd':
		*d = val_d;
		(*s)++;
		break;
	case 's':
		*d = val_s;
		(*s)++;
		break;
	case 'c':
		*d = val_c;
		(*s)++;
		break;
	default: {
		char *t;

		*d = strtod(*s, &t);
		if (t == *s)
			return (-1);
		*s = t;
	}
	}
	return (0);
}

static int
eval_arg(const char **s, double *d)
{
	if (eval_val(s, d))
		return (-1);
	while (**s == ' ')
		(*s)++;
	while (**s == '+' || **s == '-' || **s == '*' || **s == '/') {
		char op = *((*s)++);
		double e;

		if (eval_val(s, &e))
			return (-1);
		switch (op) {
		case '+':
			*d += e;
			break;
		case '-':
			*d -= e;
			break;
		case '*':
			*d *= e;
			break;
		case '/':
			if (e == 0.0)
				*d = 0.0;
			else
				*d /= e;
			break;
		}
		while (**s == ' ')
			(*s)++;
	}
	return (0);
}

static int
eval_cond(const char **s)
{
	double a, b;
	int op = -1;

	while (**s == ' ')
		(*s)++;
	if (eval_arg(s, &a))
		return (-1);
	if (!**s)
		return (-1);
	if (!strncmp(*s, "<=", 2)) {
		op = COND_OP_LESSEQUAL;
		*s += 2;
	} else if (!strncmp(*s, ">=", 2)) {
		op = COND_OP_MOREEQUAL;
		*s += 2;
	} else if (!strncmp(*s, "!=", 2)) {
		op = COND_OP_UNEQUAL;
		*s += 2;
	} else if (**s == '<') {
		op = COND_OP_LESS;
		(*s)++;
	} else if (**s == '>') {
		op = COND_OP_MORE;
		(*s)++;
	} else if (**s == '=') {
		op = COND_OP_EQUAL;
		(*s)++;
	} else
		return (-1);
	if (eval_arg(s, &b))
		return (-1);
	switch (op) {
	case COND_OP_EQUAL:
		return (a == b);
	case COND_OP_UNEQUAL:
		return (a != b);
	case COND_OP_LESS:
		return (a < b);
	case COND_OP_LESSEQUAL:
		return (a <= b);
	case COND_OP_MORE:
		return (a > b);
	case COND_OP_MOREEQUAL:
		return (a >= b);
	default:
		return (-1);
	}
}

static int
eval_term(const char **s)
{
	int r;

	while (**s == ' ')
		(*s)++;
	if ((r = eval_cond(s)) < 0)
		return (-1);
	while (**s == ' ')
		(*s)++;
	while (**s == '|' || **s == '&') {
		char op = *((*s)++);
		int q;

		switch (op) {
		case '|':
			break;
		case '&':
			break;
		default:
			return (-1);
		}
		while (**s == ' ')
			(*s)++;
		if ((q = eval_cond(s)) < 0)
			return (-1);
		r = (op == '|' ? r || q : r && q);
	}
	return (r);
}

int
thres_eval(const char *thres, int modsum, int modcount)
{
	char *e;
	long l;
	int moddown;

	if (thres == NULL || !*thres)
		return (0);
	l = strtol(thres, &e, 10);
	if (!*e)
		return (modsum >= l);
	moddown = (modcount - modsum) / 2;
	val_c = modcount;
	val_s = modsum;
	val_u = modcount - moddown;
	val_d = moddown;
	return (eval_term(&thres));
}
