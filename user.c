/*	$Id: user.c,v 1.1 2007/01/11 15:40:10 dhartmei Exp $ */

/*
 * Copyright (c) 2004-2006 Daniel Hartmeier
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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sha1.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "user.h"

extern void	 msg(const char *fmt, ...);

static FILE *
user_file_open(const char *filename, int writable)
{
	int fd = -1, pass = 0;
	FILE *f;

	while (fd < 0 && ++pass <= 20) {
		fd = open(filename, (writable ? O_RDWR : O_RDONLY) | O_EXLOCK |
		    O_NONBLOCK | O_SYNC,  0);
		if (fd >= 0)
			break;
		if (errno != EWOULDBLOCK) {
			msg("error user_file_open: %s: %s", filename, strerror(errno));
			return (NULL);
		}
		msg("warning user_file_open: %s: waiting for lock (pass %d)", filename, pass);
		usleep(500000);
	}
	if (!(f = fdopen(fd, writable ? "rw" : "r"))) {
		msg("error user_file_open: fdopen: %s", strerror(errno));
		close(fd);
	}
	return (f);
}

#define SEPERATOR ';'

static void
user_token(char **c, char *buf, int len)
{
	int i = 0;

	while (**c && **c != SEPERATOR && i + 1 < len)
		buf[i++] = *(*c)++;
	if (**c == SEPERATOR)
		(*c)++;
	buf[i] = 0;
}


static const char *secret = "lw1N0mN0t8a24UR4NY12AT55p/23FxJ9H6+NReXed8nTZJraO0g";

int
user_is_registered(const struct user *u)
{
	return (!strcmp(u->group, "user") || user_is_editor(u));
}

int
user_is_editor(const struct user *u)
{
	return (!strcmp(u->group, "editor") || user_is_admin(u));
}

int
user_is_admin(const struct user *u)
{
	return (!strcmp(u->group, "admin"));
}

void
user_hash(const char *password, char *hash)
{
	SHA1_CTX ctx;

	SHA1Init(&ctx);
	SHA1Update(&ctx, secret, strlen(secret));
	SHA1Update(&ctx, password, strlen(password));
	SHA1End(&ctx, hash);
}

int
user_find(const char *filename, struct user *u)
{
	FILE *f;
	char s[8192], id[32], *p;
	int i;

	if (!filename || !u)
		return (1);
	if (!(f = user_file_open(filename, 0)))
		return (NULL);
	while (fgets(s, sizeof(s), f)) {
		i = strlen(s);
		if (i > 0 && s[i - 1] == '\n')
			s[i - 1] = 0;
		p = s;
		user_token(&p, id, sizeof(id));
		if (!strcasecmp(id, u->id)) {
			user_token(&p, u->group, sizeof(u->group));
			user_token(&p, u->hash, sizeof(u->hash));
			user_token(&p, u->name, sizeof(u->name));
			user_token(&p, u->email, sizeof(u->email));
			user_token(&p, u->href, sizeof(u->href));
			fclose(f);
			return (0);
		}
	}
	fclose(f);
	return (1);
}

int
user_add(const char *filename, const struct user *u)
{
	FILE *f, *g;
	char fn[8192], s[8192], id[32], *p;
	int fd;

	if (!(f = user_file_open(filename, 1)))
		return (1);
	snprintf(fn, sizeof(fn), "%s.tmp.XXXXXXXXXX", filename);
	if ((fd = mkstemp(fn)) == -1) {
		msg("error user_add: mkstemp: %s: %s", fn, strerror(errno));
		fclose(f);
		return (1);
	}
	if (fchmod(fd, 0660)) {
		msg("error user_add: fchmod: %s: %s", fn, strerror(errno));
		fclose(f);
		return (1);
	}
	if (!(g = fdopen(fd, "w+"))) {
		msg("error user_add: fdopen: %s", strerror(errno));
		fclose(f);
		return (1);
	}
	while (fgets(s, sizeof(s), f)) {
		p = s;
		user_token(&p, id, sizeof(id));
		if (!strcasecmp(id, u->id)) {
			msg("error user_add: id '%s' already exists", id);
			fclose(f);
			unlink(fn);
			fclose(g);
			return (1);
		}
		fputs(s, g);
	}
	fprintf(g, "%s;%s;%s;%s;%s;%s\n", u->id, u->group,
	    u->hash, u->name, u->email, u->href);
	fflush(g);
	fclose(g);
	if (rename(fn, filename)) {
		msg("error user_add: rename: %s: %s: %s", fn, filename, strerror(errno));
		return (1);
	}
	fclose(f);
	return (0);
}

int
user_update(const char *filename, const struct user *u)
{
	FILE *f, *g;
	char fn[8192], s[8192], id[32], *p;
	int fd, found = 0;

	if (!(f = user_file_open(filename, 1)))
		return (1);
	snprintf(fn, sizeof(fn), "%s.tmp.XXXXXXXXXX", filename);
	if ((fd = mkstemp(fn)) == -1) {
		msg("error user_update: mkstemp: %s: %s", fn, strerror(errno));
		fclose(f);
		return (1);
	}
	if (fchmod(fd, 0660)) {
		msg("error user_update: fchmod: %s: %s", fn, strerror(errno));
		fclose(f);
		return (1);
	}
	if (!(g = fdopen(fd, "w+"))) {
		msg("error user_update: fdopen: %s", strerror(errno));
		fclose(f);
		return (1);
	}
	while (fgets(s, sizeof(s), f)) {
		p = s;
		user_token(&p, id, sizeof(id));
		if (strcasecmp(id, u->id))
			fputs(s, g);
		else if (!found) {
			fprintf(g, "%s;%s;%s;%s;%s;%s\n", u->id, u->group,
			    u->hash, u->name, u->email, u->href);
			found = 1;
		}
	}
	if (!found) {
		msg("error user_update: id '%s' does not exist", u->id);
		fclose(f);
		unlink(fn);
		fclose(g);
		return (1);
	}
	fflush(g);
	fclose(g);
	if (rename(fn, filename)) {
		msg("error user_update: rename: %s: %s: %s", fn, filename, strerror(errno));
		return (1);
	}
	fclose(f);
	return (0);
}
