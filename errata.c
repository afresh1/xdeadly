/*	$Id: errata.c,v 1.4 2008/04/19 10:49:43 dhartmei Exp $ */

/*
 * Copyright (c) 2005 Daniel Hartmeier
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

static const char rcsid[] = "$Id: errata.c,v 1.4 2008/04/19 10:49:43 dhartmei Exp $";

#include <stdio.h>
#include <string.h>

int main(void)
{
	const char *months[12] = { "January", "February", "March", "April",
	    "May", "June", "July", "August", "September", "October",
	    "November", "December" };
	char s[8192], name[64], sev[64], mon[32], tag[32], arch[256],
	    text[8192], url[8192];
	int num, day, year, entry = 0, patch = 0;

	memset(name, 0, sizeof(name));
	while (fgets(s, sizeof(s), stdin) != NULL) {
		s[strlen(s) - 1] = 0;
		if (entry) {
			if (!strcmp(s, "<p>")) {
				int i;

				for (i = 0; i < 12; ++i)
					if (!strcmp(mon, months[i]))
						break;
				printf("%4.4d-%2.2d-%2.2d %d %s %s %s %s\n",
				    year, i + 1, day, num, name, sev, url,
				    text);
				entry = 0;
			} else if (!strncmp(s, "<font color=", 12)) {
				int i;

				memset(sev, 0, sizeof(sev));
				memset(mon, 0, sizeof(mon));
				memset(tag, 0, sizeof(tag));
				memset(arch, 0, sizeof(arch));
				memset(url, 0, sizeof(url));
				sscanf(s, "<font color=\"#009000\"><strong>%d: "
				    "%63[^:]: %31s %d, %d</strong></font> "
				    "&nbsp; <%31[^>]>%255[^<]</%31[^>]><br>",
				    &num, sev, mon, &day, &year, tag, arch,
				    tag);
				for (i = 0; i < strlen(sev) && sev[i] != ' '; i++)
					;
				if (sev[i] == ' ')
					sev[i] = '\0';
				text[0] = 0;
			} else if (!strcmp(s, "<br>")) {
				patch = 1;
			} else if (patch) {
				if (!strncmp(s, "<a href=\"", 9))
					sscanf(s, "<a href=\"%255[^\"]", url);
			} else {
				if (text[0])
					strlcat(text, " ", sizeof(text));
				strlcat(text, s, sizeof(text));
			}
		} else {
			memset(name, 0, sizeof(name));
			if (sscanf(s, "<li><a name=\"%63[^\"]c\"></a>", name) ==
			    1) {
				entry = 1;
				patch = 0;
			}
		}
	}
	return (0);
}
