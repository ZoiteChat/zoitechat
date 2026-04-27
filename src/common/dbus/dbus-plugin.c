/* dbus-plugin.c - zoitechat plugin for remote access using D-Bus
 * Copyright (C) 2006 Claessens Xavier
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
 *
 * Claessens Xavier
 * xclaesse@gmail.com
 */

#include <config.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include "zoitechat-plugin.h"
#include "dbus-plugin.h"

#define PNAME _("remote access")
#define PDESC _("plugin for remote access using DBUS")
#define PVERSION ""

#define DBUS_OBJECT_PATH "/org/zoitechat"
#define DBUS_INTERFACE_CONNECTION "org.zoitechat.connection"
#define DBUS_INTERFACE_PLUGIN "org.zoitechat.plugin"
#define DBUS_SERVICE_DBUS "org.freedesktop.DBus"
#define DBUS_PATH_DBUS "/org/freedesktop/DBus"
#define DBUS_INTERFACE_DBUS "org.freedesktop.DBus"

static const char introspection_xml[] =
"<node>"
"  <interface name='org.zoitechat.connection'>"
"    <method name='Connect'>"
"      <arg type='s' name='filename' direction='in'/>"
"      <arg type='s' name='name' direction='in'/>"
"      <arg type='s' name='desc' direction='in'/>"
"      <arg type='s' name='version' direction='in'/>"
"      <arg type='s' name='path' direction='out'/>"
"    </method>"
"    <method name='Disconnect'/>"
"  </interface>"
"  <interface name='org.zoitechat.plugin'>"
"    <method name='Command'><arg type='s' direction='in'/></method>"
"    <method name='Print'><arg type='s' direction='in'/></method>"
"    <method name='FindContext'><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='u' direction='out'/></method>"
"    <method name='GetContext'><arg type='u' direction='out'/></method>"
"    <method name='SetContext'><arg type='u' direction='in'/><arg type='b' direction='out'/></method>"
"    <method name='GetInfo'><arg type='s' direction='in'/><arg type='s' direction='out'/></method>"
"    <method name='GetPrefs'><arg type='s' direction='in'/><arg type='i' direction='out'/><arg type='s' direction='out'/><arg type='i' direction='out'/></method>"
"    <method name='HookCommand'><arg type='s' direction='in'/><arg type='i' direction='in'/><arg type='s' direction='in'/><arg type='i' direction='in'/><arg type='u' direction='out'/></method>"
"    <method name='HookServer'><arg type='s' direction='in'/><arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='u' direction='out'/></method>"
"    <method name='HookPrint'><arg type='s' direction='in'/><arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='u' direction='out'/></method>"
"    <method name='Unhook'><arg type='u' direction='in'/></method>"
"    <method name='ListGet'><arg type='s' direction='in'/><arg type='u' direction='out'/></method>"
"    <method name='ListNext'><arg type='u' direction='in'/><arg type='b' direction='out'/></method>"
"    <method name='ListStr'><arg type='u' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='out'/></method>"
"    <method name='ListInt'><arg type='u' direction='in'/><arg type='s' direction='in'/><arg type='i' direction='out'/></method>"
"    <method name='ListTime'><arg type='u' direction='in'/><arg type='s' direction='in'/><arg type='t' direction='out'/></method>"
"    <method name='ListFields'><arg type='s' direction='in'/><arg type='as' direction='out'/></method>"
"    <method name='ListFree'><arg type='u' direction='in'/></method>"
"    <method name='EmitPrint'><arg type='s' direction='in'/><arg type='as' direction='in'/><arg type='b' direction='out'/></method>"
"    <method name='Nickcmp'><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='i' direction='out'/></method>"
"    <method name='Strip'><arg type='s' direction='in'/><arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='s' direction='out'/></method>"
"    <method name='SendModes'><arg type='as' direction='in'/><arg type='i' direction='in'/><arg type='y' direction='in'/><arg type='y' direction='in'/></method>"
"    <signal name='CommandSignal'><arg type='as'/><arg type='as'/><arg type='u'/><arg type='u'/></signal>"
"    <signal name='ServerSignal'><arg type='as'/><arg type='as'/><arg type='u'/><arg type='u'/></signal>"
"    <signal name='PrintSignal'><arg type='as'/><arg type='u'/><arg type='u'/></signal>"
"    <signal name='UnloadSignal'/>"
"  </interface>"
"</node>";

static zoitechat_plugin *ph;
static guint last_context_id;
static GList *contexts;
static GHashTable *clients;
static GDBusConnection *connection;
static GDBusNodeInfo *introspection_data;
static guint name_owner_subscription;
static guint object_count;

typedef struct
{
	guint id;
	int return_value;
	zoitechat_hook *hook;
	struct _RemoteObject *obj;
} HookInfo;

typedef struct
{
	guint id;
	zoitechat_context *context;
} ContextInfo;

typedef struct _RemoteObject
{
	guint registration_id;
	guint last_hook_id;
	guint last_list_id;
	zoitechat_context *context;
	char *dbus_path;
	char *filename;
	void *handle;
	GHashTable *hooks;
	GHashTable *lists;
} RemoteObject;

static char **build_list (char *word[]);
static guint context_list_find_id (zoitechat_context *context);
static zoitechat_context *context_list_find_context (guint id);
static gboolean emit_signal (RemoteObject *obj, const char *name, GVariant *params);
static const GDBusInterfaceVTable connection_vtable;

static void
hook_info_destroy (gpointer data)
{
	HookInfo *info = (HookInfo *)data;

	if (!info)
		return;

	zoitechat_unhook (ph, info->hook);
	g_free (info);
}

static void
list_info_destroy (gpointer data)
{
	zoitechat_list_free (ph, (zoitechat_list *)data);
}

static RemoteObject *
remote_object_new (const char *path)
{
	RemoteObject *obj = g_new0 (RemoteObject, 1);

	obj->hooks = g_hash_table_new_full (g_int_hash, g_int_equal, NULL, hook_info_destroy);
	obj->lists = g_hash_table_new_full (g_int_hash, g_int_equal, g_free, list_info_destroy);
	obj->dbus_path = g_strdup (path);
	obj->context = zoitechat_get_context (ph);

	return obj;
}

static void
remote_object_free (RemoteObject *obj)
{
	if (!obj)
		return;

	if (connection && obj->registration_id)
		g_dbus_connection_unregister_object (connection, obj->registration_id);
	g_hash_table_destroy (obj->lists);
	g_hash_table_destroy (obj->hooks);
	g_free (obj->dbus_path);
	g_free (obj->filename);
	if (obj->handle)
		zoitechat_plugingui_remove (ph, obj->handle);
	g_free (obj);
}

static gboolean
remote_object_command (RemoteObject *obj,
			      const char *command)
{
	if (zoitechat_set_context (ph, obj->context))
		zoitechat_command (ph, command);
	return TRUE;
}

static gboolean
remote_object_print (RemoteObject *obj,
		    const char *text)
{
	if (zoitechat_set_context (ph, obj->context))
		zoitechat_print (ph, text);
	return TRUE;
}

static gboolean
remote_object_find_context (RemoteObject *obj,
			   const char *server,
			   const char *channel,
			   guint *ret_id)
{
	zoitechat_context *context;

	if (*server == '\0')
		server = NULL;
	if (*channel == '\0')
		channel = NULL;
	context = zoitechat_find_context (ph, server, channel);
	*ret_id = context_list_find_id (context);

	return TRUE;
}

static gboolean
remote_object_get_context (RemoteObject *obj,
			  guint *ret_id)
{
	*ret_id = context_list_find_id (obj->context);
	return TRUE;
}

static gboolean
remote_object_set_context (RemoteObject *obj,
			  guint id,
			  gboolean *ret)
{
	zoitechat_context *context = context_list_find_context (id);

	if (!context)
	{
		*ret = FALSE;
		return TRUE;
	}

	obj->context = context;
	*ret = TRUE;
	return TRUE;
}

static gboolean
remote_object_get_info (RemoteObject *obj,
		       const char *id,
		       char **ret_info)
{
	if (!zoitechat_set_context (ph, obj->context) || g_str_equal (id, "win_ptr"))
	{
		*ret_info = NULL;
		return TRUE;
	}

	*ret_info = g_strdup (zoitechat_get_info (ph, id));
	return TRUE;
}

static gboolean
remote_object_get_prefs (RemoteObject *obj,
			const char *name,
			int *ret_type,
			char **ret_str,
			int *ret_int)
{
	const char *str;

	if (!zoitechat_set_context (ph, obj->context))
	{
		*ret_type = 0;
		*ret_str = NULL;
		*ret_int = 0;
		return TRUE;
	}

	*ret_type = zoitechat_get_prefs (ph, name, &str, ret_int);
	*ret_str = g_strdup (str);
	return TRUE;
}

static int
server_hook_cb (char *word[],
		char *word_eol[],
		void *userdata)
{
	HookInfo *info = (HookInfo *)userdata;
	char **arg1 = build_list (word + 1);
	char **arg2 = build_list (word_eol + 1);
	guint context_id;
	GVariant *params;

	info->obj->context = zoitechat_get_context (ph);
	context_id = context_list_find_id (info->obj->context);
	params = g_variant_new ("(^as^asuu)", arg1 ? arg1 : (char *[]){ NULL }, arg2 ? arg2 : (char *[]){ NULL }, info->id, context_id);
	emit_signal (info->obj, "ServerSignal", params);
	g_strfreev (arg1);
	g_strfreev (arg2);
	return info->return_value;
}

static int
command_hook_cb (char *word[],
		 char *word_eol[],
		 void *userdata)
{
	HookInfo *info = (HookInfo *)userdata;
	char **arg1 = build_list (word + 1);
	char **arg2 = build_list (word_eol + 1);
	guint context_id;
	GVariant *params;

	info->obj->context = zoitechat_get_context (ph);
	context_id = context_list_find_id (info->obj->context);
	params = g_variant_new ("(^as^asuu)", arg1 ? arg1 : (char *[]){ NULL }, arg2 ? arg2 : (char *[]){ NULL }, info->id, context_id);
	emit_signal (info->obj, "CommandSignal", params);
	g_strfreev (arg1);
	g_strfreev (arg2);
	return info->return_value;
}

static int
print_hook_cb (char *word[],
	       void *userdata)
{
	HookInfo *info = (HookInfo *)userdata;
	char **arg1 = build_list (word + 1);
	guint context_id;
	GVariant *params;

	info->obj->context = zoitechat_get_context (ph);
	context_id = context_list_find_id (info->obj->context);
	params = g_variant_new ("(^asuu)", arg1 ? arg1 : (char *[]){ NULL }, info->id, context_id);
	emit_signal (info->obj, "PrintSignal", params);
	g_strfreev (arg1);
	return info->return_value;
}

static gboolean
remote_object_hook_command (RemoteObject *obj,
			   const char *name,
			   int priority,
			   const char *help_text,
			   int return_value,
			   guint *ret_id)
{
	HookInfo *info = g_new0 (HookInfo, 1);

	info->obj = obj;
	info->return_value = return_value;
	info->id = ++obj->last_hook_id;
	info->hook = zoitechat_hook_command (ph, name, priority, command_hook_cb, help_text, info);
	g_hash_table_insert (obj->hooks, &info->id, info);
	*ret_id = info->id;
	return TRUE;
}

static gboolean
remote_object_hook_server (RemoteObject *obj,
			  const char *name,
			  int priority,
			  int return_value,
			  guint *ret_id)
{
	HookInfo *info = g_new0 (HookInfo, 1);

	info->obj = obj;
	info->return_value = return_value;
	info->id = ++obj->last_hook_id;
	info->hook = zoitechat_hook_server (ph, name, priority, server_hook_cb, info);
	g_hash_table_insert (obj->hooks, &info->id, info);
	*ret_id = info->id;
	return TRUE;
}

static gboolean
remote_object_hook_print (RemoteObject *obj,
			 const char *name,
			 int priority,
			 int return_value,
			 guint *ret_id)
{
	HookInfo *info = g_new0 (HookInfo, 1);

	info->obj = obj;
	info->return_value = return_value;
	info->id = ++obj->last_hook_id;
	info->hook = zoitechat_hook_print (ph, name, priority, print_hook_cb, info);
	g_hash_table_insert (obj->hooks, &info->id, info);
	*ret_id = info->id;
	return TRUE;
}

static gboolean
remote_object_unhook (RemoteObject *obj,
		     guint id)
{
	g_hash_table_remove (obj->hooks, &id);
	return TRUE;
}

static gboolean
remote_object_list_get (RemoteObject *obj,
		       const char *name,
		       guint *ret_id)
{
	zoitechat_list *xlist;
	guint *id;

	if (!zoitechat_set_context (ph, obj->context))
	{
		*ret_id = 0;
		return TRUE;
	}

	xlist = zoitechat_list_get (ph, name);
	if (!xlist)
	{
		*ret_id = 0;
		return TRUE;
	}

	id = g_new (guint, 1);
	*id = ++obj->last_list_id;
	*ret_id = *id;
	g_hash_table_insert (obj->lists, id, xlist);
	return TRUE;
}

static gboolean
remote_object_list_next (RemoteObject *obj,
			guint id,
			gboolean *ret)
{
	zoitechat_list *xlist = g_hash_table_lookup (obj->lists, &id);

	if (!xlist)
	{
		*ret = FALSE;
		return TRUE;
	}

	*ret = zoitechat_list_next (ph, xlist);
	return TRUE;
}

static gboolean
remote_object_list_str (RemoteObject *obj,
		       guint id,
		       const char *name,
		       char **ret_str)
{
	zoitechat_list *xlist = g_hash_table_lookup (obj->lists, &id);

	if (xlist == NULL && !zoitechat_set_context (ph, obj->context))
	{
		*ret_str = NULL;
		return TRUE;
	}

	if (g_str_equal (name, "context"))
	{
		*ret_str = NULL;
		return TRUE;
	}

	*ret_str = g_strdup (zoitechat_list_str (ph, xlist, name));
	return TRUE;
}

static gboolean
remote_object_list_int (RemoteObject *obj,
		       guint id,
		       const char *name,
		       int *ret_int)
{
	zoitechat_list *xlist = g_hash_table_lookup (obj->lists, &id);

	if (xlist == NULL && !zoitechat_set_context (ph, obj->context))
	{
		*ret_int = -1;
		return TRUE;
	}

	if (g_str_equal (name, "context"))
	{
		zoitechat_context *context = (zoitechat_context *)zoitechat_list_str (ph, xlist, name);
		*ret_int = context_list_find_id (context);
	}
	else
		*ret_int = zoitechat_list_int (ph, xlist, name);

	return TRUE;
}

static gboolean
remote_object_list_time (RemoteObject *obj,
			guint id,
			const char *name,
			guint64 *ret_time)
{
	zoitechat_list *xlist = g_hash_table_lookup (obj->lists, &id);

	if (!xlist)
	{
		*ret_time = (guint64)-1;
		return TRUE;
	}

	*ret_time = zoitechat_list_time (ph, xlist, name);
	return TRUE;
}

static gboolean
remote_object_list_fields (const char *name,
			  char ***ret)
{
	*ret = g_strdupv ((char **)zoitechat_list_fields (ph, name));
	if (*ret == NULL)
		*ret = g_new0 (char *, 1);
	return TRUE;
}

static gboolean
remote_object_list_free (RemoteObject *obj,
			guint id)
{
	g_hash_table_remove (obj->lists, &id);
	return TRUE;
}

static gboolean
remote_object_emit_print (RemoteObject *obj,
			 const char *event_name,
			 const char *args[],
			 gboolean *ret)
{
	const char *argv[4] = { NULL, NULL, NULL, NULL };
	int i;

	for (i = 0; i < 4 && args[i] != NULL; i++)
		argv[i] = args[i];

	*ret = zoitechat_set_context (ph, obj->context);
	if (*ret)
		*ret = zoitechat_emit_print (ph, event_name, argv[0], argv[1], argv[2], argv[3]);

	return TRUE;
}

static gboolean
remote_object_nickcmp (RemoteObject *obj,
		      const char *nick1,
		      const char *nick2,
		      int *ret)
{
	zoitechat_set_context (ph, obj->context);
	*ret = zoitechat_nickcmp (ph, nick1, nick2);
	return TRUE;
}

static gboolean
remote_object_strip (const char *str,
		    int len,
		    int flag,
		    char **ret_str)
{
	*ret_str = zoitechat_strip (ph, str, len, flag);
	return TRUE;
}

static gboolean
remote_object_send_modes (RemoteObject *obj,
			 const char *targets[],
			 int modes_per_line,
			 char sign,
			 char mode)
{
	if (zoitechat_set_context (ph, obj->context))
	{
		zoitechat_send_modes (ph, targets,
				     g_strv_length ((char **)targets),
				     modes_per_line,
				     sign,
				     mode);
	}

	return TRUE;
}

static gboolean
emit_signal (RemoteObject *obj, const char *name, GVariant *params)
{
	if (!connection)
		return FALSE;

	return g_dbus_connection_emit_signal (connection,
					      NULL,
					      obj->dbus_path,
					      DBUS_INTERFACE_PLUGIN,
					      name,
					      params,
					      NULL);
}

static gboolean
clients_find_path_foreach (gpointer key,
			   gpointer value,
			   gpointer user_data)
{
	RemoteObject *obj = value;
	return g_str_equal (obj->dbus_path, (const char *)user_data);
}

static RemoteObject *
find_remote_by_path (const char *object_path)
{
	if (g_str_equal (object_path, DBUS_OBJECT_PATH "/Remote"))
		return g_hash_table_lookup (clients, "__root__");

	return g_hash_table_find (clients, clients_find_path_foreach, (gpointer)object_path);
}

static void
method_call_cb (GDBusConnection *conn,
		const gchar *sender,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *method_name,
		GVariant *parameters,
		GDBusMethodInvocation *invocation,
		gpointer user_data)
{
	RemoteObject *obj;

	if (g_str_equal (interface_name, DBUS_INTERFACE_CONNECTION))
	{
		if (g_str_equal (method_name, "Connect"))
		{
			const char *filename;
			const char *name;
			const char *desc;
			const char *version;
			GError *error = NULL;
			char *path;
			gchar count_buffer[15];

			g_variant_get (parameters, "(&s&s&s&s)", &filename, &name, &desc, &version);
			obj = g_hash_table_lookup (clients, sender);
			if (obj)
			{
				g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", obj->dbus_path));
				return;
			}

			g_snprintf (count_buffer, sizeof count_buffer, "%u", object_count++);
			path = g_build_filename (DBUS_OBJECT_PATH, count_buffer, NULL);
			obj = remote_object_new (path);
			obj->filename = g_path_get_basename (filename);
			obj->handle = zoitechat_plugingui_add (ph, obj->filename, name, desc, version, NULL);
			obj->registration_id = g_dbus_connection_register_object (
				conn,
				obj->dbus_path,
				introspection_data->interfaces[1],
				&connection_vtable,
				NULL,
				NULL,
				&error);
			if (!obj->registration_id)
			{
				g_dbus_method_invocation_return_dbus_error (invocation, "org.zoitechat.Error", error->message);
				g_error_free (error);
				remote_object_free (obj);
				g_free (path);
				return;
			}
			g_hash_table_insert (clients, g_strdup (sender), obj);
			g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", obj->dbus_path));
			g_free (path);
			return;
		}
		if (g_str_equal (method_name, "Disconnect"))
		{
			g_hash_table_remove (clients, sender);
			g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
			return;
		}
	}

	obj = find_remote_by_path (object_path);
	if (!obj)
	{
		g_dbus_method_invocation_return_dbus_error (invocation, "org.zoitechat.Error", "No such object");
		return;
	}

	if (g_str_equal (method_name, "Command"))
	{
		const char *command;
		g_variant_get (parameters, "(&s)", &command);
		remote_object_command (obj, command);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
	}
	else if (g_str_equal (method_name, "Print"))
	{
		const char *text;
		g_variant_get (parameters, "(&s)", &text);
		remote_object_print (obj, text);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
	}
	else if (g_str_equal (method_name, "FindContext"))
	{
		const char *server;
		const char *channel;
		guint id;
		g_variant_get (parameters, "(&s&s)", &server, &channel);
		remote_object_find_context (obj, server, channel, &id);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", id));
	}
	else if (g_str_equal (method_name, "GetContext"))
	{
		guint id;
		remote_object_get_context (obj, &id);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", id));
	}
	else if (g_str_equal (method_name, "SetContext"))
	{
		guint id;
		gboolean ret;
		g_variant_get (parameters, "(u)", &id);
		remote_object_set_context (obj, id, &ret);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
	}
	else if (g_str_equal (method_name, "GetInfo"))
	{
		const char *id;
		char *ret_info;
		g_variant_get (parameters, "(&s)", &id);
		remote_object_get_info (obj, id, &ret_info);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", ret_info ? ret_info : ""));
		g_free (ret_info);
	}
	else if (g_str_equal (method_name, "GetPrefs"))
	{
		const char *name;
		int ret_type;
		char *ret_str;
		int ret_int;
		g_variant_get (parameters, "(&s)", &name);
		remote_object_get_prefs (obj, name, &ret_type, &ret_str, &ret_int);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(isi)", ret_type, ret_str ? ret_str : "", ret_int));
		g_free (ret_str);
	}
	else if (g_str_equal (method_name, "HookCommand"))
	{
		const char *name;
		int priority;
		const char *help;
		int rv;
		guint id;
		g_variant_get (parameters, "(&si&si)", &name, &priority, &help, &rv);
		remote_object_hook_command (obj, name, priority, help, rv, &id);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", id));
	}
	else if (g_str_equal (method_name, "HookServer"))
	{
		const char *name;
		int priority;
		int rv;
		guint id;
		g_variant_get (parameters, "(&sii)", &name, &priority, &rv);
		remote_object_hook_server (obj, name, priority, rv, &id);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", id));
	}
	else if (g_str_equal (method_name, "HookPrint"))
	{
		const char *name;
		int priority;
		int rv;
		guint id;
		g_variant_get (parameters, "(&sii)", &name, &priority, &rv);
		remote_object_hook_print (obj, name, priority, rv, &id);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", id));
	}
	else if (g_str_equal (method_name, "Unhook"))
	{
		guint id;
		g_variant_get (parameters, "(u)", &id);
		remote_object_unhook (obj, id);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
	}
	else if (g_str_equal (method_name, "ListGet"))
	{
		const char *name;
		guint id;
		g_variant_get (parameters, "(&s)", &name);
		remote_object_list_get (obj, name, &id);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", id));
	}
	else if (g_str_equal (method_name, "ListNext"))
	{
		guint id;
		gboolean ret;
		g_variant_get (parameters, "(u)", &id);
		remote_object_list_next (obj, id, &ret);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
	}
	else if (g_str_equal (method_name, "ListStr"))
	{
		guint id;
		const char *name;
		char *ret_str;
		g_variant_get (parameters, "(u&s)", &id, &name);
		remote_object_list_str (obj, id, name, &ret_str);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", ret_str ? ret_str : ""));
		g_free (ret_str);
	}
	else if (g_str_equal (method_name, "ListInt"))
	{
		guint id;
		const char *name;
		int ret;
		g_variant_get (parameters, "(u&s)", &id, &name);
		remote_object_list_int (obj, id, name, &ret);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", ret));
	}
	else if (g_str_equal (method_name, "ListTime"))
	{
		guint id;
		const char *name;
		guint64 ret;
		g_variant_get (parameters, "(u&s)", &id, &name);
		remote_object_list_time (obj, id, name, &ret);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(t)", ret));
	}
	else if (g_str_equal (method_name, "ListFields"))
	{
		const char *name;
		char **ret;
		g_variant_get (parameters, "(&s)", &name);
		remote_object_list_fields (name, &ret);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(^as)", ret));
		g_strfreev (ret);
	}
	else if (g_str_equal (method_name, "ListFree"))
	{
		guint id;
		g_variant_get (parameters, "(u)", &id);
		remote_object_list_free (obj, id);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
	}
	else if (g_str_equal (method_name, "EmitPrint"))
	{
		const char *event_name;
		GVariant *args;
		char **strv;
		gboolean ret;
		g_variant_get (parameters, "(&s@as)", &event_name, &args);
		strv = g_variant_dup_strv (args, NULL);
		remote_object_emit_print (obj, event_name, (const char **)strv, &ret);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", ret));
		g_strfreev (strv);
		g_variant_unref (args);
	}
	else if (g_str_equal (method_name, "Nickcmp"))
	{
		const char *n1;
		const char *n2;
		int ret;
		g_variant_get (parameters, "(&s&s)", &n1, &n2);
		remote_object_nickcmp (obj, n1, n2, &ret);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", ret));
	}
	else if (g_str_equal (method_name, "Strip"))
	{
		const char *str;
		int len;
		int flag;
		char *ret;
		g_variant_get (parameters, "(&sii)", &str, &len, &flag);
		remote_object_strip (str, len, flag, &ret);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", ret ? ret : ""));
		g_free (ret);
	}
	else if (g_str_equal (method_name, "SendModes"))
	{
		GVariant *targets;
		char **strv;
		int modes_per_line;
		guchar sign;
		guchar mode;
		g_variant_get (parameters, "(@asiyy)", &targets, &modes_per_line, &sign, &mode);
		strv = g_variant_dup_strv (targets, NULL);
		remote_object_send_modes (obj, (const char **)strv, modes_per_line, (char)sign, (char)mode);
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
		g_strfreev (strv);
		g_variant_unref (targets);
	}
	else
		g_dbus_method_invocation_return_dbus_error (invocation, "org.zoitechat.Error", "Unknown method");
}

static const GDBusInterfaceVTable connection_vtable = {
	method_call_cb,
	NULL,
	NULL
};

static void
name_owner_changed_cb (GDBusConnection *conn,
		      const gchar *sender_name,
		      const gchar *object_path,
		      const gchar *interface_name,
		      const gchar *signal_name,
		      GVariant *parameters,
		      gpointer user_data)
{
	const char *name;
	const char *old_owner;
	const char *new_owner;

	g_variant_get (parameters, "(&s&s&s)", &name, &old_owner, &new_owner);
	if (*new_owner == '\0')
		g_hash_table_remove (clients, name);
}

static gboolean
init_dbus (void)
{
	GError *error = NULL;
	GVariant *request_name_result;
	RemoteObject *root_remote;
	guint registration_id;

	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (!connection)
	{
		zoitechat_printf (ph, _("Couldn't connect to session bus: %s\n"), error->message);
		g_error_free (error);
		return FALSE;
	}

	introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, &error);
	if (!introspection_data)
	{
		zoitechat_printf (ph, _("Couldn't parse DBus introspection data: %s\n"), error->message);
		g_error_free (error);
		return FALSE;
	}

	request_name_result = g_dbus_connection_call_sync (
		connection,
		DBUS_SERVICE_DBUS,
		DBUS_PATH_DBUS,
		DBUS_INTERFACE_DBUS,
		"RequestName",
		g_variant_new ("(su)", DBUS_SERVICE, 1u),
		G_VARIANT_TYPE ("(u)"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		&error);
	if (!request_name_result)
	{
		zoitechat_printf (ph, _("Failed to acquire %s: %s\n"), DBUS_SERVICE, error->message);
		g_error_free (error);
		return FALSE;
	}
	g_variant_unref (request_name_result);

	registration_id = g_dbus_connection_register_object (
		connection,
		DBUS_OBJECT_PATH,
		introspection_data->interfaces[0],
		&connection_vtable,
		NULL,
		NULL,
		&error);
	if (!registration_id)
	{
		zoitechat_printf (ph, _("Failed to register %s: %s\n"), DBUS_OBJECT_PATH, error->message);
		g_error_free (error);
		return FALSE;
	}

	root_remote = remote_object_new (DBUS_OBJECT_PATH "/Remote");
	root_remote->registration_id = g_dbus_connection_register_object (
		connection,
		root_remote->dbus_path,
		introspection_data->interfaces[1],
		&connection_vtable,
		NULL,
		NULL,
		&error);
	if (!root_remote->registration_id)
	{
		zoitechat_printf (ph, _("Failed to register %s: %s\n"), root_remote->dbus_path, error->message);
		g_error_free (error);
		remote_object_free (root_remote);
		return FALSE;
	}
	g_hash_table_insert (clients, g_strdup ("__root__"), root_remote);

	name_owner_subscription = g_dbus_connection_signal_subscribe (
		connection,
		DBUS_SERVICE_DBUS,
		DBUS_INTERFACE_DBUS,
		"NameOwnerChanged",
		DBUS_PATH_DBUS,
		NULL,
		G_DBUS_SIGNAL_FLAGS_NONE,
		name_owner_changed_cb,
		NULL,
		NULL);

	return TRUE;
}

static char **
build_list (char *word[])
{
	guint i;
	guint num = 0;
	char **result;

	if (!word)
		return NULL;

	while (word[num] && word[num][0])
		num++;

	result = g_new0 (char *, num + 1);
	for (i = 0; i < num; i++)
		result[i] = g_strdup (word[i]);

	return result;
}

static guint
context_list_find_id (zoitechat_context *context)
{
	GList *l;

	for (l = contexts; l != NULL; l = l->next)
	{
		if (((ContextInfo *)l->data)->context == context)
			return ((ContextInfo *)l->data)->id;
	}

	return 0;
}

static zoitechat_context *
context_list_find_context (guint id)
{
	GList *l;

	for (l = contexts; l != NULL; l = l->next)
	{
		if (((ContextInfo *)l->data)->id == id)
			return ((ContextInfo *)l->data)->context;
	}

	return NULL;
}

static int
open_context_cb (char *word[],
		 void *userdata)
{
	ContextInfo *info = g_new0 (ContextInfo, 1);

	info->id = ++last_context_id;
	info->context = zoitechat_get_context (ph);
	contexts = g_list_prepend (contexts, info);
	return ZOITECHAT_EAT_NONE;
}

static int
close_context_cb (char *word[],
		  void *userdata)
{
	GList *l;
	zoitechat_context *context = zoitechat_get_context (ph);

	for (l = contexts; l != NULL; l = l->next)
	{
		if (((ContextInfo *)l->data)->context == context)
		{
			g_free (l->data);
			contexts = g_list_delete_link (contexts, l);
			break;
		}
	}

	return ZOITECHAT_EAT_NONE;
}

static gboolean
clients_find_filename_foreach (gpointer key,
			       gpointer value,
			       gpointer user_data)
{
	RemoteObject *obj = value;
	return g_str_equal (obj->filename, (char *)user_data);
}

static int
unload_plugin_cb (char *word[], char *word_eol[], void *userdata)
{
	RemoteObject *obj = g_hash_table_find (clients, clients_find_filename_foreach, word[2]);

	if (obj != NULL)
	{
		emit_signal (obj, "UnloadSignal", g_variant_new ("()"));
		return ZOITECHAT_EAT_ALL;
	}

	return ZOITECHAT_EAT_NONE;
}

int
dbus_plugin_init (zoitechat_plugin *plugin_handle,
		  char **plugin_name,
		  char **plugin_desc,
		  char **plugin_version,
		  char *arg)
{
	ph = plugin_handle;
	*plugin_name = PNAME;
	*plugin_desc = PDESC;
	*plugin_version = PVERSION;

	clients = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)remote_object_free);

	if (init_dbus ())
	{
		zoitechat_hook_print (ph, "Open Context", ZOITECHAT_PRI_NORM, open_context_cb, NULL);
		zoitechat_hook_print (ph, "Close Context", ZOITECHAT_PRI_NORM, close_context_cb, NULL);
		zoitechat_hook_command (ph, "unload", ZOITECHAT_PRI_HIGHEST, unload_plugin_cb, NULL, NULL);
	}

	return TRUE;
}
