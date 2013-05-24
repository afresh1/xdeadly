/*	$Id: undeadly.c,v 1.49 2008/06/19 12:49:31 dhartmei Exp $ */

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

static const char rcsid[] = "$Id: undeadly.c,v 1.49 2008/06/19 12:49:31 dhartmei Exp $";

#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <fts.h>
#include <sha1.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "captcha.h"
#include "conf.h"
#include "cgi.h"
#include "htsearch.h"
#include "thres.h"
#include "user.h"
#include "mail.h"

static const char	*conffile = "cgi.conf";
static const char	*corefile = "cgi.core";
static char		 domain[MAXPATHLEN];
static char		 datadir[MAXPATHLEN];
static char		 htmldir[MAXPATHLEN];
static char		 htdigdir[MAXPATHLEN];
static char		 passwd[MAXPATHLEN];
static char		 secret[MAXPATHLEN];
static char		 logfile[MAXPATHLEN];
static char		 baseurl[MAXPATHLEN];
static char		 authurl[MAXPATHLEN];
static char		 ct_html[MAXPATHLEN];
static char		 vuxmlfile[MAXPATHLEN];
static char		 erratafile[MAXPATHLEN];
static char		 nopostfile[MAXPATHLEN];
static char		 mailaddr[MAXPATHLEN];
struct conf confentries[] = {
	{ "domain", domain },
	{ "datadir", datadir },
	{ "htmldir", htmldir },
	{ "htdigdir", htdigdir },
	{ "passwd", passwd },
	{ "secret", secret },
	{ "logfile", logfile },
	{ "baseurl", baseurl },
	{ "authurl", authurl },
	{ "ct_html", ct_html },
	{ "vuxmlfile", vuxmlfile },
	{ "erratafile", erratafile },
	{ "nopostfile", nopostfile },
	{ "mailaddr", mailaddr },
	{ NULL, NULL }
};
static struct query	*q = NULL;
static struct entry	 newest[64];	/* 9 front, 9 weeklist */
static struct entry	 poll;
static const char	*mode = NULL;
static const char	*thres = NULL;
static const char	*invalidposterr = "Posted form data is invalid. This "
			    "is likely a problem with the data you were "
			    "trying to post, not a server error. "
			    "If you're trying to post HTML, please verify "
			    "that no unacceptable HTML tags are used (the "
			    "list of valid tags is found at the bottom of "
			    "the submission page) and that there are no "
			    "closing tags missing. "
			    "You can go back to the form to edit and re-"
			    "submit. If the problem persists for a valid "
			    "HTML post, please include a copy of the data "
			    "in any bug report.";
static gzFile		 gz = NULL;
static unsigned char captcha_text[4096] = "", captcha_cksum[128] = "";

static struct user user;
static time_t tlastread;

#define	FLAG_FRONT	0x0000001
#define FLAG_FLAT	0x0000002
#define FLAG_EXPANDED	0x0000004
#define FLAG_SUBMISSION	0x0000008
#define FLAG_EDIT	0x0000010
#define FLAG_PLAIN	0x0000020
#define FLAG_PREFS	0x0000040
#define FLAG_POLL	0x0000080

typedef	void (*render_cb)(const char *, const struct entry *);

static char	*url(int esc, const char *action, const char *sid,
		    const char *pid, const char *opts, ...);
static void	 render_error(const char *fmt, ...);
static void	 render_redirect(const char *fmt, ...);
static int	 render_html(const char *html_fn, render_cb r,
		    const struct entry *e);
static void	 render_rss(const char *m, const struct entry *e);
static void	 render_rss_item(const char *m, const struct entry *e);
static void	 render_rss_errata(const char *m, const struct entry *e);
static void	 render_rss_errata_item(const char *m, const struct entry *e);
static void	 render_front(const char *m, const struct entry *e);
static void	 render_front_story(const char *m, const struct entry *e);
static void	 render_front_box(const char *m, const struct entry *e);
static void	 render_front_weeklistentry(const char *m,
		    const struct entry *e);
static void	 render_article(const char *m, const struct entry *e);
static void	 render_article_comment(const char *m, const struct entry *e);
static void	 render_submit(const char *m, const struct entry *e);
static void	 render_register(const char *m, const struct entry *e);
static void	 render_preview(const char *m, const struct entry *e);
static void	 render_submit_form(const char *m, const struct entry *e);
static void	 render_prefs_form(const char *m, const struct entry *e);
static void	 render_searchresults(const char *m, const struct entry *e);
static void	 render_searchresults_match(const char *m,
		    const struct entry *e);
static void	 find_articles(const char *path, unsigned long long date,
		    struct entry *a, int size, int polls, struct entry *p);
static void	 find_article_adjacent(const char *sid, struct entry *prev,
		    struct entry *next);
static void	 find_comments(const char *sid, const char *pid,
		    struct entry *a, int size);
static void	 find_parent(const char *sid, const char *pid,
		    struct entry *e);
static void	 find_comment_adjacent(const char *sid, const char *pid,
		    struct entry *prev, struct entry *next);
static int	 read_file(struct entry *e);
static unsigned	 count_comments(const char *sid);
static int	 get_lastread(const char *sid, time_t *t, int update);
static time_t	 find_newest_change(const char *sid);
static unsigned	 find_max_pid(const char *sid);
static int	 find_pid_path(const char *sid, const char *pid,
		    char *path, int size);
static int	 create_article(struct entry *ce);
static int	 create_comment(struct entry *ce);
static int	 moderate_comment(const char *sid, const char *pid, int val);
static int	 delete_comment(const char *sid, const char *pid);
static char	*de_html(const char *s, char *d, size_t len);
static char	*html_esc(const char *s, char *d, size_t len, int allownl);
static int	 html_validate(const char *d);
static int	 compare_name_asc_fts(const FTSENT **a, const FTSENT **b);
static int	 compare_name_des_fts(const FTSENT **a, const FTSENT **b);
static int	 compare_fts(const FTSENT **a, const FTSENT **b);
static int	 compare_qsort(const void *va, const void *vb);
static time_t	 convert_time(const char *date);
static const char *rfc822_time(time_t t);
static const char *duration_str(unsigned d);
static int	 read_post_data(char *buf, unsigned siz);
static int	 read_post(struct entry *e, int *preview);
static void	 read_poll(const char *sid, int *a, int siz, int *sum);
static int	 vote_poll(const char *sid, int val);
static int	 rss_rate_limit(void);
static int	 nopost_check(void);
static void	 check_auth(void);
void		 msg(const char *fmt, ...);

static unsigned long long
timenow(time_t t)
{
	struct tm *tm;

	if (t <= 0)
		t = time(NULL);
	tm = gmtime(&t);
	if (!tm) {
		msg("error timenow: gmtime: %s", strerror(errno));
		return (0);
	}
	return (tm->tm_sec + tm->tm_min * 100 + tm->tm_hour * 10000 +
	    tm->tm_mday * 1000000 + (tm->tm_mon + 1) * 100000000 +
	    (tm->tm_year + 1900) * 10000000000);
}

static double
timelapse(struct timeval *t)
{
	struct timeval u;
	double d;

	gettimeofday(&u, NULL);
	d = (double)((u.tv_sec * 1000000 + u.tv_usec) -
	    (t->tv_sec * 1000000 + t->tv_usec)) / 1000.0;
	memcpy(t, &u, sizeof(*t));
	return (d);
}

static char *
url_esc(const char *s, char *d, int len)
{
	int i = 0;

	while (*s && i < len - 4) {
		switch (*s) {
		case ' ':
			d[i++] = '+';
			break;
		case '+':
			strlcpy(d + i, "%2B", 4);
			i += 3;
			break;
		case '&':
			strlcpy(d + i, "%26", 4);
			i += 3;
			break;
		case '<':
			strlcpy(d + i, "%3C", 4);
			i += 3;
			break;
		case '>':
			strlcpy(d + i, "%3E", 4);
			i += 3;
			break;
		default:
			d[i++] = *s;
		}
		s++;
	}
	d[i] = 0;
	return (d);
}

static char *
url(int esc, const char *action, const char *sid, const char *pid,
    const char *opts, ...)
{
	const char *amp = esc ? "&amp;" : "&";
	static char s[8192], e[8192];

	snprintf(s, sizeof(s), "%s?action=%s", baseurl, action);
	if (sid != NULL && sid[0])
		snprintf(s + strlen(s), sizeof(s) - strlen(s),
		    "%ssid=%s", amp, html_esc(sid, e, sizeof(e), 0));
	if (pid != NULL && pid[0])
		snprintf(s + strlen(s), sizeof(s) - strlen(s),
		    "%spid=%s", amp, html_esc(pid, e, sizeof(e), 0));
	if (mode != NULL && mode[0])
		snprintf(s + strlen(s), sizeof(s) - strlen(s),
		    "%smode=%s", amp, html_esc(mode, e, sizeof(e), 0));
	if (thres != NULL && thres[0])
		snprintf(s + strlen(s), sizeof(s) - strlen(s),
		    "%sthres=%s", amp, url_esc(thres, e, sizeof(e)));
	if (opts != NULL && opts[0]) {
		va_list ap;

		va_start(ap, opts);
		vsnprintf(s + strlen(s), sizeof(s) - strlen(s),
		    opts, ap);
		va_end(ap);
	}
	return (s);
}

static void
dprintf(const char *fmt, ...)
{
	static char s[65536];
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);
	if (r < 0 || r >= sizeof(s))
		msg("error dprintf: vsnprintf: r %d (%d)", r, (int)sizeof(s));
	if (gz != NULL) {
		r = gzputs(gz, s);
		if (r != strlen(s))
			msg("error dprintf: gzputs: r %d (%d)",
			    r, (int)strlen(s));
	} else
		fprintf(stdout, "%s", s);
}

static void
render_error(const char *fmt, ...)
{
	va_list ap;
	char s[8192], e[8192];

	va_start(ap, fmt);
	vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);
	printf("%s\r\n\r\n", ct_html);
	fflush(stdout);
	dprintf("<html><head><title>Error</title></head><body>\n");
	dprintf("<h2>Error</h2><p><b>%s</b><p>\n", s);
	if (q != NULL) {
		dprintf("Request: <b>%s</b><br>\n",
		    html_esc(q->query_string, e, sizeof(e), 0));
		dprintf("Address: <b>%s</b><br>\n",
		    html_esc(q->remote_addr, e, sizeof(e), 0));
		if (q->user_agent != NULL)
			dprintf("User agent: <b>%s</b><br>\n",
			    html_esc(q->user_agent, e, sizeof(e), 0));
		if (q->referer != NULL)
			dprintf("Referer: <b>%s</b><br>\n",
			    html_esc(q->referer, e, sizeof(e), 0));
	}
	dprintf("Time: <b>%s</b><br>\n", rfc822_time(time(0)));
	dprintf("<p>If you believe this is a bug in <i>this</i> server, "
	    "please send reports with instructions about how to "
	    "reproduce to <a href=\"mailto:%s\"><b>%s</b></a><p>\n",
	    mailaddr, mailaddr);
	dprintf("</body></html>\n");
}

static void
render_redirect(const char *fmt, ...)
{
	va_list ap;
	char s[8192];

	va_start(ap, fmt);
	vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);
	printf("Status: 302\r\nLocation: %s\r\n\r\n", s);
	fflush(stdout);
}

static int
render_html(const char *html_fn, render_cb r, const struct entry *e)
{
	FILE *f;
	char s[8192], h[8192];

	if ((f = fopen(html_fn, "r")) == NULL) {
		dprintf("ERROR: fopen: %s: %s<br>\n", html_fn, strerror(errno));
		return (1);
	}
	while (fgets(s, sizeof(s), f)) {
		char *a, *b;

		for (a = s; (b = strstr(a, "%%")) != NULL;) {
			*b = 0;
			dprintf("%s", a);
			a = b + 2;
			if ((b = strstr(a, "%%")) != NULL) {
				*b = 0;
				if (!strcmp(a, "BASEURL"))
					dprintf("%s", baseurl);
				else if (!strcmp(a, "AUTHURL")) {
					const char *h = getenv("HTTP_HOST");
	
					dprintf("https://%s/", h);
				} else if (!strcmp(a, "RCSID"))
					dprintf("%s", rcsid);
				else if (!strcmp(a, "MODE")) {
					if (mode != NULL && mode[0])
						dprintf("%s", html_esc(mode,
						    h, sizeof(h), 0));
				} else if (!strcmp(a, "THRES")) {
					if (thres != NULL && thres[0])
						dprintf("%s", url_esc(thres,
						    h, sizeof(h)));
				} else if (!strcmp(a, "URLOPTS")) {
					if (mode != NULL && mode[0])
						dprintf("&amp;mode=%s",
						    html_esc(mode, h,
						    sizeof(h), 0));
					if (thres != NULL && thres[0])
						dprintf("&amp;thres=%s",
						    url_esc(thres, h,
						    sizeof(h)));
				} else if (!strcmp(a, "USERGROUP")) {
					if (user_is_registered(&user))
						dprintf("%s, %s ", user.id,
						    user.group);
				} else if (!strcmp(a, "USERNAVBAR")) {
					char fn[8192];

					snprintf(fn, sizeof(fn),
					    "%s/%s_navbar.html", htmldir,
					    user_is_editor(&user) ? "editor" :
					    (user_is_registered(&user) ? "user" :
					    "anon"));
					render_html(fn, NULL, NULL);
				} else if (!strcmp(a, "CAPTCHA"))  {
					char x[8192];

					if (user_is_registered(&user))
						goto skip_captcha;
					dprintf("<i>Please prove that you are a "
					    "human being. We don't accept "
					    "automated submissions from dumb "
					    "scripts (spam and all that). If "
					    "you are human, but for some reason "
					    "can't do this, please notify <a "
					    "href=\"mailto:daniel@benzedrine"
					    ".cx\">daniel@benzedrine.cx</a>."
					    "<b> Create an account and "
					    "log in to get rid of this thing."
					    "</b></i><br>\n");
					dprintf("<pre>\n%s</pre>",
					    html_esc(captcha_text, x, sizeof(x),
					    1));
					dprintf("Please enter the eight "
					    "lower-case letters and digits "
					    "above in the field below<br>\n");
					dprintf("<input type=\"text\" name="
					    "\"captcha\" value=\"\" size="
					    "\"8\"><p>\n");
				} else if (!strcmp(a, "CKSUM"))  {
					dprintf("%s", captcha_cksum);
				} else if (r != NULL)
					(*r)(a, e);
			skip_captcha:
				a = b + 2;
			}
		}
		dprintf("%s", a);
	}
	fclose(f);
	return (0);
}

static void
render_rss(const char *m, const struct entry *e)
{
	if (!strcmp(m, "ITEMS")) {
		const char *items = get_query_param(q, "items");
		char fn[1024];
		int i, max = 4;

		if (items != NULL) {
			max = atoi(items);
			if (max < 1)
				max = 1;
			else if (max > 10)
				max = 10;
		}
		snprintf(fn, sizeof(fn), "%s/summary_item.rss", htmldir);
		for (i = 0; i < max && i < sizeof(newest) / sizeof(newest[0]) &&
		    newest[i].sid[0]; ++i) {
			newest[i].flags = e->flags;
			render_html(fn, &render_rss_item, &newest[i]);
		}
	} else
		dprintf("render_rss: unknown macro '%s'\n", m);
}

static void
render_rss_item(const char *m, const struct entry *e)
{
	char d[256];

	if (!strcmp(m, "TITLE")) {
		dprintf("%s", html_esc(e->subject, d, sizeof(d), 0));
	} else if (!strcmp(m, "LINK")) {
		dprintf("%s", url(1, "article", e->sid, NULL, NULL));
	} else if (!strcmp(m, "DATE")) {
		dprintf("%s", rfc822_time(convert_time(e->date)));
	} else if (!strcmp(m, "CATEGORY")) {
		if (!strncmp(e->topic, "topic", 5))
			dprintf("%s", e->topic + 5);
		else
			dprintf("%s", e->topic);
	} else if (!strcmp(m, "BODY")) {
		FILE *f;
		char s[8192], fn[8192];
		int body = 0;

		if ((f = fopen(e->fn, "r")) == NULL) {
			dprintf("render_rss_item: fopen: %s: %s\n",
			    e->fn, strerror(errno));
			return;
		}
		while (fgets(s, sizeof(s), f)) {
			if (body)
				dprintf("%s", s);
			else if (s[0] == '\n' && !s[1])
				body = 1;
		}
		fclose(f);
		snprintf(fn, sizeof(fn), "%s.more", e->fn);
		if ((f = fopen(fn, "r")) == NULL)
			return;
		fclose(f);
		dprintf("Read <a href=\"%s\">more</a>...\n",
		    url(1, "article", e->sid, NULL, NULL));
	} else
		dprintf("render_rss_item: unknown macro '%s'\n", m);
}

static void
render_rss_errata(const char *m, const struct entry *e)
{
	if (!strcmp(m, "ITEMS")) {
		FILE *f;
		char url[8192], s[8192], i = 0;

		if ((f = fopen(erratafile, "r")) == NULL)
			return;
		fgets(url, sizeof(url), f);
		while (fgets(s, sizeof(s), f) && i++ < 8) {
			char date[128], name[128], severity[128], patch[256],
			    text[1024];
			int num;
			struct entry e;
			char fn[1024];

			memset(text, 0, sizeof(text));
			if (sscanf(s, "%127s %d %127s %127s %255s %1023c",
			    date, &num, name, severity, patch, text) != 6)
				continue;
			memset(&e, 0, sizeof(e));
			strlcpy(e.date, date, sizeof(e.date));
			snprintf(e.href, sizeof(e.href),
			    "%s#%s", url, name);
			strlcpy(e.dept, severity, sizeof(e.dept));
			snprintf(e.subject, sizeof(e.subject), "%03d %s",
			    num, severity);
			e.body = strdup(text);
			if (!e.body)
				continue;
			snprintf(fn, sizeof(fn), "%s/errata_item.rss", htmldir);
			render_html(fn, &render_rss_errata_item, &e);
			free(e.body);
		}
		fclose(f);
	} else
		dprintf("render_rss_errata: unknown macro '%s'\n", m);
}

static void
render_rss_errata_item(const char *m, const struct entry *e)
{
	char d[8192];

	if (!strcmp(m, "TITLE")) {
		dprintf("%s", html_esc(e->subject, d, sizeof(d), 0));
	} else if (!strcmp(m, "LINK")) {
		dprintf("%s", html_esc(e->href, d, sizeof(d), 0));
	} else if (!strcmp(m, "DATE")) {
		dprintf("%s", html_esc(e->date, d, sizeof(d), 0)); /* XXX */
	} else if (!strcmp(m, "CATEGORY")) {
		dprintf("%s", html_esc(e->dept, d, sizeof(d), 0));
	} else if (!strcmp(m, "BODY")) {
		dprintf("%s", e->body);
	} else
		dprintf("render_rss_errata_item: unknown macro '%s'\n", m);
}

static void
render_front(const char *m, const struct entry *e)
{
	char fn[1024];
	int i;

	if (!strcmp(m, "STORY")) {
		snprintf(fn, sizeof(fn), "%s/front_story.html", htmldir);
		for (i = 0; (e->flags & FLAG_SUBMISSION || i < 9) &&
		    i < sizeof(newest) / sizeof(newest[0]) &&
		    newest[i].sid[0]; ++i) {
			newest[i].flags |= e->flags;
			render_html(fn, &render_front_story, &newest[i]);
		}
	} else if (!strcmp(m, "HEADER")) {
		snprintf(fn, sizeof(fn), "%s/header.html", htmldir);
		render_html(fn, NULL, NULL);
	} else if (!strcmp(m, "FOOTER")) {
		snprintf(fn, sizeof(fn), "%s/footer.html", htmldir);
		render_html(fn, NULL, NULL);
	} else if (!strncmp(m, "BOX", 3)) {
		if (strcmp(m + 3, "_poll") || poll.sid[0]) {
			snprintf(fn, sizeof(fn), "%s/front_box%s.html",
			    htmldir, m + 3);
			render_html(fn, render_front_box, e);
		}
	} else if (!strcmp(m, "EDITORS")) {
		FILE *f;
		char s[8192], u[32], g[32], p[128], n[128], e[128], h[128];
		char x[8192];
		unsigned users = 0;

		if ((f = fopen(passwd, "r")) == NULL)
			return;
		while (fgets(s, sizeof(s), f)) {
			memset(u, 0, sizeof(u));
			memset(g, 0, sizeof(g));
			memset(p, 0, sizeof(p));
			memset(n, 0, sizeof(n));
			memset(e, 0, sizeof(e));
			memset(h, 0, sizeof(h));
			if (sscanf(s, "%31[^;];%31[^;];%127[^;];%127[^;];"
			    "%127[^;];%127[^;]", u, g, p, n, e, h) != 6)
				continue;
			if (strcmp(g, "editor") && strcmp(g, "admin")) {
				if (!strcmp(g, "user"))
					users++;
				continue;
			}
			if (!h[0] || h[0] == '\n')
				continue;
			dprintf("<tr>");
			html_esc(u, x, sizeof(x), 0);
			dprintf("<td><a href=\"%s\">%s</a></td>\n",
			    url(1, "search", NULL, NULL,
			    "&amp;sort=time&amp;query=%s", x), x);
			dprintf("<td>%s</td>", html_esc(n,
			    x, sizeof(x), 0));
			html_esc(e, x, sizeof(x), 0);
			dprintf("<td><a href=\"mailto:%s\">%s</a></td>", x, x);
			dprintf("<td>%s</td>", html_esc(h,
			    x, sizeof(x), 0));
			dprintf("</tr>\n");
		}
		fclose(f);
		dprintf("<tr><td>... and %u users!</td></tr>\n", users);
	} else
		dprintf("render_front: unknown macro '%s'<br>\n", m);
}

static void
render_front_story(const char *m, const struct entry *e)
{
	if (!strcmp(m, "NAME")) {
		if (e->href[0])
			dprintf("<a href=\"%s\">", e->href);
		if (e->name[0])
			dprintf("%s", e->name);
		if (e->href[0])
			dprintf("</a>");
	} else if (!strcmp(m, "SUBJECT")) {
		if (e->flags & FLAG_SUBMISSION)
			dprintf("<font color=\"ff0000\">");
		dprintf("%s", e->subject);
		if (e->flags & FLAG_SUBMISSION)
			dprintf("</font>");
	} else if (!strcmp(m, "DATE")) {
		dprintf("%s (GMT)", e->date);
		if (e->pubdate && e->pubdate > timenow(0))
			dprintf(" <b>delayed</b>");
	} else if (!strcmp(m, "DEPARTMENT")) {
		dprintf("%s", e->dept);
	} else if (!strcmp(m, "TOPICQUERY")) {
		dprintf("%s", e->topic);
	} else if (!strcmp(m, "TOPICIMG")) {
		/* XXX should be in files */
		if (!strcmp(e->topic, "oreilly_weasel") ||
		    !strcmp(e->topic, "topicblog") ||
		    !strcmp(e->topic, "topicbsd") ||
		    !strcmp(e->topic, "topiccrypto") ||
		    !strcmp(e->topic, "topiceditorial") ||
		    !strcmp(e->topic, "topicmail") ||
		    !strcmp(e->topic, "topicopenbsd") ||
		    !strcmp(e->topic, "topicopenssh") ||
		    !strcmp(e->topic, "topicports"))
			dprintf("%s.gif", e->topic);
		else if (!strcmp(e->topic, "topic30") ||
		    !strcmp(e->topic, "topic30") ||
		    !strcmp(e->topic, "topicpf2") ||
		    !strcmp(e->topic, "topicreadme") ||
		    !strcmp(e->topic, "topicnda") ||
		    !strcmp(e->topic, "topicsparc"))
			dprintf("%s.png", e->topic);
		else
			dprintf("%s.jpg", e->topic);
	} else if (!strcmp(m, "BODY")) {
		FILE *f;
		char s[8192];
		int body = 0, line = 0;
		int a[32], votesum = 0;

		if (e->body != NULL) {
			dprintf("%s", e->body);
			return;
		}
		if ((f = fopen(e->fn, "r")) == NULL) {
			dprintf("render_front_story: fopen: %s: %s<br>\n",
			    e->fn, strerror(errno));
			return;
		}
		if (e->flags & FLAG_POLL) {
			read_poll(e->sid, a, sizeof(a) / sizeof(a[0]),
			    &votesum);
			dprintf("<table width=\"100%%\">\n");
		}
		while (fgets(s, sizeof(s), f)) {
			if (body) {
				if (e->flags & FLAG_POLL) {
					double p = votesum ? ((double)a[line] *
					    100.0 / (double)votesum) : 0.0;

					dprintf("<tr><td>%s</td><td>%.1f%% "
					    "(%d votes)</td></tr>\n",
					    s, p, a[line]);
					dprintf("<tr><td colspan=\"2\"><table "
					    "width=\"100%%\" cellpadding=\"0\" "
					    "cellspacing=\"0\" border=\"0\">"
					    "<tr><td width=\"%d%%\" bgcolor="
					    "\"#000000\"><br></td><td width="
					    "\"%d%%\" bgcolor=\"#cccccc\"><br>"
					    "</td></tr></table></td></tr>\n",
					    (int)(p + 0.5),
					    100 - (int)(p + 0.5));
					dprintf("</tr>\n");
					line++;
				} else
					dprintf("%s", s);
			} else if (s[0] == '\n' && !s[1])
				body = 1;
		}
		fclose(f);
		if (e->flags & FLAG_POLL) {
			dprintf("</table>\n");
			dprintf("<p>Total votes: %d\n", votesum);
		}
	} else if (!strcmp(m, "MORE")) {
		char fn[8192], s[8192];
		FILE *f;

		if (e->more != NULL) {
			dprintf("%s", e->more);
			return;
		}
		snprintf(fn, sizeof(fn), "%s.more", e->fn);
		if ((f = fopen(fn, "r")) == NULL)
			return;
		if (e->flags & FLAG_FRONT) {
			fclose(f);
			dprintf("Read <a href=\"%s\">more</a>...\n",
			    url(1, "article", e->sid, NULL, NULL));
			return;
		}
		while (fgets(s, sizeof(s), f))
			dprintf("%s", s);
		fclose(f);
	} else if (!strcmp(m, "NAVIGATION")) {
		const char *oldmode = mode;
		int count = count_comments(e->sid);
		time_t newest;

		if (!(e->flags & FLAG_FRONT))
			return;
		if (e->flags & FLAG_SUBMISSION && user_is_editor(&user)) {
			dprintf("<font size=\"2\">\n");
			dprintf("<a href=\"%s\"><font color=\"#ff0000\">"
			    "[ Edit ]</font></a> ",
			    url(1, "edit", e->sid, NULL, NULL));
			dprintf("<a href=\"%s\"><font color=\"#ff0000\">"
			    "[ Publish ]</font></a> ",
			    url(1, "publish", e->sid, NULL, NULL));
			dprintf("<a href=\"%s\"><font color=\"#ff0000\">"
			    "[ Delete ]</font></a> ",
			    url(1, "delete", e->sid, NULL, NULL));
			dprintf("</font>");
			return;
		}
		dprintf("<font size=\"2\">\n");
		dprintf("<a href=\"%s\">[ <i>%u comment%s",
		    url(1, "article", e->sid, NULL, NULL), count,
		    count != 1 ? "s" : "");
		if (count > 0) {
			newest = find_newest_change(e->sid);
			dprintf(" %s ago", duration_str(time(NULL) -
			    newest));
		}
		dprintf("</i> ]</a> ");
		mode = NULL;
		dprintf("<a href=\"%s&amp;mode=flat&amp;count=%d\"><i> (flat) </i>"
		    "</a> ", url(1, "article", e->sid, NULL, NULL), count);
		dprintf("<a href=\"%s&amp;mode=expanded&amp;count=%d\"><i> (expanded)"
		    " </i></a> ",
		    url(1, "article", e->sid, NULL, NULL), count);
		if (user_is_editor(&user)) {
			dprintf("<a href=\"%s\"><font color=\"#ff0000\">"
			    "[ Edit ]</font></a> ",
			    url(1, "edit", e->sid, NULL, NULL));
		}
		if (user_is_admin(&user)) {
			dprintf("<a href=\"%s\"><font color=\"#ff0000\">"
			    "[ Delete ]</font></a> ",
			    url(1, "delete", e->sid, NULL, NULL));
		}
		if (user_is_registered(&user)) {
			if (!get_lastread(e->sid, &tlastread, 0))
				dprintf("<i>(new article)</i>");
			else if (count > 0 && newest > tlastread)
				dprintf("<i>(new comments)</i>");
		}
		dprintf("</font>\n");
		mode = oldmode;
	} else
		dprintf("render_front_story: unknown macro '%s'<br>\n", m);
}

static void
render_front_box(const char *m, const struct entry *e)
{
	if (!strcmp(m, "WEEKLIST")) {
		char fn[8192];
		int i;

		snprintf(fn, sizeof(fn), "%s/front_week.html", htmldir);
		for (i = 9; i < 18 && i < sizeof(newest) / sizeof(newest[0]) &&
		    newest[i].sid[0]; ++i) {
			if (i == 9 || strncmp(newest[i].sid,
			    newest[i - 1].sid, 8)) {
				const char *nd[7] = { "Sunday", "Monday",
				    "Tuesday", "Wednesday", "Thursday",
				    "Friday", "Saturday" };
				const char *nm[12] = { "January", "February",
				    "March", "April", "May", "June", "July",
				    "August", "September", "October",
				    "November", "December" };
				struct tm t;
				int y, m, d;

				sscanf(newest[i].sid, "%4d%2d%2d", &y, &m, &d);
				memset(&t, 0, sizeof(t));
				t.tm_year = y - 1900;
				t.tm_mon = m - 1;
				t.tm_mday = d;
				mktime(&t);
				dprintf("<tr><td colspan=\"2\"><font size=\"3\""
				    " color=\"black\"><b>"
				    "%s, %s %2.2d</b></font><br></td></tr>\n",
				    nd[t.tm_wday], nm[t.tm_mon], t.tm_mday);
			}
			newest[i].flags = e->flags;
			render_html(fn, &render_front_weeklistentry,
			    &newest[i]);
		}
	} else if (!strcmp(m, "YESTERDAY")) {
		time_t t = time(NULL);
		struct tm tm = *gmtime(&t);

		tm.tm_mday--;
		mktime(&tm);
		dprintf("%4.4d%2.2d%2.2d", tm.tm_year + 1900, tm.tm_mon + 1,
		    tm.tm_mday);
	} else if (!strcmp(m, "POLL")) {
		FILE *f;
		char s[8192];
		int hdr = 1, val = 0;

		if (!poll.sid[0])
			return;
		if ((f = fopen(poll.fn, "r")) == NULL)
			return;
		dprintf("<form method=\"post\" action=\"%s\">\n",
		    url(1, "vote", NULL, NULL, NULL));
		dprintf("<b>%s</b><br>\n", poll.subject);
		dprintf("<input type=\"hidden\" name=\"sid\" value=\"%s\">\n",
		    poll.sid);
		while (fgets(s, sizeof(s), f)) {
			if (hdr) {
				if (s[0] == '\n')
					hdr = 0;
				continue;
			}
			dprintf("<input type=\"radio\" name=\"val\" "
			    "value=\"%d\"> %s<br>\n", val++, s);
		}
		dprintf("<br><input type=\"submit\" value=\"Vote\">\n");
		dprintf("<a href=\"%s\">[ %d comments ]</a>\n",
		    url(1, "article", poll.sid, NULL, NULL),
		    count_comments(poll.sid));
		dprintf("</form>");
		fclose(f);
	} else if (!strcmp(m, "ERRATA")) {
		FILE *f;
		char url[8192], s[8192], i = 0;

		if ((f = fopen(erratafile, "r")) == NULL) {
			msg("error errata: fopen: %s: %s", erratafile,
			    strerror(errno));
			return;
		}
		fgets(url, sizeof(url), f);
		while (fgets(s, sizeof(s), f) && i++ < 4) {
			char date[128], name[128], severity[128], patch[256],
			    text[1024];
			int num;

			memset(text, 0, sizeof(text));
			if (sscanf(s, "%127s %d %127s %127s %255s %1023c",
			    date, &num, name, severity, patch, text) != 6) {
				msg("error errata: sscanf");
				continue;
			}
			dprintf("<tr><td valign=\"top\">");
			dprintf("<font size=\"1\">%s</font>",
			    date);
			dprintf("</td><td valign=\"top\">");
			dprintf("<font size=\"1\">");
			if (!strcmp(severity, "SECURITY"))
				strlcpy(severity, "<font color=\"#e00000\">"
				    "<b>SECURITY</b></font>", sizeof(severity));
			dprintf("<a href=\"%s#%s\">%03d</a> %s %s",
			    url, name, num, severity, text);
			dprintf("</font></td></tr>\n");
		}
		fclose(f);
	} else if (!strcmp(m, "VUXML")) {
		FILE *f;
		char s[8192], i = 0;

		if ((f = fopen(vuxmlfile, "r")) == NULL)
			return;
		while (fgets(s, sizeof(s), f) && i++ < 4) {
			char date[128], vid[128], topic[128];

			memset(topic, 0, sizeof(topic));
			if (sscanf(s, "%127s %127s %127c", date, vid, topic) !=
			    3)
				continue;
			dprintf("<tr><td valign=\"top\">");
			dprintf("<font size=\"1\">%s</font>",
			    date);
			dprintf("</td><td valign=\"top\">");
			dprintf("<font size=\"1\">");
			dprintf("<a href=\"http://www.vuxml.org/openbsd/%s"
			    ".html\">%s</a>", vid, topic);
			dprintf("</font></td></tr>\n");
		}
		fclose(f);
	} else
		dprintf("render_front_box: unknown macro '%s'<br>\n", m);
}

static void
render_front_weeklistentry(const char *m, const struct entry *e)
{
	if (!strcmp(m, "TIME")) {
		char s[32];
		char *t;

		strlcpy(s, e->date, sizeof(s));
		t = strrchr(s, ':');
		if (t == NULL || t - s < 5 || !t[1] || !t[2]) {
			dprintf("??");
			return;
		}
		*t = 0;
		dprintf("%s", t - 5);
	} else if (!strcmp(m, "SUBJECT")) {
		dprintf("%s", e->subject);
	} else if (!strcmp(m, "HREF")) {
		dprintf("%s", url(1, "article", e->sid, NULL, NULL));
	} else if (!strcmp(m, "COUNT")) {
		dprintf("%u", count_comments(e->sid));
	} else
		dprintf("render_front_weeklistentry: unknown macro '%s'<br>\n",
		    m);
}

static void
render_article(const char *m, const struct entry *e)
{
	char fn[1024];

	if (!strcmp(m, "TITLE")) {
		dprintf("%s", e->subject);
	} else if (!strcmp(m, "STORY")) {
		snprintf(fn, sizeof(fn), "%s/front_story.html", htmldir);
		render_html(fn, &render_front_story, e);
	} else if (!strcmp(m, "RELATED")) {
		if (e->name[0])
			dprintf("<a href=\"%s\">more by %s</a>\n",
			    url(1, "search", NULL, NULL, "&amp;sort=time&amp;"
			    "query=%s", e->name), e->name);
	} else if (!strcmp(m, "NAVIGATION")) {
		const char *oldmode = mode;
		struct entry prev, next;
		char h[8192];

		if (e->flags & FLAG_SUBMISSION)
			return;
		mode = NULL;
		if (e->pid[0])
			find_comment_adjacent(e->sid, e->pid, &prev, &next);
		else
			find_article_adjacent(e->sid, &prev, &next);
		dprintf("<p>\n");
		mode = oldmode;
		if ((prev.pid[0] || (!e->pid[0] && prev.sid[0])) &&
		    !(e->flags & FLAG_POLL)) {
			dprintf("&lt;&lt;");
			dprintf(" <a href=\"%s\">%s</a> |",
			    url(1, "article", prev.sid, prev.pid, NULL),
			    prev.subject);
		}
		if (e->pid[0]) {
			struct entry p;

			find_parent(e->sid, e->pid, &p);
			dprintf(" <a href=\"%s\"><b>Up:</b> %s</a>",
			    url(1, "article", p.sid, p.pid, NULL),
			    p.pid[0] ? p.subject : e->subject);
		} else
			dprintf(" <a href=\"%s\"><b>Reply</b></a>",
			    url(1, "submit", e->sid, NULL, NULL));
		mode = NULL;
		if (e->flags & FLAG_FLAT)
			dprintf(" | <a href=\"%s\">Threaded</a>",
			    url(1, "article", e->sid, e->pid, NULL));
		else {
			dprintf(" | <a href=\"%s&amp;mode=flat\">"
			    "Flattened</a>",
			    url(1, "article", e->sid, e->pid, NULL));
			if (e->flags & FLAG_EXPANDED)
				dprintf(" | <a href=\"%s\">Collapsed</a>",
				    url(1, "article", e->sid, e->pid,
				    NULL));
			else
				dprintf(" | <a href=\"%s&amp;mode="
				    "expanded\">Expanded</a>",
				    url(1, "article", e->sid, e->pid,
				    NULL));
		}
		mode = oldmode;
		if ((next.pid[0] || (!e->pid[0] && next.sid[0])) &&
		    !(e->flags & FLAG_POLL)) {
			dprintf(" | <a href=\"%s\">%s</a>",
			    url(1, "article", next.sid, next.pid, NULL),
			    next.subject);
			dprintf(" &gt;&gt;\n");
		}

		dprintf("<form method=\"get\" action=\"%s\">\n",
		    url(1, "article", e->sid, e->pid, NULL));
		dprintf("<input type=\"hidden\" name=\"action\" "
		    "value=\"article\">\n");
		dprintf("<input type=\"hidden\" name=\"sid\" "
		    "value=\"%s\">\n", html_esc(e->sid, h, sizeof(h), 0));
		if (e->pid[0])
			dprintf("<input type=\"hidden\" name=\"pid\" "
			    "value=\"%s\">\n", html_esc(e->pid, h, sizeof(h),
			    0));
		if (e->flags & FLAG_EXPANDED)
			dprintf("<input type=\"hidden\" name=\"mode\" "
			    "value=\"expanded\">\n");
		else if (e->flags & FLAG_FLAT)
			dprintf("<input type=\"hidden\" name=\"mode\" "
			    "value=\"flat\">\n");
		dprintf("Threshold: ");
		dprintf("<input type=\"text\" name=\"thres\" "
		    "value=\"%s\" size=\"30\">\n", thres ? html_esc(thres,
		    h, sizeof(h), 0) : "");
		if (thres != NULL && thres[0] && thres_eval(thres, 0,0 ) < 0)
			dprintf("syntax error!\n");
		dprintf("<input type=\"submit\" value=\"Change\">\n");
		dprintf("<a href=\"%s\">Help</a>\n",
		    url(1, "helpthres", NULL, NULL, NULL));
		dprintf("</form>\n");
		mode = oldmode;
	} else if (!strcmp(m, "COMMENTS")) {
		char html_col[1024], html_exp[1024];
		struct entry a[256];
		int i;

		if (e->flags & FLAG_SUBMISSION)
			return;
		if (user_is_registered(&user))
			get_lastread(e->sid, &tlastread, 1);
		snprintf(html_exp, sizeof(html_exp),
		    "%s/article_comment.html", htmldir);
		snprintf(html_col, sizeof(html_col),
		    "%s/article_comment_collapsed.html", htmldir);
		memset(&a, 0, sizeof(a));
		find_comments(e->sid, e->pid, a, sizeof(a) / sizeof(a[0]));
		if (e->flags & FLAG_FLAT)
			qsort(a, sizeof(a) / sizeof(a[0]), sizeof(a[0]),
			    &compare_qsort);
		for (i = 0; (i < sizeof(a) / sizeof(a[0])) && a[i].pid[0];
		    ++i) {
			if (thres != NULL && thres[0] &&
			    !thres_eval(thres, a[i].modsum, a[i].modcount))
				continue;
			a[i].flags = e->flags;
			render_html(a[i].level && !(a[i].flags &
			    (FLAG_FLAT|FLAG_EXPANDED)) ?  html_col : html_exp,
			    &render_article_comment, a + i);
		}
	} else if (!strcmp(m, "HEADER")) {
		snprintf(fn, sizeof(fn), "%s/header.html", htmldir);
		render_html(fn, NULL, NULL);
	} else if (!strcmp(m, "FOOTER")) {
		snprintf(fn, sizeof(fn), "%s/footer.html", htmldir);
		render_html(fn, NULL, NULL);
	} else
		dprintf("render_article: invalid macro '%s'<br>\n", m);
}

static void
render_article_comment(const char *m, const struct entry *e)
{
	char d[256];
	int i;

	if (!strcmp(m, "INDENT")) {
		for (i = 0; !(e->flags & FLAG_FLAT) && e->level &&
		    i <= e->level; ++i)
			dprintf("<ul>");
	} else if (!strcmp(m, "UNINDENT")) {
		for (i = 0; !(e->flags & FLAG_FLAT) && e->level &&
		    i <= e->level; ++i)
			dprintf("</ul>");
	} else if (!strcmp(m, "LINK")) {
		dprintf("%s", url(1, "article", e->sid, e->pid, NULL));
	} else if (!strcmp(m, "NAME")) {
		dprintf("%s", html_esc(e->name, d, sizeof(d), 0));
	} else if (!strcmp(m, "HOST")) {
		html_esc(e->host, d, sizeof(d), 0);
		if (d[0])
			dprintf("(<a href=\"%s\">%s</a>)",
			    url(1, "search", NULL, NULL,
			    "&amp;method=and&amp;sort=time&amp;comments=yes&amp;"
			    "query=%s", d), d);
	} else if (!strcmp(m, "EMAIL")) {
		html_esc(e->email, d, sizeof(d), 0);
		if (d[0])
			dprintf("<b><font color=\"#336666\" "
			    "size=2>(%s)</font></b> ", d);
	} else if (!strcmp(m, "HREF")) {
		html_esc(e->href, d, sizeof(d), 0);
		if (d[0])
			dprintf("<br><a href=\"%s\">%s</a>", d, d);
	} else if (!strcmp(m, "SUBJECT")) {
		dprintf("%s", html_esc(e->subject, d, sizeof(d), 0));
	} else if (!strcmp(m, "DATE")) {
		html_esc(e->date, d, sizeof(d), 0);
		if (d[0])
			dprintf("%s (GMT)", d);
	} else if (!strcmp(m, "MODCOUNT")) {
		dprintf("%d", e->modcount);
	} else if (!strcmp(m, "MODSUM")) {
		dprintf("%d", e->modsum);
	} else if (!strcmp(m, "NEW")) {
		if (user_is_registered(&user)
		    && convert_time(e->date) > tlastread)
			dprintf("<font color=\"#ff0000\">new</font>");
	} else if (!strcmp(m, "BODY")) {
		FILE *f;
		char s[8192];
		int body = 0;

		if (e->body != NULL) {
			if (e->flags & FLAG_PLAIN) {
				char b[65536];

				html_esc(e->body, b, sizeof(b), 2);
				dprintf("%s", b);
			} else
				dprintf("%s", e->body);
			return;
		}
		if ((f = fopen(e->fn, "r")) == NULL) {
			dprintf("render_article_comment: fopen: %s: %s<br>\n",
			    e->fn, strerror(errno));
			return;
		}
		while (fgets(s, sizeof(s), f)) {
			if (body)
				dprintf("%s", s);
			else if (s[0] == '\n' && !s[1])
				body = 1;
		}
		fclose(f);
	} else if (!strcmp(m, "NAVIGATION")) {
		dprintf("<a href=\"%s\">[ <i>Show thread</i> ]</a>",
		    url(1, "article", e->sid, e->pid, NULL));
		dprintf("<a href=\"%s\"> [ <i>Reply to this comment</i> ]</a>",
		    url(1, "submit", e->sid, e->pid, NULL));
		if (e->modable) {
			dprintf(" <a href=\"%s&amp;val=1\">[ <i>Mod Up</i> ]"
			    "</a>",
			    url(1, "moderate", e->sid, e->pid, NULL));
			dprintf(" <a href=\"%s&amp;val=-1\">[ <i>Mod Down</i> "
			    "]</a>", url(1, "moderate", e->sid, e->pid, NULL));
		}
		if (user_is_admin(&user))
			dprintf("<a href=\"%s\"><font color=\"#ff0000\">"
			    "[ Delete ]</font></a> ",
			    url(1, "delete", e->sid, e->pid, NULL));
		dprintf("\n");
	} else
		dprintf("render_article_comment: unknown macro '%s'<br>\n", m);
}

static void
render_register(const char *m, const struct entry *e)
{
	char fn[1024];

	if (!strcmp(m, "HEADER")) {
		snprintf(fn, sizeof(fn), "%s/header.html", htmldir);
		render_html(fn, NULL, NULL);
	} else if (!strcmp(m, "FOOTER")) {
		snprintf(fn, sizeof(fn), "%s/footer.html", htmldir);
		render_html(fn, NULL, NULL);
	} else
		dprintf("render_register: invalid macro '%s'<br>\n", m);
}

static void
render_submit(const char *m, const struct entry *e)
{
	char fn[1024];

	if (!strcmp(m, "TITLE")) {
		if (e->flags & FLAG_PREFS) {
			dprintf("Edit preferences for user %s", user.id);
			return;
		}
		if (e->flags & FLAG_EDIT) {
			dprintf("Edit %s article %s", e->flags &
			    FLAG_SUBMISSION ?  "submitted" : "published",
			    e->sid);
			return;
		}
		if (e->sid[0]) {
			struct entry f;

			memset(&f, 0, sizeof(f));
			snprintf(f.fn, sizeof(f.fn), "%s/%s/article",
			    datadir, e->sid);
			read_file(&f);
			dprintf("%s", f.subject);
		} else
			dprintf("Add Story");
	} else if (!strcmp(m, "STORY")) {
		snprintf(fn, sizeof(fn), "%s/front_story.html", htmldir);
		render_html(fn, &render_front_story, e);
	} else if (!strcmp(m, "RELATED")) {
		if (e->name[0])
			dprintf("<a href=\"%s\">more by %s</a>\n",
			    url(1, "search", NULL, NULL,
			    "&amp;sort=time&amp;query=%s", e->name), e->name);
	} else if (!strcmp(m, "FORM")) {
		if (e->flags & FLAG_PREFS) {
			snprintf(fn, sizeof(fn), "%s/prefs_form.html", htmldir);
			render_html(fn, &render_prefs_form, e);
			return;
		}
		if (e->flags & FLAG_EDIT)
			snprintf(fn, sizeof(fn), "%s/edit_form.html", htmldir);
		else
			snprintf(fn, sizeof(fn), "%s/submit_form.html",
			    htmldir);
		render_html(fn, &render_submit_form, e);
	} else if (!strcmp(m, "COMMENTS")) {
		/* nothing */
	} else if (!strcmp(m, "HEADER")) {
		snprintf(fn, sizeof(fn), "%s/header.html", htmldir);
		render_html(fn, NULL, NULL);
	} else if (!strcmp(m, "FOOTER")) {
		snprintf(fn, sizeof(fn), "%s/footer.html", htmldir);
		render_html(fn, NULL, NULL);
	} else
		dprintf("render_submit: invalid macro '%s'<br>\n", m);
}

static void
render_preview(const char *m, const struct entry *e)
{
	char fn[1024];

	if (!strcmp(m, "TITLE")) {
		dprintf("Preview");
	} else if (!strcmp(m, "STORY")) {
		if (e->flags & FLAG_EDIT) {
			snprintf(fn, sizeof(fn), "%s/front_story.html",
			    htmldir);
			render_html(fn, &render_front_story, e);
		} else {
			snprintf(fn, sizeof(fn), "%s/article_comment.html",
			    htmldir);
			render_html(fn, &render_article_comment, e);
		}
	} else if (!strcmp(m, "MORE")) {
		if (e->more != NULL) {
			if (e->flags & FLAG_PLAIN) {
				char m[65536];

				html_esc(e->more, m, sizeof(m), 2);
				dprintf("%s\n", m);
			} else
				dprintf("%s\n", e->more);
		}
	} else if (!strcmp(m, "FORM")) {
		snprintf(fn, sizeof(fn), "%s/submit_form.html", htmldir);
		render_html(fn, &render_submit_form, e);
	} else if (!strcmp(m, "COMMENTS")) {
		/* nothing */
	} else if (!strcmp(m, "HEADER")) {
		snprintf(fn, sizeof(fn), "%s/header.html", htmldir);
		render_html(fn, NULL, NULL);
	} else if (!strcmp(m, "FOOTER")) {
		snprintf(fn, sizeof(fn), "%s/footer.html", htmldir);
		render_html(fn, NULL, NULL);
	} else
		dprintf("render_preview: invalid macro '%s'<br>\n", m);
}

static void
render_submit_form(const char *m, const struct entry *e)
{
	if (!strcmp(m, "SUBJECT")) {
		struct entry f;

		if (e->flags & FLAG_EDIT) {
			dprintf("%s", e->subject);
			return;
		}
		if (e->body != NULL) {
			dprintf("%s", e->subject);
			return;
		}
		if (!e->sid[0])
			return;
		memset(&f, 0, sizeof(f));
		if (e->pid[0]) {
			if (find_pid_path(e->sid, e->pid, f.fn, sizeof(f.fn)))
				return;
			strlcat(f.fn, "/comment", sizeof(f.fn));
		} else
			snprintf(f.fn, sizeof(f.fn), "%s/%s/article",
			    datadir, e->sid);
		if (read_file(&f))
			return;
		if (strncmp(f.subject, "Re: ", 4) &&
		    strcmp(f.subject, "No Subject Given"))
			dprintf("Re: ");
		dprintf("%s", f.subject);
	} else if (!strcmp(m, "NAME"))  {
		if (!e->name[0] && user_is_registered(&user))
			dprintf("%s", user.name);
		else
			dprintf("%s", e->name);
	} else if (!strcmp(m, "EMAIL"))  {
		if (!e->pid[0] && !e->email[0] && user_is_registered(&user))
			dprintf("%s", user.email);
		else
			dprintf("%s", e->email);
	} else if (!strncmp(m, "SELECTED_TOPIC_", 15))  {
		if (!strcmp(m + 15, e->topic))
			dprintf("selected");
	} else if (!strcmp(m, "DEPT"))  {
		dprintf("%s", e->dept);
	} else if (!strcmp(m, "HREF"))  {
		if (!e->href[0] && user_is_registered(&user))
			dprintf("%s", user.href);
		else
			dprintf("%s", e->href);
	} else if (!strcmp(m, "SUBMISSION"))  {
		dprintf("%d", e->flags & FLAG_SUBMISSION ? 1 : 0);
	} else if (!strcmp(m, "DATA"))  {
		if (e->body != NULL) {
			char x[8192];

			dprintf("%s", html_esc(e->body, x, sizeof(x), 1));
		} else if (e->flags & FLAG_EDIT) {
			FILE *f;
			char s[8192], x[8192];
			int body = 0, line = 0;

			if ((f = fopen(e->fn, "r")) == NULL) {
				dprintf("render_submit_form: fopen: %s: %s\n",
				    e->fn, strerror(errno));
				return;
			}
			while (fgets(s, sizeof(s), f)) {
				s[strlen(s) - 1] = 0;
				if (body) {
					if (line++ > 0)
						dprintf("\n");
					dprintf("%s", html_esc(s, x, sizeof(x),
					    1));
				} else if (!s[0])
					body = 1;
			}
			fclose(f);
		} else if (e->pid[0]) {
			/* edit comment, quote parent */
			char fn[MAXPATHLEN];
			FILE *f;
			char s[8192], x[8192];
			int body = 0, i;

			if (find_pid_path(e->sid, e->pid, fn, sizeof(fn)))
				return;
			strlcat(fn, "/comment", sizeof(fn));
			if ((f = fopen(fn, "r")) == NULL)
				return;
			while (fgets(s, sizeof(s), f)) {
				while ((i = strlen(s)) > 0 &&
				    (s[i - 1] == '\n' || s[i - 1] == '\r'))
					s[i - 1] = 0;
				if (body) {
					dprintf("> %s\n", de_html(s, x,
					    sizeof(x)));
				} else if (!s[0])
					body = 1;
			}
			dprintf("\n");
			fclose(f);
		}
	} else if (!strcmp(m, "MORE"))  {
		if (e->more != NULL) {
			char x[8192];

			dprintf("%s", html_esc(e->more, x, sizeof(x), 1));
		} else if (e->flags & FLAG_EDIT) {
			char fn[8192], s[8192], x[8192];
			FILE *f;

			snprintf(fn, sizeof(fn), "%s.more", e->fn);
			if ((f = fopen(fn, "r")) == NULL)
				return;
			while (fgets(s, sizeof(s), f))
				dprintf("%s", html_esc(s, x, sizeof(x), 1));
			fclose(f);
		}
	} else if (!strncmp(m, "CHECKED_TYPE_", 13))  {
		if ((!strcmp(m + 13, "HTML") && !(e->flags & FLAG_PLAIN)) ||
		    (!strcmp(m + 13, "PLAIN") && e->flags & FLAG_PLAIN))
			dprintf("checked");
	} else if (!strcmp(m, "CHECKED_POLL"))  {
		if (e->flags & FLAG_POLL)
			dprintf("checked");
	} else if (!strcmp(m, "PUBDATE"))  {
		if (e->pubdate > 0)
			dprintf("%llu", e->pubdate);
	} else if (!strcmp(m, "SID"))  {
		dprintf("%s", e->sid);
	} else if (!strcmp(m, "PID"))  {
		dprintf("%s", e->pid);
	} else
		dprintf("render_submit_form: invalid macro '%s'<br>\n", m);
}

static void
render_prefs_form(const char *m, const struct entry *e)
{
	if (!strcmp(m, "NAME")) {
		dprintf("%s", user.name);
	} else if (!strcmp(m, "EMAIL"))  {
		dprintf("%s", user.email);
	} else if (!strcmp(m, "COMMENT"))  {
		dprintf("%s", user.href);
	} else
		dprintf("render_prefs_form: invalid macro '%s'<br>\n", m);
}

static void
render_searchresults(const char *m, const struct entry *e)
{
	char fn[8192], h[8192];
	const char *p;
	static int matches = -1;

	if (!strcmp(m, "RESULTS")) {
		struct entry res[30];
		const char *words = get_query_param(q, "query");
		const char *page = get_query_param(q, "page");
		const char *sort = get_query_param(q, "sort");
		const char *method = get_query_param(q, "method");
		const char *comments = get_query_param(q, "comments");
		const char *verbose = get_query_param(q, "verbose");
		int i;
		struct timeval ta, tb;
		unsigned usec;
		int cur = page != NULL && atoi(page) ? atoi(page) : 1;
		int pagesize = verbose != NULL && !strcmp(verbose, "yes") ?
		    10 : 30;

		if (words == NULL || !words[0])
			return;
		gettimeofday(&ta, NULL);
		matches = htsearch(htdigdir, comments != NULL &&
		    !strcmp(comments, "yes") ?  "comments" : "articles", words,
		    page == NULL || atoi(page) <= 0 ? 1 : atoi(page),
		    sort, method, res, pagesize);
		if (matches < 0) {
			render_error("htsearch: %d", matches);
			msg("error htsearch: %d", matches);
			return;
		}
		gettimeofday(&tb, NULL);
		usec = tb.tv_sec * 1000000 + tb.tv_usec -
		    (ta.tv_sec * 1000000 + ta.tv_usec);
		dprintf("<tr><td align=\"right\">");
		dprintf("<font color=\"#336666\" size=\"2\">");
		if (!matches)
			dprintf("No matching results found");
		else {
			int beg = (cur - 1) * pagesize + 1;
			int end = cur * pagesize;

			if (end > matches)
				end = matches;
			dprintf("Results <b>%d</b> - <b>%d</b> of <b>%d</b>",
			    beg, end, matches);
		}
		dprintf(" (<b>%.2f</b> seconds)<p>", (double)usec / 1000000.0);
		dprintf("<p></font></td></tr>\n");
		for (i = 0; res[i].sid[0] && i < pagesize; ++i) {
			char fn[8192];

			snprintf(fn, sizeof(fn), "%s/%s", datadir, res[i].fn);
			strlcpy(res[i].fn, fn, sizeof(res[i].fn));
			if (read_file(&res[i]))
				continue;
			if (res[i].pubdate && res[i].pubdate > timenow(0))
				continue;
			snprintf(fn, sizeof(fn),
			    "%s/searchresults_match.html", htmldir);
			render_html(fn, &render_searchresults_match, &res[i]);
		}
		msg("search %d matches in %.2f s for query %s", matches,
		    (double)usec / 1000000.0, words);
	} else if (!strcmp(m, "NAVIGATION")) {
		const char *query = get_query_param(q, "query");
		const char *page = get_query_param(q, "page");
		const char *sort = get_query_param(q, "sort");
		const char *method = get_query_param(q, "method");
		const char *comments = get_query_param(q, "comments");
		const char *verbose = get_query_param(q, "verbose");
		int i, cur = page != NULL && atoi(page) ? atoi(page) : 1;
		int pagesize = verbose != NULL && !strcmp(verbose, "yes") ?
		    10 : 30;
		int lastpage = (matches - 1) / pagesize + 1;
		char opts[1024];

		if (matches <= pagesize)
			return;
		snprintf(opts, sizeof(opts), "&amp;query=%s",
		    query);
		if (method != NULL)
			snprintf(opts + strlen(opts), sizeof(opts) -
			    strlen(opts), "&amp;method=%s",
			    html_esc(method, h, sizeof(h), 0));
		if (sort != NULL)
			snprintf(opts + strlen(opts), sizeof(opts) -
			    strlen(opts), "&amp;sort=%s",
			    html_esc(sort, h, sizeof(h), 0));
		if (comments != NULL)
			snprintf(opts + strlen(opts), sizeof(opts) -
			    strlen(opts), "&amp;comments=%s",
			    html_esc(comments, h, sizeof(h), 0));
		if (verbose != NULL)
			snprintf(opts + strlen(opts), sizeof(opts) -
			    strlen(opts), "&amp;verbose=%s",
			    html_esc(verbose, h, sizeof(h), 0));
		dprintf("Page: ");
		if (cur > 1)
			dprintf("<a href=\"%s\"><b>Previous</b></a> ",
			    url(1, "search", NULL, NULL, "%s&amp;page=%d",
			    opts, cur - 1));
		if (cur > 11)
			dprintf("<a href=\"%s\">1</a> ... ",
			    url(1, "search", NULL, NULL, "%s&amp;page=1",
			    opts));
		for (i = cur - 10; i <= cur + 10; ++i) {
			if (i < 1)
				continue;
			if (i > lastpage)
				break;
			if (i == cur)
				dprintf("<b>");
			else
				dprintf("&nbsp;<a href=\"%s\">", url(1,
				    "search", NULL, NULL, "%s&amp;page=%d",
				    opts, i));
			dprintf("%d", i);
			if (i == cur)
				dprintf("</b> ");
			else
				dprintf("</a> ");
		}
		if (cur < lastpage - 10)
			dprintf(" ... <a href=\"%s\">%d</a>",
			    url(1, "search", NULL, NULL, "%s&amp;page=%d",
			    opts, lastpage), lastpage);
		if (cur < lastpage)
			dprintf(" <a href=\"%s\"><b>Next</b></a> ",
			    url(1, "search", NULL, NULL, "%s&amp;page=%d",
			    opts, cur + 1));
		dprintf("\n");
	} else if (!strcmp(m, "QUERY")) {
		if ((p = get_query_param(q, "query")) != NULL)
			dprintf("%s", html_esc(p, h, sizeof(h), 0));
	} else if (!strncmp(m, "SELECTED_METHOD_", 16)) {
		if ((p = get_query_param(q, "method")) != NULL &&
		    !strcmp(p, m + 16))
			dprintf("selected");
	} else if (!strncmp(m, "SELECTED_SORT_", 14)) {
		if ((p = get_query_param(q, "sort")) != NULL &&
		    !strcmp(p, m + 14))
			dprintf("selected");
	} else if (!strcmp(m, "CHECKED_COMMENTS")) {
		if ((p = get_query_param(q, "comments")) != NULL &&
		    !strcmp(p, "yes"))
			dprintf("checked");
	} else if (!strcmp(m, "CHECKED_VERBOSE")) {
		if ((p = get_query_param(q, "verbose")) != NULL &&
		    !strcmp(p, "yes"))
			dprintf("checked");
	} else if (!strcmp(m, "HEADER")) {
		snprintf(fn, sizeof(fn), "%s/header.html", htmldir);
		render_html(fn, NULL, NULL);
	} else if (!strcmp(m, "FOOTER")) {
		snprintf(fn, sizeof(fn), "%s/footer.html", htmldir);
		render_html(fn, NULL, NULL);
	} else
		dprintf("render_searchresults: invalid macro '%s'<br>\n", m);
}

static void
render_searchresults_match(const char *m, const struct entry *e)
{
	char d[256];

	if (!strcmp(m, "LINK")) {
		dprintf("%s", url(1, "article", e->sid, e->pid, NULL));
	} else if (!strcmp(m, "SUBJECT")) {
		dprintf("%s", html_esc(e->subject, d, sizeof(d), 0));
	} else if (!strcmp(m, "NAME")) {
		if (e->href[0])
			dprintf("<a href=\"%s\">",
			    html_esc(e->href, d, sizeof(d), 0));
		dprintf("%s", html_esc(e->name, d, sizeof(d), 0));
		if (e->href[0])
			dprintf("</a>");
	} else if (!strcmp(m, "DATE")) {
		if (e->date[0])
			dprintf("%s (GMT)", e->date);
	} else if (!strcmp(m, "COUNT")) {
		dprintf("%u", count_comments(e->sid));
	} else if (!strcmp(m, "MOD")) {
		if (e->pid[0])
			dprintf("(%d/%d)", e->modsum, e->modcount);
	} else if (!strcmp(m, "SCORE")) {
		dprintf("%d", e->score);
	} else if (!strcmp(m, "EXCERPT")) {
		const char *p;

		if ((p = get_query_param(q, "verbose")) != NULL &&
		    !strcmp(p, "yes"))
			dprintf("<ul>%s</ul>\n", e->excerpt);
	} else
		dprintf("render_searchresults_match: invalid macro '%s'<br>\n",
		    m);
}

static void
find_articles(const char *path, unsigned long long date, struct entry *a,
    int size, int polls, struct entry *p)
{
	FTS *fts;
	FTSENT *e;
	char * const path_argv[] = { (char*)path, NULL };
	int i = 0;

	memset(a, 0, size * sizeof(*a));
	if (p != NULL)
		memset(p, 0, sizeof(*p));
	fts = fts_open(path_argv, FTS_LOGICAL|FTS_NOSTAT,
	    compare_name_des_fts);
	if (fts == NULL) {
		dprintf("fts_open: %s: %s<br>\n", path, strerror(errno));
		return;
	} else if ((e = fts_read(fts)) == NULL || !(e->fts_info & FTS_D)) {
		dprintf("fts_read: %s: %s<br>\n", path, strerror(errno));
		return;
	} else if ((e = fts_children(fts, FTS_NAMEONLY)) == NULL) {
		if (errno != 0)
			dprintf("fts_children: %s: %s<br>\n",
			    path, strerror(errno));
		return;
	}
	/* skip future articles for non-privileged users */
	if (!user_is_editor(&user)) {
		unsigned long long now = timenow(0);
		char d[15];

		if (!date || date > now)
			date = now;
		for (; e != NULL; e = e->fts_link) {
			if (!(e->fts_info & FTS_D) ||
			    (strncmp(e->fts_name, "20", 2) &&
			    strncmp(e->fts_name, "19", 2)))
				continue;
			strlcpy(d, e->fts_name, sizeof(d));
			if (strtoull(d, NULL, 10) <= date)
				break;
		}
	}
	for (; e != NULL && i < size; e = e->fts_link) {
		if (!(e->fts_info & FTS_D) || (strncmp(e->fts_name, "20", 2) &&
		    strncmp(e->fts_name, "19", 2)))
			continue;
		snprintf(a[i].fn, sizeof(a[i].fn), "%s/%s/article",
		    path, e->fts_name);
		if (read_file(&a[i])) {
			memset(&a[i], 0, sizeof(a[i]));
			continue;
		}
		strlcpy(a[i].sid, e->fts_name, sizeof(a[i].sid));
		if (a[i].flags & FLAG_POLL) {
			if (p != NULL && !p->sid[0])
				memcpy(p, &a[i], sizeof(*p));
			if (!polls) {
				memset(&a[i], 0, sizeof(a[i]));
				continue;
			}
		}
		i++;
	}
	fts_close(fts);
}

static void
find_article_adjacent(const char *sid, struct entry *prev, struct entry *next)
{
	FTS *fts;
	FTSENT *e;
	char * const path_argv[] = { datadir, NULL };
	unsigned long long now = timenow(0);
	int found;

	memset(prev, 0, sizeof(*prev));
	memset(next, 0, sizeof(*prev));
	/* find next */
	fts = fts_open(path_argv, FTS_LOGICAL|FTS_NOSTAT,
	    compare_name_asc_fts);
	if (fts == NULL || 
	    (e = fts_read(fts)) == NULL || !(e->fts_info & FTS_D) ||
	    (e = fts_children(fts, FTS_NAMEONLY)) == NULL)
		return;
	for (found = 0; e != NULL; e = e->fts_link) {
		if (!(e->fts_info & FTS_D) || (strncmp(e->fts_name, "20", 2) &&
		    strncmp(e->fts_name, "19", 2)))
			continue;
		if (!user_is_editor(&user) &&
		    strtoull(e->fts_name, NULL, 10) > now)
			continue;
		if (found) {
			snprintf(next->fn, sizeof(next->fn), "%s/%s/article",
			    datadir, e->fts_name);
			strlcpy(next->sid, e->fts_name, sizeof(next->sid));
			if (read_file(next) || next->flags & FLAG_POLL)
				memset(next, 0, sizeof(*next));
			else
				break;
		} else if (!strcmp(e->fts_name, sid))
			found = 1;
	}
	fts_close(fts);
	/* find prev */
	fts = fts_open(path_argv, FTS_LOGICAL|FTS_NOSTAT,
	    compare_name_des_fts);
	if (fts == NULL || 
	    (e = fts_read(fts)) == NULL || !(e->fts_info & FTS_D) ||
	    (e = fts_children(fts, FTS_NAMEONLY)) == NULL)
		return;
	for (found = 0; e != NULL; e = e->fts_link) {
		if (!(e->fts_info & FTS_D) || (strncmp(e->fts_name, "20", 2) &&
		    strncmp(e->fts_name, "19", 2)))
			continue;
		if (!user_is_editor(&user) &&
		    strtoull(e->fts_name, NULL, 10) > now)
			continue;
		if (found) {
			snprintf(prev->fn, sizeof(prev->fn), "%s/%s/article",
			    datadir, e->fts_name);
			strlcpy(prev->sid, e->fts_name, sizeof(prev->sid));
			if (read_file(prev) || prev->flags & FLAG_POLL)
				memset(prev, 0, sizeof(*prev));
			else
				break;
		} else if (!strcmp(e->fts_name, sid))
			found = 1;
	}
	fts_close(fts);
}

static void
find_comments(const char *sid, const char *pid, struct entry *a, int size)
{
	FTS *fts;
	FTSENT *e;
	char path[8192];
	char * const path_argv[] = { path, NULL };
	int i = 0;

	snprintf(path, sizeof(path), "%s/%s", datadir, sid);
	if (pid[0]) {
		fts = fts_open(path_argv, FTS_LOGICAL|FTS_NOSTAT, NULL);
		if (fts == NULL)
			return;
		while ((e = fts_read(fts)) != NULL)
			if (e->fts_info & FTS_D && !strcmp(e->fts_name, pid)) {
				strlcpy(path, e->fts_path, sizeof(path));
				break;
			}
		fts_close(fts);
		if (e == NULL)
			return;
	}
	fts = fts_open(path_argv, FTS_LOGICAL|FTS_NOSTAT, &compare_fts);
	if (fts == NULL)
		return;
	while ((e = fts_read(fts)) != NULL && i < size)
		if (e->fts_info & FTS_F && !strcmp(e->fts_name, "comment")) {
			const char *p = strrchr(e->fts_path, '/');
			int j = 0;

			strlcpy(a[i].sid, sid, sizeof(a[i].sid));
			while (*--p != '/')
				;
			p++;
			while (*p != '/' && j < sizeof(a[i].pid) - 1)
				a[i].pid[j++] = *p++;
			a[i].pid[j] = 0;
			strlcpy(a[i].fn, e->fts_path, sizeof(a[i].fn));
			if (!read_file(&a[i])) {
				a[i].level = e->fts_level - (pid[0] ? 1 : 2);
				i++;
			}
		}
	fts_close(fts);
}

static void
find_parent(const char *sid, const char *pid, struct entry *p)
{
	FTS *fts;
	FTSENT *e;
	char path[8192];
	char * const path_argv[] = { path, NULL };
	short level;

	memset(p, 0, sizeof(*p));
	strlcpy(p->sid, sid, sizeof(p->sid));

	snprintf(path, sizeof(path), "%s/%s", datadir, sid);
	fts = fts_open(path_argv, FTS_LOGICAL|FTS_NOSTAT, NULL);
	if (fts == NULL)
		return;
	while ((e = fts_read(fts)) != NULL) {
		if (!(e->fts_info & FTS_D))
			continue;
		if (!strcmp(e->fts_name, pid)) {
			level = e->fts_level;
			break;
		}
	}
	fts_close(fts);
	if (e == NULL)
		return;
	fts = fts_open(path_argv, FTS_LOGICAL|FTS_NOSTAT, NULL);
	if (fts == NULL)
		return;
	while ((e = fts_read(fts)) != NULL) {
		if (!(e->fts_info & FTS_D))
			continue;
		if (e->fts_level > 0 && e->fts_level < level) {
			strlcpy(p->pid, e->fts_name, sizeof(p->pid));
			snprintf(p->fn, sizeof(p->fn), "%s/comment",
			    e->fts_path);
		}
		if (!strcmp(e->fts_name, pid))
			break;
	}
	fts_close(fts);
	if (e == NULL)
		p->pid[0] = 0;
	if (p->pid[0])
		if (read_file(p))
			memset(p, 0, sizeof(*p));
}

static void
find_comment_adjacent(const char *sid, const char *pid, struct entry *prev,
    struct entry *next)
{
	FTS *fts;
	FTSENT *e;
	char path[8192];
	char * const path_argv[] = { path, NULL };
	short level, found = 0;

	memset(prev, 0, sizeof(*prev));
	memset(next, 0, sizeof(*next));
	strlcpy(prev->sid, sid, sizeof(prev->sid));
	strlcpy(next->sid, sid, sizeof(next->sid));

	snprintf(path, sizeof(path), "%s/%s", datadir, sid);
	fts = fts_open(path_argv, FTS_LOGICAL|FTS_NOSTAT, NULL);
	if (fts == NULL)
		return;
	while ((e = fts_read(fts)) != NULL) {
		if (!(e->fts_info & FTS_D))
			continue;
		if (!strcmp(e->fts_name, pid))  {
			level = e->fts_level;
			break;
		}
	}
	fts_close(fts);
	if (e == NULL)
		return;
	fts = fts_open(path_argv, FTS_LOGICAL|FTS_NOSTAT, NULL);
	if (fts == NULL)
		return;
	while ((e = fts_read(fts)) != NULL) {
		if (e->fts_level <= 0 || e->fts_level > level ||
		    !(e->fts_info & FTS_D))
			continue;
		if (!strcmp(e->fts_name, pid)) {
			found = 1;
			continue;
		}
		if (!found) {
			strlcpy(prev->pid, e->fts_name, sizeof(prev->pid));
			snprintf(prev->fn, sizeof(prev->fn), "%s/comment",
			    e->fts_path);
		} else {
			strlcpy(next->pid, e->fts_name, sizeof(next->pid));
			snprintf(next->fn, sizeof(next->fn), "%s/comment",
			    e->fts_path);
			break;
		}
	}
	fts_close(fts);
	if (prev->pid[0])
		if (read_file(prev))
			memset(prev, 0, sizeof(*prev));
	if (next->pid[0])
		if (read_file(next))
			memset(next, 0, sizeof(*next));
}

static void
read_poll(const char *sid, int *a, int siz, int *sum)
{
	int i;
	char fn[MAXNAMLEN + 1];
	FILE *f;
	char s[8192];

	for (i = 0; i < siz; ++i)
		a[i] = 0;
	*sum = 0;
	snprintf(fn, sizeof(fn), "%s/%s/article.poll", datadir, sid);
	if ((f = fopen(fn, "r")) == NULL)
		return;
	while (fgets(s, sizeof(s), f)) {
		char host[128];
		int val;

		if (sscanf(s, "%127s %d", host, &val) == 2 &&
		    val >= 0 && val < siz)
			a[val]++;
	}
	for (i = 0; i < siz; ++i)
		*sum += a[i];
	fclose(f);
}

static int
rss_rate_limit(void)
{
	char fni[MAXNAMLEN + 1], fno[MAXNAMLEN + 1];
	FILE *fi, *fo;
	char s[8192];
	unsigned now = (unsigned)time(NULL), ts, count = 0;
	int exceeded = 1;

	snprintf(fno, sizeof(fno), "%s/rsslog/%s.tmp", datadir, q->remote_addr);
	if ((fo = fopen(fno, "w+")) == NULL) {
		msg("error rss_rate_limit: fopen: %s: %s",
		    q->remote_addr, strerror(errno));
		return (0);
	}
	snprintf(fni, sizeof(fni), "%s/rsslog/%s", datadir, q->remote_addr);
	if ((fi = fopen(fni, "r")) != NULL) {
		while (fgets(s, sizeof(s), fi)) {
			if (sscanf(s, "%u", &ts) != 1 ||
			    ts < (now - 60 * 60 * 24))
				continue;
			fprintf(fo, "%u\n", ts);
			count++;
		}
		fclose(fi);
	}
	if (count <= 24 * 4) {
		fprintf(fo, "%u\n", now);
		exceeded = 0;
	}
	fclose(fo);
	if (rename(fno, fni))
		msg("error rss_rate_limit: rename: %s: %s",
		    q->remote_addr, strerror(errno));
	return (exceeded);
}

static int
nopost_check(void)
{
	char s[256];
	FILE *f;

	f = fopen(nopostfile, "r");
	if (f == NULL) {
		msg("error nopost_check: fopen: %s: %s", nopostfile,
		    strerror(errno));
		return (0);
	}
	while (fgets(s, sizeof(s), f)) {
		s[strlen(s) - 1] = 0;
		if (!strcmp(s, q->remote_addr)) {
			fclose(f);
			return (1);
		}
	}
	fclose(f);
	return (0);
}

static int
vote_poll(const char *sid, int val)
{
	char fn[MAXNAMLEN + 1];
	FILE *f;
	char s[8192];

	snprintf(fn, sizeof(fn), "%s/%s/article.poll", datadir, sid);
	if ((f = fopen(fn, "a+")) == NULL)
		return (1);
	rewind(f);
	while (fgets(s, sizeof(s), f)) {
		char host[128];

		if (sscanf(s, "%127s", host) != 1)
			continue;
		if (!strcmp(host, q->remote_addr)) {
			fclose(f);
			return (1);
		}
	}
	fprintf(f, "%s %d\n", q->remote_addr, val);
	fclose(f);
	return (0);
}

static int
read_file(struct entry *e)
{
	FILE *f;
	char s[8192], fn[MAXNAMLEN + 1], *p;

	e->name[0] = e->host[0] = e->email[0] = e->href[0] = e->subject[0] =
	    e->date[0] = e->dept[0] = e->topic[0] = 0;
	e->ts = 0;
	e->modcount = e->modsum = e->modable = 0;
	e->pubdate = 0;
	if (!user_is_editor(&user) && e->sid[0] &&
	    strtoull(e->sid, NULL, 10) > timenow(0))
		return (1);
	if ((f = fopen(e->fn, "r")) == NULL)
		return (1);
	while (fgets(s, sizeof(s), f)) {
		if (s[0] == '\n' || !s[0])
			break;
		s[strlen(s) - 1] = 0;
		if (!strncmp(s, "name: ", 6))
			strlcpy(e->name, s + 6, sizeof(e->name));
		else if (!strncmp(s, "host: ", 6))
			strlcpy(e->host, s + 6, sizeof(e->host));
		else if (!strncmp(s, "email: ", 7))
			strlcpy(e->email, s + 7, sizeof(e->email));
		else if (!strncmp(s, "href: ", 6))
			strlcpy(e->href, s + 6, sizeof(e->href));
		else if (!strncmp(s, "subject: ", 9))
			strlcpy(e->subject, s + 9, sizeof(e->subject));
		else if (!strncmp(s, "date: ", 6)) {
			if (e->pubdate)
				continue;
			strlcpy(e->date, s + 6, sizeof(e->date));
			e->ts = convert_time(e->date);
		} else if (!strncmp(s, "dept: ", 6))
			strlcpy(e->dept, s + 6, sizeof(e->dept));
		else if (!strncmp(s, "topic: ", 7))
			strlcpy(e->topic, s + 7, sizeof(e->topic));
		else if (!strncmp(s, "poll:", 5))
			e->flags |= FLAG_POLL;
		else if (!strncmp(s, "pubdate: ", 9) && strlen(s) == 23) {
			struct tm tm;
			time_t t;

			e->pubdate = strtoull(s + 9, NULL, 10);
			memset(&tm, 0, sizeof(tm));
			tm.tm_sec = atoi(s + 21); s[21] = 0;
			tm.tm_min = atoi(s + 19); s[19] = 0;
			tm.tm_hour = atoi(s + 17); s[17] = 0;
			tm.tm_mday = atoi(s + 15); s[15] = 0;
			tm.tm_mon = atoi(s + 13) - 1; s[13] = 0;
			tm.tm_year = atoi(s + 9) - 1900;
			t = mktime(&tm);
			strlcpy(e->date, ctime(&t), sizeof(e->date));
			if (e->date[0])
				e->date[strlen(e->date) - 1] = 0;
			e->ts = convert_time(e->date);
		}
	}
	fclose(f);
	if ((p = strrchr(e->fn, '/')) == NULL || strcmp(p + 1, "comment"))
		return (0);
	e->modable = strcmp(e->host, q->remote_addr) ? 1 : 0;
	if (user_is_registered(&user) && !strcmp(user.id, e->host))
		e->modable = 0;
	snprintf(fn, sizeof(fn), "%s.mod", e->fn);
	if ((f = fopen(fn, "r")) == NULL)
		return (0);
	while (fgets(s, sizeof(s), f)) {
		char host[128];
		int val;

		if (sscanf(s, "%127s %d", host, &val) != 2)
			continue;
		if (val) {
			e->modcount++;
			e->modsum += val;
		}
		if (!strcmp(host, q->remote_addr) ||
		    (user_is_registered(&user) && !strcmp(user.id, host)))
			e->modable = 0;
	}
	fclose(f);
	return (0);
}

static unsigned
count_comments(const char *sid)
{
	FTS *fts;
	FTSENT *e;
	char path[8192];
	char * const path_argv[] = { path, NULL };
	unsigned count = 0;

	snprintf(path, sizeof(path), "%s/%s", datadir, sid);
	fts = fts_open(path_argv, FTS_LOGICAL|FTS_NOSTAT, NULL);
	if (fts == NULL)
		return (0);
	while ((e = fts_read(fts)) != NULL)
		if (e->fts_info & FTS_F && !strcmp(e->fts_name, "comment"))
			count++;
	fts_close(fts);
	return (count);
}

static int
get_lastread(const char *sid, time_t *t, int update)
{
	char filename[MAXPATHLEN], s[8192], *u, *d;
	FILE *f;
	long pos;

	snprintf(filename, sizeof(filename), "%s/%s/lastread", datadir, sid);
	if ((f = fopen(filename, update ? "r+" : "r")) != NULL) {
		pos = ftell(f);
		while (fgets(s, sizeof(s), f)) {	
			d = s;
			if ((u = strsep(&d, " \t")) != NULL && d != NULL
			    && !strcmp(user.id, u)) {
				d = strsep(&d, "\n\r ");
				*t = strtonum(d, 0, INT_MAX, NULL);
				if (update) {
					fseek(f, pos+strlen(user.id)+1,
					    SEEK_SET);
					fprintf(f, "%.10d", time(NULL));
				}
				fclose(f);
				return (1);
			}
			pos = ftell(f);
		}
		fclose(f);
	}
	if (update) {
		if ((f = fopen(filename, "a")) != NULL) {
			fprintf(f, "%s %.10d\n", user.id, time(NULL));
			fclose(f);
		}
	}
	*t = 0;
	return (0);
}

static time_t
find_newest_change(const char *sid)
{
	time_t t = 0;
	struct stat sb;
	FTS *fts;
	FTSENT *e;
	char path[8192];
	char * const path_argv[] = { path, NULL };

	snprintf(path, sizeof(path), "%s/%s/article", datadir, sid);
	if (!stat(path, &sb) && sb.st_mtime > t)
		t = sb.st_mtime;
	snprintf(path, sizeof(path), "%s/%s/article.more", datadir, sid);
	if (!stat(path, &sb) && sb.st_mtime > t)
		t = sb.st_mtime;
	snprintf(path, sizeof(path), "%s/%s", datadir, sid);
	fts = fts_open(path_argv, FTS_LOGICAL, NULL);
	if (fts == NULL)
		return (t);
	while ((e = fts_read(fts)) != NULL)
		if ((e->fts_info & FTS_F) && (!strcmp(e->fts_name, "comment") ||
		    !strcmp(e->fts_name, "commend.mod")))
			if (e->fts_statp && e->fts_statp->st_mtime > t)
				t = e->fts_statp->st_mtime;
	fts_close(fts);
	return (t);
}

static unsigned
find_max_pid(const char *sid)
{
	FTS *fts;
	FTSENT *e;
	char path[8192];
	char * const path_argv[] = { path, NULL };
	unsigned maxpid = 0;

	snprintf(path, sizeof(path), "%s/%s", datadir, sid);
	fts = fts_open(path_argv, FTS_LOGICAL|FTS_NOSTAT, NULL);
	if (fts == NULL)
		return (0);
	while ((e = fts_read(fts)) != NULL)
		if (e->fts_level > 0 && e->fts_info & FTS_D) {
			unsigned pid = atoi(e->fts_name);

			if (pid > maxpid)
				maxpid = pid;
		}
	fts_close(fts);
	return (maxpid);
}

static int
find_pid_path(const char *sid, const char *pid, char *path, int size)
{
	FTS *fts;
	FTSENT *e;
	char * const path_argv[] = { path, NULL };

	snprintf(path, size, "%s/%s", datadir, sid);
	fts = fts_open(path_argv, FTS_LOGICAL|FTS_NOSTAT, NULL);
	if (fts == NULL) {
		path[0] = 0;
		return (1);
	}
	while ((e = fts_read(fts)) != NULL)
		if (e->fts_info & FTS_D && !strcmp(e->fts_name, pid)) {
			strlcpy(path, e->fts_path, size);
			break;
		}
	fts_close(fts);
	if (e == NULL) {
		path[0] = 0;
		return (1);
	}
	return (0);
}

static int
create_article(struct entry *ce)
{
	int fd = -1, retry = 0;
	char path[MAXNAMLEN + 1];

	snprintf(path, sizeof(path), "%s/submissions", datadir);
	while (retry < 3) {
		time_t t = time(NULL);
		struct tm *tm = gmtime(&t);

		snprintf(ce->fn, sizeof(ce->fn),
		    "%s/%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d", path,
		    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		    tm->tm_hour, tm->tm_min, tm->tm_sec);
		if (!mkdir(ce->fn, 0775)) {
			strlcat(ce->fn, "/article", sizeof(ce->fn));
			fd = open(ce->fn, O_CREAT|O_TRUNC|O_WRONLY|O_EXLOCK,
			    0644);
			if (fd >= 0)
				break;
		}
		retry++;
		sleep(1);
	}
	return (fd);
}

static int
create_comment(struct entry *ce)
{
	char path[MAXNAMLEN + 1];
	int fd = -1, retry = 0;

	if (!ce->pid[0])
		snprintf(path, sizeof(path), "%s/%s", datadir, ce->sid);
	else if (find_pid_path(ce->sid, ce->pid, path, sizeof(path)))
		return (-1);
	while (retry < 3) {
		snprintf(ce->fn, sizeof(ce->fn), "%s/%u", path,
		    find_max_pid(ce->sid) + 1 + retry * 10);
		if (!mkdir(ce->fn, 0775)) {
			strlcat(ce->fn, "/comment", sizeof(ce->fn));
			fd = open(ce->fn, O_CREAT|O_TRUNC|O_WRONLY|O_EXLOCK,
			    0664);
			if (fd >= 0)
				break;
			else
				rmdir(ce->fn);
		}
		retry++;
	}
	return (fd);
}

static int
moderate_comment(const char *sid, const char *pid, int val)
{
	char path[MAXNAMLEN + 1];
	FILE *f;
	char s[8192];

	if (find_pid_path(sid, pid, path, sizeof(path)))
		return (1);
	strlcat(path, "/comment.mod", sizeof(path));
	if ((f = fopen(path, "a+")) == NULL)
		return (1);
	rewind(f);
	while (fgets(s, sizeof(s), f)) {
		char host[128];

		if (sscanf(s, "%127s", host) != 1)
			continue;
		if (!strcmp(host, q->remote_addr) ||
		    (user_is_registered(&user) && !strcmp(host, user.id))) {
			fclose(f);
			return (1);
		}
	}
	fprintf(f, "%s %d\n", q->remote_addr, val);
	if (user_is_registered(&user))
		fprintf(f, "%s 0\n", user.id);
	fclose(f);
	return (0);
}

static int
delete_comment(const char *sid, const char *pid)
{
	FTS *fts;
	FTSENT *e;
	char path[8192], parent[8192], *p;
	char * const path_argv[] = { path, NULL };
	DIR *dirp;
	struct dirent *dp;

	/* first, find the pid subdirectory */
	snprintf(path, sizeof(path), "%s/%s", datadir, sid);
	fts = fts_open(path_argv, FTS_LOGICAL|FTS_NOSTAT, NULL);
	if (fts == NULL)
		return (1);
	while ((e = fts_read(fts)) != NULL)
		if (e->fts_info & FTS_D && !strcmp(e->fts_name, pid)) {
			strlcpy(path, e->fts_path, sizeof(path));
			break;
		}
	fts_close(fts);
	if (e == NULL)
		return (1);
	strlcpy(parent, path, sizeof(parent));
	if ((p = strrchr(parent, '/')) == NULL)
		return (1);
	*p = 0;
	/* now, move all sub-comments of pid up one level */
	dirp = opendir(path);
	if (dirp == NULL)
		return (1);
	while ((dp = readdir(dirp)) != NULL) {
		char below[8192];

		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		snprintf(below, sizeof(below), "%s/%s", path, dp->d_name);
		if (dp->d_type == DT_REG && (!strcmp(dp->d_name, "comment") ||
		    !strcmp(dp->d_name, "comment.mod"))) {
			if (unlink(below)) {
				msg("error delete_comment: unlink: %s: %s",
				    below, strerror(errno));
				closedir(dirp);
				return (1);
			}
		} else if (dp->d_type != DT_DIR || !atoi(dp->d_name)) {
			msg("error delete_comment: unexpected file %s", below);
			closedir(dirp);
			return (1);
		} else {
			char above[8192];

			snprintf(above, sizeof(above), "%s/%s", parent,
			    dp->d_name);
			if (rename(below, above)) {
				msg("error delete_comment: rename: %s: %s: %s",
				    below, above, strerror(errno));
				closedir(dirp);
				return (1);
			}
		}
	}
	closedir(dirp);
	if (rmdir(path)) {
		msg("error delete_comment: rmdir: %s: %s", path,
		    strerror(errno));
		return (1);
	}
	return (0);
}

static char *
de_html(const char *s, char *d, size_t len)
{
	size_t p;
	int opened = 0;

	for (p = 0; *s && p < len - 1; ++s)
		switch (*s) {
		case '<':
			opened++;
			break;
		case '>':
			if (opened > 0)
				opened--;
			break;
		case '&':
			if (!strncmp(s, "&amp;", 5)) {
				if (!opened)
					d[p++] = '&';
				s += 4;
			} else if (!strncmp(s, "&quot;", 6)) {
				if (!opened)
					d[p++] = '\"';
				s += 5;
			} else if (!strncmp(s, "&lt;", 4)) {
				if (!opened)
					d[p++] = '<';
				s += 3;
			} else if (!strncmp(s, "&gt;", 4)) {
				if (!opened)
					d[p++] = '>';
				s += 3;
			} else
				while (*s && *s != ';')
					s++;
			break;
		default:
			if (opened <= 0)
				d[p++] = *s;
		}
	d[p] = 0;
	return (d);
}

static char *
html_esc(const char *s, char *d, size_t len, int allownl)
{
	size_t p;

	for (p = 0; *s && p < len - 1; ++s)
		switch (*s) {
		case '&':
			if (p < len - 5) {
				strlcpy(d + p, "&amp;", 6);
				p += 5;
			}
			break;
		case '\"':
			if (p < len - 6) {
				strlcpy(d + p, "&quot;", 7);
				p += 6;
			}
			break;
		case '<':
			if (p < len - 4) {
				strlcpy(d + p, "&lt;", 5);
				p += 4;
			}
			break;
		case '>':
			if (p < len - 4) {
				strlcpy(d + p, "&gt;", 5);
				p += 4;
			}
			break;
		case '\r':
		case '\n':
			if (!allownl) {
				/* skip */
				break;
			} else if (allownl > 1 && *s == '\r') {
				if (p < len - 4) {
					strlcpy(d + p, "<br>", 5);
					p += 4;
				}
				break;
			}
			/* else fall through */
		default:
			d[p++] = *s;
		}
	d[p] = 0;
	return (d);
}

static int
html_validate(const char *d)
{
	int balance = 0, inpre = 0;

	while (*d) {
		int closing = 0;
		char name[256], opts[256];
		int i;

		while (*d && *d != '<')
			d++;
		if (!*d)
			break;
		d++;
		while (*d == ' ' || *d == '\t')
			d++;
		if (!*d)
			return (1);
		if (*d == '!' && !inpre)
			return (1);
		if (*d == '/') {
			closing = 1;
			d++;
			while (*d == ' ' || *d == '\t')
				d++;
		}
		for (i = 0; i < sizeof(name) - 1 &&
		    *d && *d != '>' && *d != ' ' && *d != '\t'; ++i)
			name[i] = *d++;
		if (!*d || i == sizeof(name) - 1)
			return (1);
		name[i] = 0;
		while (*d == ' ' || *d == '\t')
			d++;
		for (i = 0; i < sizeof(opts) - 1 && *d && *d != '>'; ++i)
			opts[i] = *d++;
		if (!*d || i == sizeof(opts) - 1)
			return (1);
		if (inpre) {
			if (closing && !strcasecmp(name, "pre"))
				inpre = 0;
		} else {
			if (!strcasecmp(name, "pre")) {
				if (closing)
					return (1);
				else
					inpre = 1;
			} else if (!strcasecmp(name, "br") ||
			    !strcasecmp(name, "p")) {
				/* ignore */
			} else if (!strcasecmp(name, "i") ||
			    !strcasecmp(name, "b") ||
			    !strcasecmp(name, "em") ||
			    !strcasecmp(name, "a")) {
				if (closing)
					balance--;
				else
					balance++;
			} else
				return (1);
		}
	}
	return (balance);
}

static int
compare_name_asc_fts(const FTSENT **a, const FTSENT **b)
{
	return (strcmp((*a)->fts_name, (*b)->fts_name));
}

static int
compare_name_des_fts(const FTSENT **a, const FTSENT **b)
{
	return (strcmp((*b)->fts_name, (*a)->fts_name));
}

/* sort comment threads for find_comments() */
static int
compare_fts(const FTSENT **a, const FTSENT **b)
{
	int na = atoi((*a)->fts_name), nb = atoi((*b)->fts_name);

	/* sort comment threads by pid ascending */
	if (na == nb)
		return (0);
	return (na < nb ? -1 : 1);
}

/* sort comments chronologically for render_article() */
static int
compare_qsort(const void *va, const void *vb)
{
	const struct entry *a = (const struct entry *)va;
	const struct entry *b = (const struct entry *)vb;

	/* empty entries last */
	if (!a->pid[0] != !b->pid[0])
		return (a->pid[0] ? -1 : 1);
	/* non-empty entries by ts descending */
	if (a->ts != b->ts)
		return (a->ts < b->ts ? -1 : 1);
	return (0);
}

static const char *
rfc822_time(time_t t)
{
	static char s[30], *p;

	p = ctime(&t);
	if (p == NULL || strlen(p) != 25) {
		strlcpy(s, "<invalid-time>", sizeof(s));
		return (s);
	}
	/* Thu Nov 24 18:22:48 1986\n */
	/* Wed, 02 Oct 2002 13:00:00 GMT */
	strlcpy(s, p, 4);
	strlcat(s, ", ", 6);
	strlcat(s, p + 8, 9);
	strlcat(s, p + 4, 13);
	strlcat(s, p + 20, 17);
	strlcat(s, " ", 18);
	strlcat(s, p + 11, 26);
	strlcat(s, " GMT", 30);
	return (s);
}

static const char *
duration_str(unsigned d)
{
	static char s[128];
	int zero = 0;

	s[0] = 0;
	if (d >= 24 * 60 * 60) {
		snprintf(s, sizeof(s), "%ud", d / (24 * 60 * 60));
		d = d % (24 * 60 * 60);
	}
	if (d >= 60 * 60) {
		snprintf(s + strlen(s), sizeof(s) - strlen(s), "%u:",
		    d / (60 * 60));
		d = d % (60 * 60);
		zero = 1;
	}
	if (zero)
		snprintf(s + strlen(s), sizeof(s) - strlen(s), "%2.2u",
		    d / 60);
	else
		snprintf(s + strlen(s), sizeof(s) - strlen(s), "%um",
		    d / 60);
	return (s);
}

static time_t
convert_rfc822_time(const char *date)
{
	const char *mns[13] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
	    "Aug", "Sep", "Oct", "Nov", "Dec", NULL };
	char wd[4], mn[4], zone[16];
	int d, h, m, s, y, i;
	struct tm tm;
	time_t t;

	if (sscanf(date, "%3s, %d %3s %d %d:%d:%d %15s",
	    wd, &d, mn, &y, &h, &m, &s, zone) != 8)
		return (0);
	for (i = 0; mns[i] != NULL; ++i)
		if (!strcmp(mns[i], mn))
			break;
	if (mns[i] == NULL)
		return (0);
	memset(&tm, 0, sizeof(tm));
	tm.tm_year = y - 1900;
	tm.tm_mon = i;
	tm.tm_mday = d;
	tm.tm_hour = h;
	tm.tm_min = m;
	tm.tm_sec = s;
	tm.tm_zone = zone;
	t = mktime(&tm);
	return (t);
}

/* convert ctime(3) output back to time_t */
static time_t
convert_time(const char *date)
{
	const char *mns[13] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
	    "Aug", "Sep", "Oct", "Nov", "Dec", NULL };
	char wd[4], mn[4];
	int d, h, m, s, y, i;
	struct tm tm;
	time_t t;

	if (sscanf(date, "%3s %3s %d %d:%d:%d %d",
	    wd, mn, &d, &h, &m, &s, &y) != 7)
		return (0);
	for (i = 0; mns[i] != NULL; ++i)
		if (!strcmp(mns[i], mn))
			break;
	if (mns[i] == NULL)
		return (0);
	memset(&tm, 0, sizeof(tm));
	tm.tm_year = y - 1900;
	tm.tm_mon = i;
	tm.tm_mday = d;
	tm.tm_hour = h;
	tm.tm_min = m;
	tm.tm_sec = s;
	t = mktime(&tm);
	return (t);
}

static int
read_post_data(char *buf, unsigned siz)
{
	int fd = fileno(stdin), len = -1, pos = 0, r;
	const char *content_length;
	time_t t = time(NULL);

	if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)) {
		msg("error read_post_data: fcntl: %s", strerror(errno));
		return (1);
        }
	if ((content_length = getenv("CONTENT_LENGTH")) != NULL)
		len = atoi(content_length);
	if (len < 0 || len > siz - 1)
		len = siz - 1;
	while (pos < len && time(NULL) - t < 10) {
		fd_set fds;
		struct timeval tv;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		r = select(fd + 1, &fds, NULL, NULL, &tv);
		if (r < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			msg("error read_post_data: select: %s", strerror(errno));
			return (1);
		}
		if (r == 0 || !FD_ISSET(fd, &fds))
			continue;
		r = read(fd, buf + pos, len - pos);
		if (r < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			msg("error read_post_data: read: %s", strerror(errno));
			return (1);
		}
		if (r == 0) {
			msg("warning read_post_data: read() returned 0, "
			    "pos %d, len %d", pos, len);
			len = pos;
			break;
		}
		pos += r;
	}
	if (pos < len) {
		msg("warning read_post_data: read timeout (%d < %d)", pos, len);
		return (1);
	}
	buf[pos] = 0;
	return (0);
}

static int
read_post(struct entry *e, int *preview)
{
	char buf[65536];
	struct query pq;
	const char *sid, *pid;
	time_t t = time(NULL);
	const char *name, *email, *href, *subject, *host;
	const char *data, *more, *type, *poll, *pubdate;
	const char *captcha;

	memset(e, 0, sizeof(*e));
	if (read_post_data(buf, sizeof(buf)))
		return (1);
	memset(&pq, 0, sizeof(pq));
	tokenize_query_params(buf, &pq.params);
	mode = get_query_param(&pq, "mode");
	thres = get_query_param(&pq, "thres");
	*preview = get_query_param(&pq, "preview") != NULL;
	sid = get_query_param(&pq, "sid");
	pid = get_query_param(&pq, "pid");
	name = get_query_param(&pq, "name");
	if (!name || strchr(name, '\r') || strchr(name, '\n'))
		return (1);
	email = get_query_param(&pq, "email");
	if (!email || strchr(email, '\r') || strchr(email, '\n'))
		return (1);
	href = get_query_param(&pq, "href");
	if (!href || strchr(href, '\r') || strchr(href, '\n'))
		return (1);
	subject = get_query_param(&pq, "subject");
	if (!subject || strchr(subject, '\r') || strchr(subject, '\n'))
		return (1);
	type = get_query_param(&pq, "type");
	if (type && !strcmp(type, "plain"))
		e->flags |= FLAG_PLAIN;
	poll = get_query_param(&pq, "poll");
	if (poll && !strcmp(poll, "yes"))
		e->flags |= FLAG_POLL;
	pubdate = get_query_param(&pq, "pubdate");
	if (pubdate) {
		if (pubdate[0] == '+')
			e->pubdate = timenow(time(NULL) +
			    atoi(pubdate + 1) * 60);
		else
			e->pubdate = strtoull(pubdate, NULL, 10);
	}
	data = get_query_param(&pq, "data");
	if (!data || (!user_is_editor(&user) && !(e->flags & FLAG_PLAIN) &&
	    html_validate(data)))
		return (1);
	more = get_query_param(&pq, "more");
	if (more && !user_is_editor(&user) && !(e->flags & FLAG_PLAIN) &&
	    html_validate(more))
		return (1);
	captcha = get_query_param(&pq, "cksum");
	if (captcha)
		strlcpy(captcha_cksum, captcha, sizeof(captcha_cksum));
	captcha = get_query_param(&pq, "captcha");
	if (captcha)
		strlcpy(captcha_text, captcha, sizeof(captcha_cksum));

	if (sid != NULL)
		strlcpy(e->sid, sid, sizeof(e->sid));
	if (pid != NULL)
		strlcpy(e->pid, pid, sizeof(e->pid));
	strlcpy(e->name, name[0] ? name : "Anonymous Coward", sizeof(e->name));
	if ((host = getenv("REMOTE_ADDR")) != NULL)
		strlcpy(e->host, host, sizeof(e->host));
	strlcpy(e->email, email, sizeof(e->email));
	strlcpy(e->href, href, sizeof(e->href));
	strlcpy(e->date, ctime(&t), sizeof(e->date));
	e->date[strlen(e->date) - 1] = 0;
	strlcpy(e->subject, subject[0] ? subject : "No Subject Given",
	    sizeof(e->subject));
	e->body = strdup(data);
	e->more = more == NULL ? NULL : strdup(more);
	if (get_query_param(&pq, "edit") != NULL) {
		const char *topic = get_query_param(&pq, "topic");
		const char *dept = get_query_param(&pq, "dept");
		const char *submission = get_query_param(&pq, "submission");

		e->flags |= FLAG_EDIT;
		if (!topic || strchr(topic, '\r') || strchr(topic, '\n'))
			return (1);
		strlcpy(e->topic, topic, sizeof(e->topic));
		if (!dept || strchr(dept, '\r') || strchr(dept, '\n'))
			return (1);
		strlcpy(e->dept, dept, sizeof(e->dept));
		if (submission != NULL && atoi(submission))
			e->flags |= FLAG_SUBMISSION;
	}
	return (0);
}

static void
check_auth(void)
{
	char *c, *d;
	int i;
	time_t t;
	char u[256], host[256], sig[256], salt[256], sec[256], hash[256];
	FILE *f;
	SHA1_CTX ctx;

	if ((c = getenv("HTTP_COOKIE")) == NULL || (d = strdup(c)) == NULL)
		return;
	c = d;
	while (*c == ' ')
		c++;
	while (c && *c && strncmp(c, "auth=", 5)) {
		c = strchr(c, ';');
		while (c && (*c == ';' || *c == ' '))
			c++;
	}
	if (!c || !*c) {
		free(d);
		return;
	}
	c += 5;
	while ((i = strlen(c) > 0) && (c[i - 1] == ';' || c[i - 1] == ' '))
		c[i - 1] = 0;
	memset(u, 0, sizeof(u));
	memset(host, 0, sizeof(host));
	memset(sig, 0, sizeof(sig));
	if (sscanf(c, "%255[^/]/%u/%255[^/]/%255[^ ;]", u, &t, host, sig) !=
	    4) {
		if (c[0])
			msg("check_auth: malformed cookie '%s'", c);
		free(d);
		return;
	}
	free(d);
	if (t < time(NULL)) {
		msg("check_auth: cookie expired %u > %u", time(NULL), t);
		return;
	}
	if (strcmp(host, q->remote_addr)) {
		msg("check_auth: cookie address %s != %s",
		    q->remote_addr, host);
		return;
	}
	strlcpy(user.id, u, sizeof(user.id));
	if (user_find(passwd, &user)) {
		memset(&user, 0, sizeof(user));
		msg("check_auth: id '%s' not in file %s", u, passwd);
		return;
	}
	snprintf(salt, sizeof(salt), "%s/%u/%s", u, t, host);
	if ((f = fopen(secret, "r")) == NULL) {
		memset(&user, 0, sizeof(user));
		msg("error fopen: %s: %s", secret, strerror(errno));
		return;
	}
	sec[0] = 0;
	if (!fgets(sec, sizeof(sec), f) || !sec[0]) {
		memset(&user, 0, sizeof(user));
		msg("error fgets: %s: %s", secret, strerror(errno));
		return;
	}
	sec[strlen(sec) - 1] = 0;
	fclose(f);
	SHA1Init(&ctx);
	SHA1Update(&ctx, salt, strlen(salt));
	SHA1Update(&ctx, sec, strlen(sec));
	SHA1End(&ctx, hash);
	if (strcmp(hash, sig)) {
		memset(&user, 0, sizeof(user));
		msg("check_auth: cookie signature %s != %s", sig, hash);
		return;
	}
}

int main(int argc, char *argv[])
{
	const char *action, *s;
	static struct timeval tx;
	time_t if_modified_since = 0;

	memset(&user, 0, sizeof(user));
	gettimeofday(&tx, NULL);
	umask(007);
	if (load_conf(conffile, confentries)) {
		char cwd[MAXPATHLEN];

		msg("error load_conf: cwd '%s' file '%s'", cwd, conffile);
		render_error("load_conf: cwd '%s' file '%s'",
		    getcwd(cwd, MAXPATHLEN), conffile);
		goto done;
	}
	if (chdir("/tmp")) {
		msg("error main: chdir: /tmp: %s", strerror(errno));
		render_error("chdir: /tmp: %s", strerror(errno));
		goto done;
	}
	if (!access(corefile, R_OK)) {
		struct stat sb;

		if (stat(corefile, &sb))
			msg("error main: stat: %s: %s", corefile,
			    strerror(errno));
		else {
			char fn[1024];
			struct tm *tm = gmtime(&sb.st_mtime);

			snprintf(fn, sizeof(fn),
			    "%s.%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d", corefile,
			    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			    tm->tm_hour, tm->tm_min, tm->tm_sec);
			if (rename(corefile, fn))
				msg("error main: rename: %s: %s: %s", corefile,
				    fn, strerror(errno));
			else
				msg("warning main: core file %s renamed to %s",
				    corefile, fn);
		}
	}
	if ((q = get_query()) == NULL) {
		render_error("get_query");
		msg("error main: get_query() NULL");
		goto done;
	}
	check_auth();
	if ((s = getenv("QUERY_STRING")) != NULL &&
	    strstr(s, "&amp;") != NULL) {
		msg("warning main: escaped query '%s', user agent '%s', "
		    "referer '%s'", s,
		    q->user_agent ? q->user_agent : "(null)",
		    q->referer ? q->referer : "(null)");
		printf("Status: 400\r\n\r\nHTML escaped ampersand in cgi "
		    "query string \"%s\"\n"
		    "This might be a problem in your client \"%s\",\n"
		    "or in the referring document \"%s\"\n"
		    "See http://www.htmlhelp.org/tools/validator/problems.html"
		    "#amp\n", s, q->user_agent ? q->user_agent : "",
		    q->referer ? q->referer : "");
		fflush(stdout);
		return (0);
	}
	if ((q->referer != NULL && strstr(q->referer, "morisit")) ||
	    (s != NULL && strstr(s, "http://"))) {
		printf("Status: 503\r\n\r\nWe are not redirecting, "
		    "nice try.\n");
		fflush(stdout);
		return (0);
	}
	if (q->user_agent != NULL && !strncmp(q->user_agent, "Googlebot", 9)) {
		printf("Status: 503\r\n\r\nGooglebot you are not.\n");
		fflush(stdout);
		return (0);
	}
	mode = get_query_param(q, "mode");
	thres = get_query_param(q, "thres");
	if (thres != NULL && (thres[0] == '\n' || thres[0] == '\r'))
		thres = NULL;

	if ((action = get_query_param(q, "action")) == NULL)
		action = "front";

	if ((s = getenv("IF_MODIFIED_SINCE")) != NULL) {
		if_modified_since = convert_rfc822_time(s);
		if (!if_modified_since)
			if_modified_since =
			    (time_t)strtoul(s, NULL, 10);
		if (!if_modified_since)
			msg("warning main: invalid IF_MODIFIED_SINCE '%s'", s);
	}
	if ((s = getenv("HTTP_ACCEPT_ENCODING")) != NULL) {
		char *p = strstr(s, "gzip");

		if (p != NULL && (strncmp(p, "gzip;q=0", 8) ||
		    atoi(p + 7) > 0.0)) {
			gz = gzdopen(fileno(stdout), "wb9");
			if (gz == NULL)
				msg("error main: gzdopen");
			else
				printf("Content-Encoding: gzip\r\n");
		}
	}

	if (!strcmp(action, "front")) {
		const char *d = get_query_param(q, "date");
		unsigned long long date = 0;
		char fn[1024];
		struct entry e;

		if (d != NULL) {
			char e[9];

			strlcpy(e, d, sizeof(e));
			date = strtoull(e, NULL, 10) * 1000000;
		}
		strlcpy(fn, datadir, sizeof(fn));
		find_articles(fn, date, newest, 18, 0, &poll);
		printf("%s\r\n\r\n", ct_html);
		fflush(stdout);
		snprintf(fn, sizeof(fn), "%s/front.html", htmldir);
		memset(&e, 0, sizeof(e));
		e.flags = FLAG_FRONT;
		render_html(fn, &render_front, &e);
	} else if (!strcmp(action, "submissions") && user_is_editor(&user)) {
		char fn[1024];
		struct entry e;

		snprintf(fn, sizeof(fn), "%s/submissions", datadir);
		find_articles(fn, 0, newest, sizeof(newest) /
		    sizeof(newest[0]), 1, NULL);
		printf("%s\r\n\r\n", ct_html);
		fflush(stdout);
		snprintf(fn, sizeof(fn), "%s/submissions.html", htmldir);
		memset(&e, 0, sizeof(e));
		e.flags = FLAG_FRONT | FLAG_SUBMISSION;
		render_html(fn, &render_front, &e);
		msg("submissions viewed");
	} else if (!strcmp(action, "rss")) {
		char fn[1024], wd[4], m[4], d[3], y[5], t[9];
		struct entry e;

		strlcpy(fn, datadir, sizeof(fn));
		find_articles(fn, 0, newest, 10, 0, NULL);
		if (newest[0].sid[0] && sscanf(newest[0].date,
		    "%3s %3s %2s %8s %4s", wd, m, d, t, y) == 5)
			printf("Last-Modified: %s, %s %s %s %s GMT\r\n",
			    wd, d, m, y, t);
		if (if_modified_since &&
		    if_modified_since >= convert_time(newest[0].date)) {
			printf("Status: 304\r\n\r\n");
			fflush(stdout);
			goto done;
		}
		if (rss_rate_limit()) {
			printf("Status: 503\r\n\r\n");
			fflush(stdout);
			goto done;
		}
		printf("Content-Type: text/xml\r\n\r\n");
		fflush(stdout);
		snprintf(fn, sizeof(fn), "%s/summary.rss", htmldir);
		memset(&e, 0, sizeof(e));
		render_html(fn, &render_rss, &e);
	} else if (!strcmp(action, "errata")) {
		char fn[1024];
		struct entry e;

		printf("Content-Type: text/xml\r\n\r\n");
		fflush(stdout);
		snprintf(fn, sizeof(fn), "%s/errata.rss", htmldir);
		memset(&e, 0, sizeof(e));
		render_html(fn, &render_rss_errata, &e);
	} else if (!strcmp(action, "article")) {
		time_t last_modified = 0;
		struct entry e;
		const char *sid = get_query_param(q, "sid");
		const char *pid = get_query_param(q, "pid");
		char fn[1024];

		memset(&e, 0, sizeof(e));
		if (sid == NULL || strlen(sid) > 14) {
			render_error("missing/invalid sid");
			if (sid != NULL && sid[0])
				msg("error article: invalid sid '%s'", sid);
			goto done;
		}
		if (if_modified_since && if_modified_since >=
		    (last_modified = find_newest_change(sid))) {
			printf("Status: 304\r\nLast-Modified: %s\r\n\r\n",
			    rfc822_time(last_modified));
			fflush(stdout);
			goto done;
		}
		strlcpy(e.sid, sid, sizeof(e.sid));
		snprintf(e.fn, sizeof(e.fn), "%s/%s/article", datadir, sid);
		if (read_file(&e)) {
			e.flags |= FLAG_SUBMISSION;
			snprintf(e.fn, sizeof(e.fn),
			    "%s/submissions/%s/article", datadir, sid);
			if (read_file(&e)) {
				render_error("sid file not found");
				msg("error article: read_file: sid '%s'", sid);
				goto done;
			}
		}
		if (pid != NULL && strlen(pid) <= 14)
			strlcpy(e.pid, pid, sizeof(e.pid));
		if (mode != NULL) {
			if (!strcmp(mode, "flat"))
				e.flags |= FLAG_FLAT;
			else if (!strcmp(mode, "expanded"))
				e.flags |= FLAG_EXPANDED;
		}

		printf("%s\r\n\r\n", ct_html);
		fflush(stdout);
		if (pid != NULL && pid[0])
			snprintf(fn, sizeof(fn), "%s/comment.html", htmldir);
		else
			snprintf(fn, sizeof(fn), "%s/article.html", htmldir);
		render_html(fn, &render_article, &e);
	} else if (!strcmp(action, "vote")) {
		char buf[65536];
		struct query pq;
		const char *sid, *val;

		if (read_post_data(buf, sizeof(buf)))
			goto done;
		memset(&pq, 0, sizeof(pq));
		tokenize_query_params(buf, &pq.params);
		sid = get_query_param(&pq, "sid");
		val = get_query_param(&pq, "val");
		if (sid == NULL || strlen(sid) > 14) {
			render_error("missing/invalid sid");
			if (sid != NULL && sid[0])
				msg("error vote: invalid sid %s", sid);
			goto done;
		}
		if (val == NULL || (atoi(val) < 0 || atoi(val) > 255)) {
			render_error("missing/invalid val");
			msg("error vote: invalid val '%s'",
			    val == NULL ? "(null)" : val);
			goto done;
		}
		if (nopost_check()) {
			msg("warning vote: nopost_check");
			printf("Status: 403\r\n\r\n");
			fflush(stdout);
			goto done;
		}
		if (!vote_poll(sid, atoi(val)))
			msg("vote sid %s val %d", sid, atoi(val));
		render_redirect("%s", url(0, "article", sid, NULL, NULL));
	} else if (!strcmp(action, "redirect")) {
		render_redirect("%s", url(0, "front", NULL, NULL, NULL));
	} else if (!strcmp(action, "moderate")) {
		const char *sid = get_query_param(q, "sid");
		const char *pid = get_query_param(q, "pid");
		const char *val = get_query_param(q, "val");

		if ((sid == NULL || strlen(sid) > 14) ||
		    (pid == NULL || strlen(pid) > 14)) {
			render_error("missing/invalid sid or pid");
			msg("error moderate: invalid sid '%s' pid '%s'",
			    sid == NULL ? "(null)" : sid,
			    pid == NULL ? "(null)" : pid);
			goto done;
		}
		if (val == NULL || (atoi(val) != 1 && atoi(val) != -1)) {
			render_error("missing/invalid val");
			msg("error moderate: invalid val '%s'",
			    val == NULL ? "(null)" : val);
			goto done;
		}
		if (nopost_check()) {
			msg("warning moderate: nopost_check");
			printf("Status: 403\r\n\r\n");
			fflush(stdout);
			goto done;
		}
		if (moderate_comment(sid, pid, atoi(val))) {
			render_error("moderate_comment");
			msg("error moderate: moderate_comment");
			goto done;
		}
		msg("moderation sid %s pid %s val %d", sid, pid, atoi(val));
		render_redirect("%s", url(0, "article", sid, NULL, NULL));
	} else if (!strcmp(action, "publish") && user_is_editor(&user)) {
		const char *sid = get_query_param(q, "sid");
		char fns[8192], fnp[8192];
		struct entry e;
		int retry = 0;

		if (sid == NULL || strlen(sid) != 14 || strchr(sid, '.') ||
		    strchr(sid, '/')) {
			render_error("missing/invalid sid");
			msg("error publish: invalid sid '%s'",
			    sid == NULL ? "(null)" : sid);
			goto done;
		}
		snprintf(e.fn, sizeof(e.fn), "%s/submissions/%s/article",
		    datadir, sid);
		if (read_file(&e)) {
			render_error("sid not found in submissions");
			msg("error publish: read_file: %s", e.fn);
			goto done;
		}
		snprintf(fns, sizeof(fns), "%s/submissions/%s", datadir, sid);
		while (retry < 3) {
			unsigned long long now = timenow(0);

			if (e.pubdate < now)
				e.pubdate = now;
			snprintf(fnp, sizeof(fnp), "%s/%llu", datadir,
			    e.pubdate);
			if (!mkdir(fnp, 0775)) {
				if (rename(fns, fnp)) {
					render_error("rename: %s %s: %s",
					    fns, fnp, strerror(errno));
					msg("error publish: rename: %s %s: %s",
					    fns, fnp, strerror(errno));
					goto done;
				}
				break;
			} else
				msg("error publish: mkdir: %s: %s", fnp,
				    strerror(errno));
			retry++;
			sleep(1);
		}
		if (retry == 3) {
			render_error("couldn't allocate new sid");
			msg("error publish: sid allocation");
			goto done;
		}
		render_redirect("%s", url(0, "front", NULL, NULL, NULL));
		msg("published file %s", fnp);
	} else if (!strcmp(action, "delete") && user_is_editor(&user)) {
		const char *sid = get_query_param(q, "sid");
		const char *pid = get_query_param(q, "pid");
		char fn[8192];

		if (sid == NULL || strlen(sid) != 14 || strchr(sid, '.') ||
		    strchr(sid, '/')) {
			render_error("missing/invalid sid");
			msg("error delete: invalid sid %s",
			    sid == NULL ? "(null)" : sid);
			goto done;
		}
		if (pid != NULL && pid[0] && !strchr(pid, '.') &&
		    !strchr(pid, '/')) {
			if (!user_is_admin(&user)) {
				render_error("Only admins can delete comments");
				msg("warning delete: attempt deletion comment "
				    "sid %s pid %s", sid, pid);
				goto done;
			}
			if (delete_comment(sid, pid)) {
				render_error("delete_comment");
				msg("error delete: delete_comment sid %s "
				    "pid %s", sid, pid);
				goto done;
			}
			render_redirect("%s", url(0, "article", sid, NULL,
			    NULL));
			msg("deleted comment sid %s pid %s", sid, pid);
			goto done;
		}
		snprintf(fn, sizeof(fn), "%s/submissions/%s", datadir, sid);
		if (!access(fn, R_OK)) {
			snprintf(fn, sizeof(fn),
			    "%s/submissions/%s/article.more", datadir, sid);
			unlink(fn);
			snprintf(fn, sizeof(fn), "%s/submissions/%s/article",
			    datadir, sid);
			if (unlink(fn)) {
				render_error("unlink: %s: %s", fn,
				    strerror(errno));
				msg("error delete: unlink: %s: %s", fn,
				    strerror(errno));
				goto done;
			}
			snprintf(fn, sizeof(fn), "%s/submissions/%s",
			    datadir, sid);
			if (rmdir(fn)) {
				render_error("rmdir: %s: %s", fn,
				    strerror(errno));
				msg("error delete: rmdir: %s: %s", fn,
				    strerror(errno));
				goto done;
			}
			render_redirect("%s", url(0, "submissions",
			    NULL, NULL, NULL));
			msg("deleted submitted article file %s", fn);
		} else {
			if (!user_is_admin(&user)) {
				render_error("Published articles can only "
				    "be deleted by admins");
				msg("warning delete: attempted deletion "
				    "published article sid %s", sid);
				goto done;
			}
			snprintf(fn, sizeof(fn), "%s/%s", datadir, sid);
			if (access(fn, R_OK)) {
				render_error("invalid sid");
				msg("warning delete: invalid sid %s", sid);
				goto done;
			}
			snprintf(fn, sizeof(fn), "%s/%s/article.more",
			    datadir, sid);
			unlink(fn);
			snprintf(fn, sizeof(fn), "%s/%s/article",
			    datadir, sid);
			if (unlink(fn)) {
				render_error("unlink: %s: %s", fn,
				    strerror(errno));
				msg("error delete: unlink: %s: %s", fn,
				    strerror(errno));
				goto done;
			}
			snprintf(fn, sizeof(fn), "%s/%s", datadir, sid);
			if (rmdir(fn)) {
				render_error("rmdir: %s: %s", fn,
				    strerror(errno));
				msg("error delete: rmdir: %s: %s", fn,
				    strerror(errno));
				goto done;
			}
			render_redirect("%s", url(0, "front",
			    NULL, NULL, NULL));
			msg("deleted published article file %s", fn);
		}
	} else if (!strcmp(action, "edit") && user_is_editor(&user)) {
		struct entry e;
		const char *sid = get_query_param(q, "sid");
		char fn[1024];

		memset(&e, 0, sizeof(e));
		if ((sid != NULL && strlen(sid) > 14)) {
			render_error("missing/invalid sid");
			msg("error edit: invalid sid %s",
			    sid == NULL ? "(null)" : sid);
			goto done;
		}
		if (sid != NULL)
			strlcpy(e.sid, sid, sizeof(e.sid));

		printf("%s\r\n\r\n", ct_html);
		fflush(stdout);
		e.flags |= FLAG_EDIT;
		snprintf(e.fn, sizeof(e.fn), "%s/submissions/%s/article",
		    datadir, sid);
		if (!read_file(&e))
			e.flags |= FLAG_SUBMISSION;
		else {
			snprintf(e.fn, sizeof(e.fn), "%s/%s/article",
			    datadir, sid);
			if (read_file(&e)) {
				render_error("sid not found");
				msg("error edit: read_file: %s", fn);
				goto done;
			}
		}
		snprintf(fn, sizeof(fn), "%s/edit.html", htmldir);
		render_html(fn, &render_submit, &e);
	} else if (!strcmp(action, "editpost") && user_is_editor(&user)) {
		struct entry e;
		int preview;
		FILE *f;
		time_t t = time(NULL);
		const char *host;

		if (read_post(&e, &preview)) {
			render_error("%s", invalidposterr);
			msg("error editpost: read_post: %s", e.sid);
			goto done;
		}
		if (preview) {
			char fn[8192];

			printf("%s\r\n\r\n", ct_html);
			fflush(stdout);
			snprintf(fn, sizeof(fn), "%s/edit.html", htmldir);
			e.flags |= FLAG_EDIT;
			render_html(fn, &render_submit, &e);
			goto done;
		}
		snprintf(e.fn, sizeof(e.fn), "%s/submissions/%s/article",
		    datadir, e.sid);
		if (access(e.fn, R_OK)) {
			struct entry o;

			snprintf(e.fn, sizeof(e.fn), "%s/%s/article",
			    datadir, e.sid);
			memset(&o, 0, sizeof(o));
			strlcpy(o.fn, e.fn, sizeof(o.fn));
			if (access(e.fn, R_OK) || read_file(&o)) {
				render_error("invalid sid");
				msg("error editpost: read_file: %s", e.fn);
				goto done;
			}
			strlcpy(e.name, o.name, sizeof(e.name));
		} else
			strlcpy(e.name, user.id, sizeof(e.name));
		if ((f = fopen(e.fn, "w")) == NULL) {
			render_error("fopen: %s: %s", e.fn, strerror(errno));
			msg("error editpost: fopen: %s: %s", e.fn,
			    strerror(errno));
			goto done;
		}
		fprintf(f, "name: %s\n", e.name);
		fprintf(f, "email: %s\n", e.email);
		fprintf(f, "href: %s\n", e.href);
		fprintf(f, "date: %s", ctime(&t));
		if (e.pubdate > 0)
			fprintf(f, "pubdate: %llu\n", e.pubdate);
		if (user_is_registered(&user))
			fprintf(f, "host: %s\n", user.id);
		else if ((host = getenv("REMOTE_ADDR")) != NULL)
			fprintf(f, "host: %s\n", host);
		fprintf(f, "subject: %s\n", e.subject);
		fprintf(f, "topic: %s\n", e.topic);
		fprintf(f, "dept: %s\n", e.dept);
		if (e.flags & FLAG_POLL)
			fprintf(f, "poll:\n");
		fprintf(f, "\n%s\n", e.body);
		fflush(f);
		fclose(f);
		strlcat(e.fn, ".more", sizeof(e.fn));
		if (!access(e.fn, R_OK) && unlink(e.fn)) {
			render_error("unlink: %s: %s", e.fn, strerror(errno));
			msg("error editpost: unlink: %s: %s", e.fn,
			    strerror(errno));
			goto done;
		}
		if (e.more[0]) {
			if ((f = fopen(e.fn, "w")) == NULL) {
				render_error("fopen: %s: %s", e.fn,
				    strerror(errno));
				msg("error editpost: fopen: %s: %s", e.fn,
				    strerror(errno));
				goto done;
			}
			fprintf(f, "%s", e.more);
			fflush(f);
			fclose(f);
		}
		if (e.flags & FLAG_SUBMISSION) {
			render_redirect("%s", url(0, "submissions", NULL, NULL,
			    NULL));
			msg("edit saved submission sid %s", e.sid);
		} else {
			render_redirect("%s", url(0, "front", NULL, NULL,
			    NULL));
			msg("warning edit: saved published sid %s", e.sid);
		}
	} else if (!strcmp(action, "register")) {
		char fn[1024];

		if (!user_is_registered(&user) &&
		    generate_captcha(captcha_text, sizeof(captcha_text),
		    captcha_cksum, sizeof(captcha_cksum))) {
			render_error("generate_captcha() failed");
			goto done;
		}
		printf("%s\r\n\r\n", ct_html);
		fflush(stdout);
		snprintf(fn, sizeof(fn), "%s/register.html", htmldir);
		render_html(fn, &render_register, NULL);
	} else if (!strcmp(action, "registerpost")) {
		char buf[65536];
		struct query pq;
		const char *login, *pw, *pwconfirm, *name, *email, *href;
		const char *captcha, *cksum;
		struct user u;

		if (read_post_data(buf, sizeof(buf)))
			return (1);
		memset(&pq, 0, sizeof(pq));
		tokenize_query_params(buf, &pq.params);
		login = get_query_param(&pq, "login");
		pw = get_query_param(&pq, "pw");
		pwconfirm = get_query_param(&pq, "pwconfirm");
		name = get_query_param(&pq, "name");
		email = get_query_param(&pq, "email");
		href = get_query_param(&pq, "href");
		if (!login || strlen(login) < 2 || strlen(login) > 14 ||
		    strchr(login, ' ') || strchr(login, ';') ||
		    strchr(login, '<') || strchr(login, '>') ||
		    strchr(login, '&') || strchr(login, '\n') ||
		    strchr(login, '\r') || strchr(login, '\t') || !pw ||
		    strlen(pw) < 1 ||
		    !pwconfirm || strcmp(pw, pwconfirm) ||
		    !name || strchr(name, ';') || !email || strlen(email) < 5 ||
		    !strchr(email, '@') || strchr(email, ';') || !href ||
		    strchr(href, ';')) {
			render_error("registerpost: invalid post data<p>");
			goto done;
		}
		if ((captcha = get_query_param(&pq, "captcha")))
			strlcpy(captcha_text, captcha, sizeof(captcha_text));
		if ((cksum = get_query_param(&pq, "cksum")))
			strlcpy(captcha_cksum, cksum, sizeof(captcha_cksum));
		if (nopost_check()) {
			msg("warning registerpost: nopost_check, login '%s', "
			    "name '%s'", login, name);
			printf("Status: 403\r\n\r\n");
			fflush(stdout);
			goto done;
		}
		if (!user_is_registered(&user) &&
		    verify_captcha(captcha_text, captcha_cksum)) {
			render_error("captcha failed<p>");
			msg("warning registerpost: captcha '%s', login '%s', "
			    "name '%s', user agent '%s', referer '%s'",
			    captcha_text, login, name,
			    q->user_agent ? q->user_agent : "",
			    q->referer ? q->referer : "");
			goto done;
		}
		memset(&u, 0, sizeof(u));
		strlcpy(u.id, login, sizeof(u.id));
		strlcpy(u.group, "none", sizeof(u.group));
		user_hash(pw, u.hash);
		strlcpy(u.name, name, sizeof(u.name));
		strlcpy(u.email, email, sizeof(u.email));
		strlcpy(u.href, href, sizeof(u.href));
		if (user_add(passwd, &u)) {
			msg("error registerpost: id '%s' already used", u.id);
			render_error("user_add() failed<p>probably the login "
			    "'%s' is already used by someone else.<p>try a "
			    "different login.<p>", u.id);
			goto done;
		}
		msg("info registerpost: id '%s' added", u.id);
		user_hash(u.id, u.hash);
		snprintf(buf, sizeof(buf),
		    "To: %s\n"
		    "From: www@register.undeadly.org\n"
		    "Subject: Activate your undeadly.org registration\n"
		    "\n"
		    "Someone, presumably you, has used your email address to "
		    "register an\n"
		    "account with the OpenBSD Journal (http://undeadly.org/).\n"
		    "\n"
		    "This email is sent to confirm that the email address is "
		    "valid and was\n"
		    "used by the real owner. If the account was not registered "
		    "by you,\n"
		    "simply ignore this message, and the registration will be "
		    "cancelled\n"
		    "and you will not receive any further messages.\n"
		    "\n"
		    "To activate your undeadly.org account, please visit the "
		    "following\n"
		    "URL:\n"
		    "\n"
		    "http://undeadly.org%s\n"
		    "\n"
		    "The registration request was made by:\n"
		    "\n"
		    "Host: %s\n"
		    "User-Agent: %s\n"
		    "Referer: %s\n"
		    "\n",
		    email,
		    url(0, "confirm", NULL, NULL, "&i=%s&h=%s", u.id, u.hash),
		    (q && q->remote_addr) ? q->remote_addr : "N/A",
		    (q && q->user_agent) ? q->user_agent : "N/A",
		    (q && q->referer) ? q->referer : "N/A");
		if (mail_send(buf)) {
			render_error("mail_send() failed<p>");
			goto done;
		}
		msg("info registerpost: mail sent to %s", email);
		render_redirect("%s", url(0, "front", NULL, NULL, NULL));
	} else if (!strcmp(action, "confirm")) {
		const char *id, *hash;
		struct user u;

		id = get_query_param(q, "i");
		hash = get_query_param(q, "h");
		if (!id || !id[0] || !hash || !hash[0]) {
			render_error("invalid parameters");
			goto done;
		}
		memset(&u, 0, sizeof(u));
		strlcpy(u.id, id, sizeof(u.id));
		user_hash(u.id, u.hash);
		if (strcmp(hash, u.hash)) {
			render_error("hash mismatch");
			goto done;
		}
		if (user_find(passwd, &u)) {
			render_error("user_find() failed");
			goto done;
		}
		if (strcmp(u.group, "none")) {
			render_error("account is already confirmed and "
			    "activated! try login.");
			goto done;
		}
		strlcpy(u.group, "user", sizeof(u.group));
		if (user_update(passwd, &u)) {
			render_error("user_update() failed");
			goto done;
		}
		msg("info confirm: id '%s' confirmed", u.id);
		render_redirect("%s", authurl);
	} else if (!strcmp(action, "submit")) {
		struct entry e;
		const char *sid = get_query_param(q, "sid");
		const char *pid = get_query_param(q, "pid");
		char fn[1024];

		memset(&e, 0, sizeof(e));
		e.flags |= FLAG_PLAIN;
		if ((sid != NULL && strlen(sid) > 14) ||
		    (pid != NULL && strlen(pid) > 14)) {
			render_error("missing/invalid sid or pid");
			goto done;
		}
		if (sid != NULL)
			strlcpy(e.sid, sid, sizeof(e.sid));
		if (pid != NULL)
			strlcpy(e.pid, pid, sizeof(e.pid));

		if (!user_is_registered(&user) &&
		    generate_captcha(captcha_text, sizeof(captcha_text),
		    captcha_cksum, sizeof(captcha_cksum))) {
			render_error("generate_captcha() failed");
			goto done;
		}

		printf("%s\r\n\r\n", ct_html);
		fflush(stdout);
		snprintf(fn, sizeof(fn), "%s/submit.html", htmldir);
		render_html(fn, &render_submit, &e);
	} else if (!strcmp(action, "submitpost")) {
		struct entry e;
		int preview;
		int fd;
		FILE *f;
		time_t t = time(NULL);
		const char *host;

		if (read_post(&e, &preview)) {
			render_error("%s", invalidposterr);
			if (e.sid[0] || e.subject[0])
				msg("error submitpost: read_post: sid '%s', "
				    "subject '%s'", e.sid, e.subject);
			goto done;
		}
		if (preview) {
			char fn[8192];

			if (!user_is_registered(&user) &&
			    generate_captcha(captcha_text, sizeof(captcha_text),
			    captcha_cksum, sizeof(captcha_cksum))) {
				render_error("generate_captcha() failed");
				goto done;
			}

			printf("%s\r\n\r\n", ct_html);
			fflush(stdout);
			snprintf(fn, sizeof(fn), "%s/preview.html", htmldir);
			render_html(fn, &render_preview, &e);
			goto done;
		}
		if (nopost_check()) {
			msg("warning submitpost: nopost_check, subject '%s'",
			    e.subject);
			printf("Status: 403\r\n\r\n");
			fflush(stdout);
			goto done;
		}
		if (!user_is_registered(&user) &&
		    verify_captcha(captcha_text, captcha_cksum)) {
			render_error("CAPTCHA not entered correctly<p>"
			    "Did you fill in the bottom field of the form and "
			    "copy the characters correctly?<p>"
			    "Press back and try again.<p>"
			    "If the characters seem ambiguous, you can "
			    "use <i>Preview</i> to get a new set of random "
			    "characters.",
			    captcha_text, captcha_cksum);
			msg("warning submitpost: captcha '%s', sid '%s', "
			    "subject '%s', user agent '%s', referer '%s'",
			    captcha_text, e.sid, e.subject,
			    q->user_agent ? q->user_agent : "",
			    q->referer ? q->referer : "");
			goto done;
		}
		if (e.flags & FLAG_PLAIN) {
			char *p;

			p = e.body;
			if ((e.body = malloc(strlen(p) * 4 + 1)) == NULL)  {
				render_error("memory allocation failed");
				msg("error submitpost: malloc");
				goto done;
			}
			html_esc(p, e.body, strlen(p) * 4 + 1, 2);
			free(p);
			if ((p = e.more) != NULL) {
				if ((e.more = malloc(strlen(p) * 4 + 1)) ==
				    NULL)  {
					render_error("memory allocation "
					    "failed");
					msg("error submitpost: malloc");
					goto done;
				}
				html_esc(p, e.more, strlen(p) * 4 + 1, 2);
				free(p);
			}
		}
		fd = e.sid[0] ? create_comment(&e) : create_article(&e);
		if (fd == -1) {
			render_error("create_comment: %s: %s", e.fn,
			    strerror(errno));
			msg("error submitpost: create: %s", e.fn);
			goto done;
		}
		if ((f = fdopen(fd, "w")) == NULL) {
			render_error("fdopen: %s: %s", e.fn, strerror(errno));
			msg("error submitpost: fdopen: %s: %s", e.fn,
			    strerror(errno));
			close(fd);
			goto done;
		}

		fprintf(f, "name: %s\n", e.name);
		fprintf(f, "email: %s\n", e.email);
		fprintf(f, "href: %s\n", e.href);
		fprintf(f, "date: %s", ctime(&t));
		if (user_is_registered(&user))
			fprintf(f, "host: %s\n", user.id);
		else if ((host = getenv("REMOTE_ADDR")) != NULL)
			fprintf(f, "host: %s\n", host);
		fprintf(f, "subject: %s\n", e.subject);
		if (!e.sid[0])
			fprintf(f, "topic: topicopenbsd\n");
		fprintf(f, "\n%s\n", e.body);
		fflush(f);
		fclose(f);
		close(fd);
		if (e.sid[0])
			render_redirect("%s", url(0, "article", e.sid, NULL,
			    NULL));
		else
			render_redirect("%s", url(0, "front", NULL, NULL,
			    NULL));
		if (e.sid[0])
			msg("submitted comment sid %s pid %s subject '%s'",
			    e.sid, e.pid[0] ? e.pid : "0", e.subject);
		else
			msg("submitted article subject '%s'", e.subject);
	} else if (!strcmp(action, "prefs") && user_is_registered(&user)) {
		struct entry e;
		char fn[8192];

		memset(&e, 0, sizeof(e));
		e.flags |= FLAG_PREFS;
		printf("%s\r\n\r\n", ct_html);
		fflush(stdout);
		snprintf(fn, sizeof(fn), "%s/submit.html", htmldir);
		render_html(fn, &render_submit, &e);
	} else if (!strcmp(action, "prefspost") && user_is_registered(&user)) {
		char buf[65536];
		struct query pq;
		const char *name, *email, *href;

		if (read_post_data(buf, sizeof(buf)))
			goto done;
		memset(&pq, 0, sizeof(pq));
		tokenize_query_params(buf, &pq.params);
		if ((name = get_query_param(&pq, "name")) == NULL)
			name = "";
		if ((email = get_query_param(&pq, "email")) == NULL)
			email = "";
		if ((href = get_query_param(&pq, "comment")) == NULL)
			href = "";

		if (!name[0]) {
			render_error("posted form data invalid (name may not "
			    "be empty)");
			msg("error prefspost empty name");
			goto done;
		}
		if (strchr(name, '\r') || strchr(name, '\n') ||
		    strchr(name, ';') || strchr(email, '\r') ||
		    strchr(email, '\n') || strchr(email, ';') ||
		    strchr(href, '\r') || strchr(href, '\n') ||
		    strchr(href, ';')) {
			render_error("posted form data invalid "
			    "(fields must not contain CR, NL, or semi-colon)");
			msg("error prefspost invalid char in field");
			goto done;
		}
		strlcpy(user.name, name, sizeof(user.name));
		strlcpy(user.email, email, sizeof(user.email));
		strlcpy(user.href, href, sizeof(user.href));
		if (user_update(passwd, &user)) {
			msg("error prefspost: user_update() failed for id '%s'",
			    user.id);
			render_error("couldn't update preferences");
			goto done;
		}
		render_redirect("%s", url(0, "front", NULL, NULL, NULL));
	} else if (!strcmp(action, "search")) {
		char fn[1024];

		printf("%s\r\n\r\n", ct_html);
		fflush(stdout);
		snprintf(fn, sizeof(fn), "%s/searchresults.html", htmldir);
		render_html(fn, &render_searchresults, NULL);
	} else if (!strcmp(action, "deauth") && user_is_registered(&user)) {
		const char *domain = getenv("HTTP_HOST");

		printf("Set-Cookie: auth=; path=/; domain=%s\r\n", domain);
		printf("Connection: close\r\n");
		render_redirect("%s", url(0, "front", NULL, NULL, NULL));
		msg("deauthorized");
	} else if (!strcmp(action, "about")) {
		char fn[1024];

		printf("%s\r\n\r\n", ct_html);
		fflush(stdout);
		snprintf(fn, sizeof(fn), "%s/about.html", htmldir);
		render_html(fn, &render_front, NULL);
		msg("about viewed");
	} else if (!strcmp(action, "helpthres")) {
		char fn[1024];

		printf("%s\r\n\r\n", ct_html);
		fflush(stdout);
		snprintf(fn, sizeof(fn), "%s/helpthres.html", htmldir);
		render_html(fn, &render_front, NULL);
		msg("helpthres viewed");
	} else {
		render_error("invalid action");
		msg("error action invalid '%s'", action);
	}

done:
	if (gz != NULL) {
		if (gzclose(gz) != Z_OK)
			msg("error main: gzclose");
		gz = NULL;
	} else
		fflush(stdout);
	if (strcmp(action, "rss") && strcmp(action, "article") &&
	    strcmp(action, "front") && strcmp(action, "about") &&
	    strcmp(action, "errata"))
		msg("total %.1f ms query %s", timelapse(&tx),
		    q == NULL || !q->query_string[0] ? "" : q->query_string);
	if (q != NULL)
		free_query(q);
	return (0);
}

void
msg(const char *fmt, ...)
{
	FILE *f;
	va_list ap;
	time_t t = time(NULL);
	struct tm *tm = gmtime(&t);

	if (!logfile[0] || (f = fopen(logfile, "a")) == NULL)
		return;
	fprintf(f, "%4.4d.%2.2d.%2.2d %2.2d:%2.2d:%2.2d %s %s\tcgi[%u] ",
	    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
	    tm->tm_hour, tm->tm_min, tm->tm_sec,
	    q == NULL || !q->remote_addr[0] ? "-" : q->remote_addr,
	    !user_is_registered(&user) ? "-" : user.id,
	    (unsigned)getpid()); 
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fprintf(f, "\n");
	fflush(f);
	fclose(f);
}
