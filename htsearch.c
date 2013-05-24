/*	$Id: htsearch.c,v 1.7 2006/06/19 17:35:06 dhartmei Exp $ */

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

static const char rcsid[] = "$Id: htsearch.c,v 1.7 2006/06/19 17:35:06 dhartmei Exp $";

#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "htsearch.h"

int
htsearch(const char *dir, const char *conf, const char *words, unsigned page,
    const char *sort, const char *method, struct entry *res, unsigned size)
{
	int fd[2];
	pid_t pid;
	FILE *f;
	int i, matches = 0, status;
	char s[8192], *t;

	memset(res, 0, size * sizeof(*res));
	if (pipe(fd))
		return (-1);
	if ((pid = fork()) < 0)
		return (-1);
	if (pid == 0) {
		char bin[MAXNAMLEN], cfg[MAXNAMLEN], qs[8192];
		char *const envp[] = { NULL };

		close(fd[0]);
		dup2(open("/dev/null", O_RDONLY, 0), STDIN_FILENO);
		dup2(open("/dev/null", O_WRONLY, 0), STDERR_FILENO);
		dup2(fd[1], STDOUT_FILENO);
		snprintf(bin, sizeof(bin), "%s/bin/htsearch", dir);
		snprintf(cfg, sizeof(cfg),
		    "%s/conf/htsearch_%s.conf", dir, conf);
		snprintf(qs, sizeof(qs),
		    "words=%s&page=%u&matchesperpage=%u",
		    words, page, size);
		if (sort != NULL && sort[0])
			snprintf(qs + strlen(qs), sizeof(qs) - strlen(qs),
			    "&sort=%s", sort);
		if (method != NULL && method[0])
			snprintf(qs + strlen(qs), sizeof(qs) - strlen(qs),
			    "&method=%s", method);
		execle(bin, bin, "-c", cfg, qs, (char *)NULL, envp);
		exit(1);
	}
	close(fd[1]);
	f = fdopen(fd[0], "r");
	if (f == NULL)
		return (-1);

	i = -1;
	while ((t = fgets(s, sizeof(s), f)) != NULL ||
	    !waitpid(pid, &status, 0)) {
		int nr, percent;
		char url[1024], excerpt[1024];

		if (strlen(s) > 0)
			s[strlen(s) - 1] = 0;
		if (i == -1) {
			matches = atoi(t);
			i++;
			continue;
		}
		if (sscanf(t, "%d %1023s %d %1023c",
		    &nr, url, &percent, excerpt) != 4)
			continue;
		excerpt[1023] = 0;
		if (i < size && (t = strstr(url, "/data/")) != NULL &&
		    strlen(t + 6) > 14 && t[20] == '/') {
			char *u;

			t += 6;
			strlcpy(res[i].fn, t, sizeof(res[i].fn));
			t[14] = 0;
			strlcpy(res[i].sid, t, sizeof(res[i].sid));
			t += 15;
			u = strrchr(t, '/');
			if (u != NULL && !strcmp(u + 1, "comment")) {
				*u-- = 0;
				while (u > t && *u != '/')
					u--;
				strlcpy(res[i].pid, *u == '/' ? u + 1 : u,
				    sizeof(res[i].pid));
			}
			res[i].score = percent;
			strlcpy(res[i].excerpt, excerpt,
			    sizeof(res[i].excerpt));
			i++;
		}
	}
	fclose(f);
	close(fd[0]);
	if (!WIFEXITED(status) || WEXITSTATUS(status))
		return (-1);
	return (matches);
}
