/*	$Id: conf.c,v 1.4 2004/08/21 13:57:28 dhartmei Exp $ */

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

static const char rcsid[] = "$Id: conf.c,v 1.4 2004/08/21 13:57:28 dhartmei Exp $";

#include <sys/file.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "conf.h"

int
load_conf(const char *fn, struct conf *c)
{
	FILE *f;
	char s[8192], *t, k[128], *v;
	int i;

	if ((f = fopen(fn, "r")) == NULL)
		return (1);
	while ((t = fgets(s, sizeof(s), f))) {
		while (*t == ' ' || *t == '\t')
			t++;
		for (i = 0; i < sizeof(k) - 1 && *t && *t != ' ' && *t != '\t';
		    ++i)
			k[i] = *t++;
		k[i] = 0;
		if (!k[0] || k[0] == '\n' || k[0] == '#')
			continue;
		for (i = 0; c[i].n != NULL && strcmp(c[i].n, k); ++i)
			;
		if (c[i].n == NULL)
			continue;
		v = c[i].v;
		while (*t == ' ' || *t == '\t')
			t++;
		for (i = 0; i < MAXPATHLEN - 1 && *t && *t != '\n'; ++i)
			v[i] = *t++;
	}
	fclose(f);
	return (0);
}
