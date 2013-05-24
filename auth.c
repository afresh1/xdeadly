/*	$Id: auth.c,v 1.13 2007/08/20 13:25:00 dhartmei Exp $ */

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

static const char rcsid[] = "$Id: auth.c,v 1.13 2007/08/20 13:25:00 dhartmei Exp $";

#include <sys/param.h>
#include <sys/types.h>
#include <errno.h>
#include <sha1.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cgi.h"
#include "user.h"

static char		 domain[MAXPATHLEN];
static char		 logfile[MAXPATHLEN] = "error.log";
static char		 passwd[MAXPATHLEN];
static char		 secret[MAXPATHLEN];

static char	*base64_decode(const char *s, char *d, int size);
static int	 check_pass(const char *user, const char *pass);
static int	 sign(const char *salt, char *hash);
static int	 change_pass(const char *user, const char *pw_new,
		    const char *pw_confirm);
static void	 error(const char *fmt, ...);
void		 msg(const char *user, const char *fmt, ...);

static const char	*host = "-";
static char		 errmsg[256] = "internal auth error";

static char *
base64_decode(const char *s, char *d, int size)
{
	const char *t = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "abcdefghijklmnopqrstuvwxyz0123456789+/";
	const char *v;
	int i, j = 0;
	unsigned u = 0;

	if (strlen(s) % 4) {
		d[0] = 0;
		return (d);
	}
	for (i = 0; s[i]; ++i) {
		u <<= 6;
		if (s[i] != '=') {
			v = strchr(t, s[i]);
			if (v == NULL)
				break;
			u |= (unsigned)(v - t);
		}
		if (i % 4 == 3 && j < size - 4) {
			d[j++] = (u >> 16) & 255;
			d[j++] = (u >> 8) & 255;
			d[j++] = u & 255;
			u = 0;
		}
	}
	d[j] = 0;
	return (d);
}

static int
check_pass(const char *user, const char *pass)
{
	int valid = 0;
	struct user u;
	char hash[256];

	if (!passwd[0]) {
		msg(NULL, "error domain unknown (missing Host: header)");
		return (1);
	}
	memset(&u, 0, sizeof(u));
	strlcpy(u.id, user, sizeof(u.id));
	if (user_find(passwd, &u)) {
		msg(NULL, "error user_find('%s', '%s') failed", passwd, u.id);
		return (1);
	}
	user_hash(pass, hash);
	if (user_is_registered(&u) && !strcmp(hash, u.hash))
		valid = 1;
	return (!valid);
}

static int
sign(const char *salt, char *hash)
{
	FILE *f;
	char s[8192];
	SHA1_CTX ctx;

	if (!secret[0]) {
		msg(NULL, "error domain unknown (missing Host: header)");
		return (1);
	}
	if ((f = fopen(secret, "r")) == NULL) {
		msg(NULL, "error fopen: %s: %s", secret, strerror(errno));
		return (1);
	}
	if (!fgets(s, sizeof(s), f)) {
		msg(NULL, "error fgets: %s: %s", secret, strerror(errno));
		fclose(f);
		return (1);
	}
	fclose(f);
	s[strlen(s) - 1] = 0;
	SHA1Init(&ctx);
	SHA1Update(&ctx, salt, strlen(salt));
	SHA1Update(&ctx, s, strlen(s));
	SHA1End(&ctx, hash);
	return (0);
}

static int
change_pass(const char *user, const char *pw_new, const char *pw_confirm)
{
	struct user u;

	if (!passwd[0]) {
		msg(NULL, "error domain unknown (missing Host: header)");
		return (1);
	}
	if (pw_new == NULL || !pw_new[0]) {
		strlcpy(errmsg, "new password not specified", sizeof(errmsg));
		return (1);
	}
	if (pw_confirm == NULL || strcmp(pw_confirm, pw_new)) {
		strlcpy(errmsg, "new password doesn't match confirmation",
		    sizeof(errmsg));
		return (1);
	}
	memset(&u, 0, sizeof(u));
	strlcpy(u.id, user, sizeof(u.id));
	if (user_find(passwd, &u)) {
		snprintf(errmsg, sizeof(errmsg), "user_find('%s', '%s') failed",
		    passwd, u.id);
		return (1);
	}
	user_hash(pw_new, u.hash);
	if (user_update(passwd, &u)) {
		snprintf(errmsg, sizeof(errmsg), "user_update('%s', '%s') failed",
		    passwd, u.id);
		return (1);
	}
	return (0);
}

int main(void)
{
	int first = 1;
	int post = 0;
	int length = 0;
	char s[8192], user[8192], pass[8192], salt[8192], hash[8192];

	user[0] = pass[0] = salt[0] = hash[0] = 0;
	if ((host = getenv("REMOTE_HOST")) == NULL || !host[0]) {
		host = "-";
		msg(NULL, "error REMOTE_HOST not defined");
		error("getenv: REMOTE_HOST");
	}

	while (fgets(s, sizeof(s), stdin)) {
		while (strlen(s) > 0 && (s[strlen(s) - 1] == '\n' ||
		    s[strlen(s) - 1] == '\r'))
			s[strlen(s) - 1] = 0;
		if (first) {
			first = 0;
			if (!strncmp(s, "POST ", 5))
				post = 1;
		}
		if (!s[0])
			break;
		if (!strncmp(s, "Host: ", 6)) {
			strlcpy(domain, s + 6, sizeof(domain));
			if (strchr(domain, '/')) {
				msg(NULL, "error invalid Host: header");
				domain[0] = 0;
				break;
			}
			snprintf(logfile, sizeof(logfile), "%s.log", domain);
			snprintf(passwd, sizeof(passwd), "%s.passwd", domain);
			snprintf(secret, sizeof(secret), "%s.secret", domain);
		} else if (!strncmp(s, "Authorization: Basic ", 21))  {
			char auth[8192], *p;

			base64_decode(s + 21, auth, sizeof(auth));
			p = strchr(auth, ':');
			if (p == NULL || p == auth || !p[1]) {
				msg(NULL, "error malformed basic auth "
				    "'%s'", auth);
				break;
			}
			*p++ = 0;
			strlcpy(user, auth, sizeof(user));
			strlcpy(pass, p, sizeof(pass));
		} else if (!strncmp(s, "Content-Length: ", 16))
			length = atoi(s + 16);
	}
	if (!user[0] || !pass[0])
		msg(NULL, "missing basic auth");
	else {
		if (check_pass(user, pass))  {
			msg(NULL, "invalid user/password %s:%s", user, pass);
			strlcpy(errmsg, "invalid user/password",
			    sizeof(errmsg));
			user[0] = 0;
		} else {
			msg(user, "valid user/password");
			snprintf(salt, sizeof(salt), "%s/%u/%s", user,
			    time(NULL) + 24 * 60 * 60, host);
			if (sign(salt, hash))
				salt[0] = hash[0] = 0;
		}
	}

	if (!user[0] || !salt[0] || !hash[0]) {
		/* authorization failed (or first try without basic auth) */
		msg(NULL, "access denied: %s", errmsg);
		printf("HTTP/1.0 401 Unauthorized\n");
		printf("WWW-authenticate: basic realm=\"%s\"\n", domain);
		printf("Connection: close\n");
		printf("Content-Type: text/plain\n");
		printf("\n");
		printf("Authorization failed: %s\n", errmsg);
		fflush(stdout);
		return (0);
	}
	/* authorization ok */

	if (post && length > 0) {
		char data[65536];
		size_t r;
		struct query q;
		const char *action;

		if (length > sizeof(data) - 1)
			length = sizeof(data) - 1;
		if ((r = fread(data, 1, length, stdin)) != length)
			error("fread: stdin: %d/%d: %s", r, length,
			    strerror(errno));
		data[r] = 0;
		memset(&q, 0, sizeof(q));
		tokenize_query_params(data, &q.params);
		action = get_query_param(&q, "action");
		if (action == NULL)
			error("invalid POST '%s'", data);
		if (!strcmp(action, "changepw")) {
			if (change_pass(user, get_query_param(&q, "pw_new"),
			    get_query_param(&q, "pw_confirm")))
				error("change_pass: %s", errmsg);
			msg(user, "password changed");
		} else
			error("invalid POST action '%s'", action);
	}

	/* set cookie and redirect to real server */
	printf("HTTP/1.0 302 Redirect\n");
	printf("Set-Cookie: auth=%s/%s; path=/; domain=%s\n",
	    salt, hash, domain);
	printf("Connection: close\n");
	printf("Location: http://%s/\n", domain);
	printf("\n");
	msg(user, "access granted, cookie sent");

	fflush(stdout);
	return (0);
}

static void
error(const char *fmt, ...)
{
	va_list ap;
	char s[8192];

	va_start(ap, fmt);
	vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);
	msg(NULL, "error %s", s);
	printf("HTTP/1.0 200 OK\n");
	printf("Content-Type: text/plain\n");
	printf("Connection: close\n");
	printf("\n");
	printf("auth error: %s\n", s);
	fflush(stdout);
	exit(0);
}

void
msg(const char *user, const char *fmt, ...)
{
	FILE *f;
	va_list ap;
	time_t t = time(NULL);
	struct tm *tm = gmtime(&t);

	if (!logfile[0] || (f = fopen(logfile, "a")) == NULL)
		return;
	fprintf(f, "%4.4d.%2.2d.%2.2d %2.2d:%2.2d:%2.2d %s %s\tauth[%u] ",
	    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec, host,
	    user == NULL || !user[0] ? "-" : user,
	    (unsigned)getpid()); 
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fprintf(f, "\n");
	fflush(f);
	fclose(f);
}
