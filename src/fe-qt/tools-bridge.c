/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/ignore.h"
#include "../common/server.h"
#include "../common/url.h"
#include "tools-bridge.h"
#include <string.h>

static int valid_server(server *serv) { return serv && g_slist_find(serv_list, serv) != NULL; }
void qt_core_urls_clear(void) { if (url_tree) url_clear(); }
static int single_word(const char *text)
{
  return text && *text && !strpbrk(text, " \t\r\n");
}
void *qt_core_session_server(void *ptr)
{
  session *sess = ptr;
  return sess && is_session(sess) ? sess->server : NULL;
}
int qt_core_server_info(void *ptr, const char **name, int *connected)
{
  server *serv = ptr;
  if (!valid_server(serv)) return FALSE;
  *name = serv->servername[0] ? serv->servername : serv->hostname;
  *connected = serv->connected;
  return TRUE;
}
int qt_core_request_channels(void *ptr, const char *filter)
{
  server *serv = ptr;
  if (!valid_server(serv) || !serv->connected || !filter || strpbrk(filter, "\r\n")) return FALSE;
  serv->p_list_channels(serv, (char *)filter, 1);
  return TRUE;
}
int qt_core_join_channel(void *ptr, const char *channel)
{
  server *serv = ptr;
  if (!valid_server(serv) || !serv->connected || !single_word(channel)) return FALSE;
  serv->p_join(serv, (char *)channel, "");
  return TRUE;
}
void qt_core_ignore_foreach(void (*visitor)(const char *, int, void *), void *data)
{
  GSList *entry;
  if (!visitor) return;
  for (entry = ignore_list; entry; entry = entry->next) {
    struct ignore *rule = entry->data;
    visitor(rule->mask, rule->type, data);
  }
}
int qt_core_ignore_set(const char *old_mask, const char *mask, int flags)
{
  struct ignore *old, *existing;
  if (!single_word(mask) || strlen(mask) > 512 || !(flags & (IG_PRIV | IG_NOTI | IG_CHAN | IG_CTCP | IG_INVI | IG_DCC)) || (flags & ~255)) return FALSE;
  old = old_mask && *old_mask ? ignore_exists((char *)old_mask) : NULL;
  if (old_mask && *old_mask && !old) return FALSE;
  existing = ignore_exists((char *)mask);
  if (existing && existing != old) return FALSE;
  if (old) ignore_del(NULL, old);
  ignore_add((char *)mask, flags, TRUE);
  ignore_save();
  return TRUE;
}
int qt_core_ignore_remove(const char *mask)
{
  if (!mask || !ignore_del((char *)mask, NULL)) return FALSE;
  ignore_save();
  return TRUE;
}
