/*
 * Copyright (C) Thomas Bernard
 * Copyright (C) HexChat contributors
 * Copyright (C) ZoiteChat contributors
 */

#include <stdio.h>
#include <string.h>

#include "upnp.h"

#ifdef USE_MINIUPNPC
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>

static struct UPNPUrls urls;
static struct IGDdatas data;
static int ready;
static char upnp_lanaddr[64];

void
upnp_init(void)
{
	int err = 0;
	int igd = 0;
	char lanaddr[64] = {0};
	struct UPNPDev *devlist;

	memset(&urls, 0, sizeof(urls));
	memset(&data, 0, sizeof(data));
	ready = 0;
	upnp_lanaddr[0] = 0;

	devlist = upnpDiscover(2000, NULL, NULL, 0, 0, 2, &err);
	if (!devlist)
		devlist = upnpDiscover(2000, NULL, NULL, 0, 1, 2, &err);
	if (!devlist)
		return;

	igd = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
	if (igd == 1 || igd == 2 || igd == 3)
	{
		ready = 1;
		snprintf(upnp_lanaddr, sizeof(upnp_lanaddr), "%s", lanaddr);
	}

	freeUPNPDevlist(devlist);
}

void
upnp_add_redir(const char *addr, int port)
{
	char port_str[16];
	const char *map_addr;
	int r;

	if (!ready)
		upnp_init();
	if (!ready)
		return;

	map_addr = upnp_lanaddr[0] ? upnp_lanaddr : addr;
	if (!map_addr || !map_addr[0])
		return;

	snprintf(port_str, sizeof(port_str), "%d", port);
	r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
		port_str, port_str, NULL, "zoitechat", "TCP", NULL, NULL);
	if (r != UPNPCOMMAND_SUCCESS)
		r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
			port_str, port_str, map_addr, "zoitechat", "TCP", NULL, NULL);
	if (r != UPNPCOMMAND_SUCCESS)
		return;
}

void
upnp_rem_redir(int port)
{
	char port_str[16];

	if (!ready)
		upnp_init();
	if (!ready)
		return;

	snprintf(port_str, sizeof(port_str), "%d", port);
	UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, port_str, "TCP", NULL);
}

#else

void
upnp_init(void)
{
}

void
upnp_add_redir(const char *addr, int port)
{
	(void)addr;
	(void)port;
}

void
upnp_rem_redir(int port)
{
	(void)port;
}

#endif
