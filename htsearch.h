/*	$Id: htsearch.h,v 1.6 2005/03/07 11:09:59 dhartmei Exp $ */

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

#ifndef _HTSEARCH_H_
#define _HTSEARCH_H_

struct entry {
	char			 sid[15];
	char			 pid[15];
	char			 fn[MAXNAMLEN + 1];
	unsigned		 ts;
	unsigned		 level;
	unsigned		 flags;
	unsigned long long	 pubdate;
	char			 name[64];
	char			 host[64];
	char			 email[64];
	char			 href[128];
	char			 subject[128];
	char			 date[32];
	char			 dept[64];
	char			 topic[32];
	int			 modcount;
	int			 modsum;
	int			 modable;
	int			 score;
	char			 excerpt[512];
	char			*body;
	char			*more;
};

int	 htsearch(const char *dir, const char *conf, const char *words,
	    unsigned page, const char *sort, const char *method,
	    struct entry *entries, unsigned size);

#endif
