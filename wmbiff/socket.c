/* $Id: socket.c,v 1.5 2002/06/01 05:42:38 bluehal Exp $ */
/* Copyright (C) 1998 Trent Piepho  <xyzzy@u.washington.edu>
 *           (C) 1999 Trent Piepho  <xyzzy@speakeasy.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.  */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>

#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

/* nspring/blueHal, 10 Apr 2002; added some extra error
   printing, in line with the debug-messages-to-stdout
   philosophy of the rest of the wmbiff code */
/* 29 May 2002; incorporated IPv6 support by 
   Jun-ichiro itojun Hagino <itojun@iijlab.net>, thanks! */


int sock_connect(const char *hostname, int port)
{
#ifdef HAVE_GETADDRINFO
	struct addrinfo hints, *res, *res0;
	int fd;
	char pbuf[NI_MAXSERV];
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	snprintf(pbuf, sizeof(pbuf), "%d", port);
	error = getaddrinfo(hostname, pbuf, &hints, &res0);
	if (error) {
		printf("%s: %s\n", hostname, gai_strerror(error));
		return -1;
	}

	fd = -1;
	for (res = res0; res; res = res->ai_next) {
		fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (fd < 0)
			continue;
		if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
			close(fd);
			fd = -1;
			continue;
		}
		break;
	}
	freeaddrinfo(res0);
	if (fd < 0) {
		perror("Error connecting");
		printf("socket/connect to %s failed: %s\n", hostname,
			   strerror(errno));
		return -1;
	}
	return fd;
#else
	struct hostent *host;
	struct sockaddr_in addr;
	int fd, i;

	host = gethostbyname(hostname);
	if (host == NULL) {
		herror("gethostbyname");
		printf("gethostbyname(%s) failed.\n", hostname);
		return (-1);
	};

	fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd == -1) {
		perror("Error opening socket");
		printf("socket() failed.\n");
		return (-1);
	};

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = *(u_long *) host->h_addr_list[0];
	addr.sin_port = htons(port);
	i = connect(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr));
	if (i == -1) {
		perror("Error connecting");
		printf("connect(%s:%d) failed: %s\n", inet_ntoa(addr.sin_addr),
			   port, strerror(errno));
		close(fd);
		return (-1);
	};
	return (fd);
#endif
}

/* vim:set ts=4: */
