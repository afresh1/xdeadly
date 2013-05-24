# $Id: Makefile,v 1.12 2007/01/11 15:40:32 dhartmei Exp $

CC=gcc -Wall -Wstrict-prototypes -g -O0 -static
CHROOT=/var/www/htdocs
DIR=/undeadly.org
AUTH_DIR=/auth

all: auth undeadly vuxml errata

undeadly: undeadly.o cgi.o htsearch.o thres.o conf.o captcha.o user.o mail.o
	$(CC) -o undeadly undeadly.o cgi.o htsearch.o thres.o conf.o captcha.o user.o mail.o -lz

auth: auth.o cgi.o user.o
	$(CC) -o auth auth.o cgi.o user.o

undeadly.o: undeadly.c cgi.h htsearch.h conf.h user.h mail.h
	$(CC) -c undeadly.c

auth.o: auth.c cgi.h
	$(CC) -c auth.c

conf.o: conf.c conf.h
	$(CC) -c conf.c

mail.o: mail.c mail.h
	$(CC) -c mail.c

user.o: user.c user.h
	$(CC) -c user.c

captcha.o: captcha.c captcha.h
	$(CC) -c captcha.c

cgi.o: cgi.c cgi.h
	$(CC) -c cgi.c

htsearch.o: htsearch.c htsearch.h
	$(CC) -c htsearch.c

thres.o: thres.c thres.h
	$(CC) -c thres.c

vuxml: vuxml.c
	$(CC) -o vuxml vuxml.c

errata: errata.c
	$(CC) -o errata errata.c

clean:
	rm -f undeadly auth vuxml errata *.o *.core

install:
	rm -f $(CHROOT)$(DIR)/cgi
	cp undeadly $(CHROOT)$(DIR)/cgi

install-auth:
	cp auth $(CHROOT)$(AUTH_DIR)/cgi

test:
	QUERY_STRING='action=article&sid=20031229085301' sudo \
		chroot -u www -g www $(CHROOT) $(DIR)/cgi

test-rss:
	QUERY_STRING='action=rss' sudo \
		chroot -u www -g www $(CHROOT) $(DIR)/cgi

test-search:
	sudo chroot -u www -g www $(CHROOT) $(DIR)/htdig/bin/htsearch \
		-c $(DIR)/htdig/conf/htsearch_comments.conf \
		'words=dhartmei'

