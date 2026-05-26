/*
 * Copyright (C) Thomas Bernard
 * Copyright (C) HexChat contributors
 * Copyright (C) ZoiteChat contributors
 */

#ifndef ZOITECHAT_UPNP_H
#define ZOITECHAT_UPNP_H

void upnp_init(void);
void upnp_add_redir(const char *addr, int port);
void upnp_rem_redir(int port);

#endif
