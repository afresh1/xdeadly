/*	$Id: vuxml.c,v 1.5 2006/06/19 17:35:06 dhartmei Exp $ */

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

static const char rcsid[] = "$Id: vuxml.c,v 1.5 2006/06/19 17:35:06 dhartmei Exp $";

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct tag {
	unsigned char	*name;
	unsigned char	*options;
	struct tag	*parent;
	struct tag	*head;
	struct tag	*next;
	struct tag	*tail;
};

struct tag *root = NULL, *parent = NULL;

static void
traverse(struct tag *t)
{
	static char vid[128], date[128], topic[128];
	static int state = 0;
	struct tag *u;

	switch (state) {
	case 0:
		if (!strcmp(t->name, "VULN") &&
		    !strncmp(t->options, "vid=\"", 5)) {
			strlcpy(vid, t->options + 5, sizeof(vid));
			if (strlen(vid) > 0 && vid[strlen(vid) - 1] == '\"')
				vid[strlen(vid) - 1] = 0;
			state = 1;
		}
		break;
	case 1:
		if (!strcmp(t->name, "TOPIC"))
			state = 2;
		break;
	case 2:
		strlcpy(topic, t->options, sizeof(topic));
		state = 3;
		break;
	case 3:
		if (!strcmp(t->name, "DATES"))
			state = 4;
		break;
	case 4:
		if (!strcmp(t->name, "ENTRY"))
			state = 5;
		break;
	case 5:
		strlcpy(date, t->options, sizeof(date));
		printf("%s %s %s\n", date, vid, topic);
		state = 0;
		break;
	}
	for (u = t->head; u; u = u->next)
		traverse(u);
}

static unsigned char
get(FILE *f)
{
	static unsigned char buf[8192];
	static unsigned high = 0, next = 0;

	if (next >= high) {
		next = 0;
		high = fread(buf, 1, sizeof(buf), f);
		if (high < 0) {
			perror("fread() failed");
			exit(1);
		} else if (high == 0)
			return (0);
	}
	return buf[next++];
}

static void
opentag(FILE *f, unsigned char c)
{
	unsigned char name[65535];
	unsigned char opts[65535];
	unsigned i;
	struct tag *tag;

	while (c == ' ' || c == '\t' || c == '\n')
		c = get(f);
	if (!c || c == '>')
		return;
	for (i = 0; c && c != ' ' && c != '>'; c = get(f))
		name[i++] = toupper(c);
	name[i] = 0;
	i = 0;
	if (c == ' ')
		c = get(f);
	for (i = 0; c && c != '>'; c = get(f)) {
		if (c == '\t' || c == '\n')
			c = ' ';
		opts[i++] = c;
	}
	opts[i] = 0;

	tag = (struct tag *)malloc(sizeof(*tag));
	if (tag == NULL) {
		perror("malloc() failed");
		exit(1);
	}
	tag->name = strdup(name);
	if (tag->name == NULL) {
		perror("strdup() failed");
		exit(1);
	}
	tag->options = strdup(opts);
	if (tag->options == NULL) {
		perror("strdup() failed");
		exit(1);
	}
	tag->parent = parent;
	tag->head = tag->next = tag->tail = NULL;
	if (parent->tail == NULL)
		parent->head = parent->tail = tag;
	else {
		parent->tail->next = tag;
		parent->tail = tag;
	}
	if (strcmp(name, "BR") && strcmp(name, "P"))
		parent = tag;
}

static void
closetag(FILE *f, unsigned char c)
{
	unsigned char name[65535];
	unsigned i;
	struct tag *old_parent;

	while (c == ' ' || c == '\t' || c == '\n')
		c = get(f);
	if (!c || c == '>')
		return;
	for (i = 0; c && c != '>'; c = get(f))
		name[i++] = toupper(c);
	name[i] = 0;

	if (!strcmp(name, "BR") || !strcmp(name, "P"))
		return;

	old_parent = parent;
	while (parent != NULL && parent->parent != NULL &&
	    strcmp(name, parent->name))
		parent = parent->parent;
	if (parent == NULL || parent->parent == NULL) {
		fprintf(stderr, "stack underflow (%s)\n", name);
		parent = old_parent;
	} else if (strcmp(name, parent->name)) 
		fprintf(stderr, "non-matching closing tag (%s)\n", name);
	else
		parent = parent->parent;
}

static int
empty(const char *s)
{
	while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
		s++;
	return (!*s);
}

static void
texttag(FILE *f, unsigned char c)
{
	unsigned char text[65535];
	unsigned i;
	struct tag *tag;

	for (i = 0; c && c != '<'; c = get(f))
		text[i++] = c;
	text[i] = 0;
	if (empty(text))
		return;

	tag = (struct tag *)malloc(sizeof(*tag));
	if (tag == NULL) {
		perror("malloc() failed");
		exit(1);
	}
	tag->name = strdup("");
	if (tag->name == NULL) {
		perror("strdup() failed");
		exit(1);
	}
	tag->options = strdup(text);
	if (tag->options == NULL) {
		perror("strdup() failed");
		exit(1);
	}
	tag->parent = parent;
	tag->head = tag->next = tag->tail = NULL;
	if (parent->tail == NULL)
		parent->head = parent->tail = tag;
	else {
		parent->tail->next = tag;
		parent->tail = tag;
	}
}

static struct tag *
find_tag(struct tag *t, const char *name)
{
	struct tag *h;

	if (!strcmp(t->name, name))
		return (t);
	for (t = t->head; t; t = t->next) {
		h = find_tag(t, name);
		if (h)
			return (h);
	}
	return (NULL);
}

static void
read_file(const char *filename)
{
	FILE *f;
	unsigned char c;

	if (!strcmp(filename, "-"))
		f = stdin;
	else
		f = fopen(filename, "rb");
	if (f == NULL) {
		fprintf(stderr, "fopen: %s: %s\n", filename, strerror(errno));
		return;
	}

	root = malloc(sizeof(*root));
	if (root == NULL) {
		fprintf(stderr, "malloc: %s\n", strerror(errno));
		fclose(f);
		return;
	}

	root->name = strdup("root");
	root->options = strdup("");
	root->parent = NULL;
	root->head = root->next = root->tail = NULL;
	parent = root;

	while ((c = get(f))) {
		if (c != '<')
			texttag(f, c);
		if (!c)
			break;
		/* c == '<' */
		c = get(f);
		if (c == '!' || c == '?') {
			while ((c = get(f)) != '>')
				;
		} else if (c == '/') {
			c = get(f);
			closetag(f, c);
		} else
			opentag(f, c);
	}
	if (ferror(f)) {
		fprintf(stderr, "ferror: %s\n", strerror(errno));
		free(root);
		root = NULL;
	}
	fclose(f);
}

int
main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "%s <file>\n", argv[0]);
		return (1);
	}

	read_file(argv[1]);
	if (root == NULL) {
		fprintf(stderr, "read_file() failed, aborting\n");
		return (1);
	}

	if (root->head)
		traverse(root->head);

	return (0);
}
