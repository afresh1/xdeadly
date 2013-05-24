/*	$Id: captcha.c,v 1.4 2006/09/26 21:39:12 dhartmei Exp $ */

/*
 * Copyright (c) 2006 Daniel Hartmeier
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

static const char rcsid[] = "$Id: captcha.c,v 1.4 2006/09/26 21:39:12 dhartmei Exp $";

#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <sha1.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void	 msg(const char *fmt, ...);

static const char *secret =
    "DhmnjLRtarhZOybYSGecYP9vX5sNKg3AS6/Es+Ha8VC6A7CbYnS1pQ";

int
verify_captcha(unsigned char *reply, unsigned char *sum)
{
	SHA1_CTX ctx;
	char s[80], hash[80], *p;
	time_t t;

	strlcpy(s, sum, sizeof(s));
	if ((p = strchr(s, ':')) == NULL) {
		msg("warning verify_captcha: no colon in sum '%s', "
		    "reply '%s'", sum, reply);
		return (1);
	}
	*p++ = 0;
	SHA1Init(&ctx);
	SHA1Update(&ctx, s, strlen(s));
	SHA1Update(&ctx, reply, strlen(reply));
	SHA1Update(&ctx, secret, strlen(secret));
	SHA1End(&ctx, hash);
	if (strcmp(hash, p)) {
		msg("warning verify_captcha: sum '%s' "
		    "mismatches reply '%s'", sum, reply);
		return (1);
	}
	t = (time_t)strtoul(s, NULL, 10);
	if (t < (time(NULL) - 15 * 60) || t >= (time(NULL) - 5)) {
		msg("warning verify_captcha: sum '%s' violates time %u, "
		    "reply '%s'", sum, (unsigned)time(NULL), reply);
		return (1);
	}
	msg("verify_captcha: sum '%s', time %u, reply '%s' valid",
	    sum, (unsigned)time(NULL), reply);
	return (0);
}

int
generate_captcha(unsigned char *buf, unsigned buflen, unsigned char *sum,
    unsigned sumlen)
{
	const unsigned char *a = "abcdefghijkmnpqrstuvwxyz23456789";
	unsigned char w[80], ts[16], h[80], s[256];
	int i;
	SHA1_CTX ctx;
	int fd[2], r, status;
	pid_t pid;
	char argv0[128] = "/undeadly.org/figlet", argv1[128] = "-f", argv2[128] = "big", argv3[128];
	char *argv[] = { argv0, argv1, argv2, argv3, NULL };
	FILE *f;
	time_t t;

	for (i = 0; i < 8; ++i)
		w[i] = a[arc4random() % strlen(a)];
	w[i] = 0;
	snprintf(ts, sizeof(ts), "%u", (unsigned)time(NULL));

	SHA1Init(&ctx);
	SHA1Update(&ctx, ts, strlen(ts));
	SHA1Update(&ctx, w, strlen(w));
	SHA1Update(&ctx, secret, strlen(secret));
	SHA1End(&ctx, h);
	snprintf(sum, sumlen, "%s:%s", ts, h);

	if (pipe(fd)) {
		msg("error: generate_captcha: pipe: %s", strerror(errno));
		return (1);
	}
	if ((pid = fork()) < 0) {
		msg("error: generate_captcha: fork: %s", strerror(errno));
		close(fd[0]);
		close(fd[1]);
		return (1);
	}
	if (!pid) {
		close(fd[0]);
		dup2(fd[1], STDOUT_FILENO);
		dup2(open("/dev/null", O_WRONLY, 0), STDIN_FILENO);
		dup2(open("/dev/null", O_WRONLY, 0), STDERR_FILENO);
		strlcpy(argv3, w, sizeof(argv3));
		execv(argv[0], argv);
		exit(1);
	}
	close(fd[1]);
	if (!(f = fdopen(fd[0], "r"))) {
		msg("error: generate_captcha: fdopen: %s", strerror(errno));
		close(fd[0]);
		return (1);
	}
	buf[0] = 0;
	while (fgets(s, sizeof(s), f) != NULL)
		strlcat(buf, s, buflen);
	fclose(f);
	t = time(0) + 5;
	do {
		usleep(10000);
		r = waitpid(pid, &status, WNOHANG);
		if (r < 0) {
			if (errno != EINTR && errno != EAGAIN)
				msg("error: generate_captcha: waitpid: %s",
				    strerror(errno));
			else
				r = 0;
		}
	} while (!r && time(0) < t);
	if (!r) {
		msg("error: generate_captcha: timeout waitpid %u",
		    (unsigned)pid);
		return (1);
	}
	if (r != pid) {
		msg("error: generate_captcha: waitpid %u != %u",
		    (unsigned)r, (unsigned)pid);
		return (1);
	}
	if (!WIFEXITED(status)) {
		msg("error: generate_captcha: child abnormal exit");
		return (1);
	}
	if (WEXITSTATUS(status)) {
		msg("error: generate_captcha: child exit status %d",
		    (int)WEXITSTATUS(status));
		return (1);
	}
	return (0);
}
