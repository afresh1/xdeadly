/*	$Id: mail.c,v 1.1 2007/01/11 15:40:10 dhartmei Exp $ */

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
#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mail.h"

extern void	 msg(const char *fmt, ...);

int
mail_send(const char *body)
{
	char *argv[] = { "/undeadly.org/mini_sendmail", "-t", NULL };
	int fd[2], r, status;
	pid_t pid;
	FILE *f;
	time_t t;

	if (pipe(fd)) {
		msg("error mail_send: pipe: %s", strerror(errno));
		return (1);
	}
	if ((pid = fork()) < 0) {
		msg("error mail_send: fork: %s", strerror(errno));
		close(fd[0]);
		close(fd[1]);
		return (1);
	}
	if (!pid) {
		close(fd[1]);
		dup2(fd[0], STDIN_FILENO);
		dup2(open("/dev/null", O_WRONLY, 0), STDOUT_FILENO);
		dup2(open("/dev/null", O_WRONLY, 0), STDERR_FILENO);
		execv(argv[0], argv);
		exit(EXIT_FAILURE);
	}
	close(fd[0]);
	if (!(f = fdopen(fd[1], "w"))) {
		msg("error mail_send: fdopen: %s", strerror(errno));
		close(fd[1]);
		return (1);
	}
	fprintf(f, "%s\n", body);
	fflush(f);
	fclose(f);
	t = time(NULL) + 10;
	do {
		usleep(10000);
		r = waitpid(pid, &status, WNOHANG);
		if (r < 0) {
			if (errno != EINTR && errno != EAGAIN)
				msg("error mail_send: waitpid: %s",
				    strerror(errno));
			else
				r = 0;
		}
	} while (!r && time(NULL) < t);
	if (!r) {
		msg("error mail_send: timeout waiting for child %u",
		    (unsigned)pid);
		return (1);
	}
	if (r != pid) {
		msg("error mail_send: waitpid returned %u for child %u",
		    (unsigned)r, (unsigned)pid);
		return (1);
	}
	if (!WIFEXITED(status)) {
		msg("error mail_send: abnormal exit of chhild %u",
		    (unsigned)pid);
		return (1);
	}
	if (WEXITSTATUS(status)) {
		msg("error mail_send: exit status %d from child %u",
		    (int)WEXITSTATUS(status), (unsigned)pid);
		return (1);
	}
	return (0);
}
