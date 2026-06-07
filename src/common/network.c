/* X-Chat
 * Copyright (C) 2001 Peter Zelezny.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/* ipv4 and ipv6 networking functions with a common interface */
#define _POSIX_C_SOURCE 200112L
#ifdef _WIN32
#  include <ws2tcpip.h>
#else
#  include <netdb.h>
#endif

#include "config.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#ifndef WIN32
#include <unistd.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#endif

#define WANTSOCKET
#define WANTARPA
#define WANTDNS
#include "inet.h"

#define NETWORK_PRIVATE
#include "network.h"
#include "zoitechat.h"

extern struct zoitechatprefs prefs;

#define RAND_INT(n) ((int)(rand() / (RAND_MAX + 1.0) * (n)))


/* ================== COMMON ================= */

static void
net_set_socket_options (int sok)
{
	socklen_t sw;

	sw = 1;
	setsockopt (sok, SOL_SOCKET, SO_REUSEADDR, (char *) &sw, sizeof (sw));
	sw = 1;
	setsockopt (sok, SOL_SOCKET, SO_KEEPALIVE, (char *) &sw, sizeof (sw));
#ifdef TCP_KEEPIDLE
	{
		int keepidle = prefs.hex_net_keepalive_idle;
		if (keepidle > 0)
			setsockopt (sok, IPPROTO_TCP, TCP_KEEPIDLE, (char *) &keepidle, sizeof (keepidle));
	}
#endif
#ifdef TCP_KEEPINTVL
	{
		int keepintvl = prefs.hex_net_keepalive_interval;
		if (keepintvl > 0)
			setsockopt (sok, IPPROTO_TCP, TCP_KEEPINTVL, (char *) &keepintvl, sizeof (keepintvl));
	}
#endif
#ifdef TCP_KEEPCNT
	{
		int keepcnt = prefs.hex_net_keepalive_count;
		if (keepcnt > 0)
			setsockopt (sok, IPPROTO_TCP, TCP_KEEPCNT, (char *) &keepcnt, sizeof (keepcnt));
	}
#endif
}

char *
net_ip (uint32_t addr)
{
	struct in_addr ia;

	ia.s_addr = htonl (addr);
	return inet_ntoa (ia);
}

int
net_lookup_ipv4 (const char *hostname, uint32_t *addr)
{
	struct addrinfo hints;
	struct addrinfo *res;
	struct sockaddr_in *sin;
	int ret;

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG;

	ret = getaddrinfo (hostname, NULL, &hints, &res);
	if (ret != 0)
		return FALSE;

	sin = (struct sockaddr_in *) res->ai_addr;
	*addr = sin->sin_addr.s_addr;
	freeaddrinfo (res);

	return TRUE;
}

void
net_store_destroy (netstore * ns)
{
	if (ns->ip6_hostent)
		freeaddrinfo (ns->ip6_hostent);
	free (ns);
}

netstore *
net_store_new (void)
{
	return calloc (1, sizeof (netstore));
}

/* =================== IPV6 ================== */

char *
net_resolve (netstore * ns, char *hostname, int port, char **real_host)
{
	struct addrinfo hints;
	char ipstring[MAX_HOSTNAME];
	char portstring[MAX_HOSTNAME];
	int ret;

/*	if (ns->ip6_hostent)
		freeaddrinfo (ns->ip6_hostent);*/

	sprintf (portstring, "%d", port);

	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = PF_UNSPEC; /* support ipv6 and ipv4 */
	hints.ai_flags = AI_CANONNAME | AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_STREAM;

	if (port == 0)
		ret = getaddrinfo (hostname, NULL, &hints, &ns->ip6_hostent);
	else
		ret = getaddrinfo (hostname, portstring, &hints, &ns->ip6_hostent);
	if (ret != 0)
		return NULL;

#ifdef LOOKUPD	/* See note about lookupd above the IPv4 version of net_resolve. */
	struct addrinfo *tmp;
	int count = 0;

	for (tmp = ns->ip6_hostent; tmp; tmp = tmp->ai_next)
		count ++;

	count = RAND_INT(count);
	
	while (count--) ns->ip6_hostent = ns->ip6_hostent->ai_next;
#endif

	/* find the numeric IP number */
	ipstring[0] = 0;
	getnameinfo (ns->ip6_hostent->ai_addr, ns->ip6_hostent->ai_addrlen,
					 ipstring, sizeof (ipstring), NULL, 0, NI_NUMERICHOST);

	if (ns->ip6_hostent->ai_canonname)
		*real_host = g_strdup(ns->ip6_hostent->ai_canonname);
	else
		*real_host = g_strdup(hostname);

	return g_strdup(ipstring);
}

/* the only thing making this interface unclean, this shitty sok4, sok6 business */

int
net_connect (netstore * ns, int sok4, int sok6, int *sok_return)
{
	struct addrinfo *res, *res0;
	int error = -1;

	res0 = ns->ip6_hostent;

	for (res = res0; res; res = res->ai_next)
	{
/*		sok = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sok < 0)
			continue;*/
		switch (res->ai_family)
		{
		case AF_INET:
			error = connect (sok4, res->ai_addr, res->ai_addrlen);
			*sok_return = sok4;
			break;
		case AF_INET6:
			error = connect (sok6, res->ai_addr, res->ai_addrlen);
			*sok_return = sok6;
			break;
		default:
			error = 1;
		}

		if (error == 0)
			break;
	}

	return error;
}

void
net_bind (netstore * tobindto, int sok4, int sok6)
{
	bind (sok4, tobindto->ip6_hostent->ai_addr,
			tobindto->ip6_hostent->ai_addrlen);
	bind (sok6, tobindto->ip6_hostent->ai_addr,
			tobindto->ip6_hostent->ai_addrlen);
}

void
net_sockets (int *sok4, int *sok6)
{
	*sok4 = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
	*sok6 = socket (AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	net_set_socket_options (*sok4);
	net_set_socket_options (*sok6);
}

void
udp_sockets (int *sok4, int *sok6)
{
	*sok4 = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	*sok6 = socket (AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
}
