/* ZoiteChat
 * Copyright (C) 1998-2010 Peter Zelezny.
 * Copyright (C) 2009-2013 Berke Viktor.
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

#ifndef ZOITECHAT_COMMONPLUGIN_H
#define ZOITECHAT_COMMONPLUGIN_H

#ifdef PLUGIN_C
struct _zoitechat_plugin
{
	/* Keep these in sync with zoitechat-plugin.h */
	/* !!don't change the order, to keep binary compat!! */
	zoitechat_hook *(*zoitechat_hook_command) (zoitechat_plugin *ph,
		    const char *name,
		    int pri,
		    int (*callback) (char *word[], char *word_eol[], void *user_data),
		    const char *help_text,
		    void *userdata);
	zoitechat_hook *(*zoitechat_hook_server) (zoitechat_plugin *ph,
		   const char *name,
		   int pri,
		   int (*callback) (char *word[], char *word_eol[], void *user_data),
		   void *userdata);
	zoitechat_hook *(*zoitechat_hook_print) (zoitechat_plugin *ph,
		  const char *name,
		  int pri,
		  int (*callback) (char *word[], void *user_data),
		  void *userdata);
	zoitechat_hook *(*zoitechat_hook_timer) (zoitechat_plugin *ph,
		  int timeout,
		  int (*callback) (void *user_data),
		  void *userdata);
	zoitechat_hook *(*zoitechat_hook_fd) (zoitechat_plugin *ph,
		   int fd,
		   int flags,
		   int (*callback) (int fd, int flags, void *user_data),
		   void *userdata);
	void *(*zoitechat_unhook) (zoitechat_plugin *ph,
	      zoitechat_hook *hook);
	void (*zoitechat_print) (zoitechat_plugin *ph,
	     const char *text);
	void (*zoitechat_printf) (zoitechat_plugin *ph,
	      const char *format, ...);
	void (*zoitechat_command) (zoitechat_plugin *ph,
	       const char *command);
	void (*zoitechat_commandf) (zoitechat_plugin *ph,
		const char *format, ...);
	int (*zoitechat_nickcmp) (zoitechat_plugin *ph,
	       const char *s1,
	       const char *s2);
	int (*zoitechat_set_context) (zoitechat_plugin *ph,
		   zoitechat_context *ctx);
	zoitechat_context *(*zoitechat_find_context) (zoitechat_plugin *ph,
		    const char *servname,
		    const char *channel);
	zoitechat_context *(*zoitechat_get_context) (zoitechat_plugin *ph);
	const char *(*zoitechat_get_info) (zoitechat_plugin *ph,
		const char *id);
	int (*zoitechat_get_prefs) (zoitechat_plugin *ph,
		 const char *name,
		 const char **string,
		 int *integer);
	zoitechat_list * (*zoitechat_list_get) (zoitechat_plugin *ph,
		const char *name);
	void (*zoitechat_list_free) (zoitechat_plugin *ph,
		 zoitechat_list *xlist);
	const char * const * (*zoitechat_list_fields) (zoitechat_plugin *ph,
		   const char *name);
	int (*zoitechat_list_next) (zoitechat_plugin *ph,
		 zoitechat_list *xlist);
	const char * (*zoitechat_list_str) (zoitechat_plugin *ph,
		zoitechat_list *xlist,
		const char *name);
	int (*zoitechat_list_int) (zoitechat_plugin *ph,
		zoitechat_list *xlist,
		const char *name);
	void * (*zoitechat_plugingui_add) (zoitechat_plugin *ph,
		     const char *filename,
		     const char *name,
		     const char *desc,
		     const char *version,
		     char *reserved);
	void (*zoitechat_plugingui_remove) (zoitechat_plugin *ph,
			void *handle);
	int (*zoitechat_emit_print) (zoitechat_plugin *ph,
			const char *event_name, ...);
	void *(*zoitechat_read_fd) (zoitechat_plugin *ph);
	time_t (*zoitechat_list_time) (zoitechat_plugin *ph,
		zoitechat_list *xlist,
		const char *name);
	char *(*zoitechat_gettext) (zoitechat_plugin *ph,
		const char *msgid);
	void (*zoitechat_send_modes) (zoitechat_plugin *ph,
		  const char **targets,
		  int ntargets,
		  int modes_per_line,
		  char sign,
		  char mode);
	char *(*zoitechat_strip) (zoitechat_plugin *ph,
	     const char *str,
	     int len,
	     int flags);
	void (*zoitechat_free) (zoitechat_plugin *ph,
	    void *ptr);
	int (*zoitechat_pluginpref_set_str) (zoitechat_plugin *ph,
		const char *var,
		const char *value);
	int (*zoitechat_pluginpref_get_str) (zoitechat_plugin *ph,
		const char *var,
		char *dest);
	int (*zoitechat_pluginpref_set_int) (zoitechat_plugin *ph,
		const char *var,
		int value);
	int (*zoitechat_pluginpref_get_int) (zoitechat_plugin *ph,
		const char *var);
	int (*zoitechat_pluginpref_delete) (zoitechat_plugin *ph,
		const char *var);
	int (*zoitechat_pluginpref_list) (zoitechat_plugin *ph,
		char *dest);
	zoitechat_hook *(*zoitechat_hook_server_attrs) (zoitechat_plugin *ph,
		   const char *name,
		   int pri,
		   int (*callback) (char *word[], char *word_eol[],
							zoitechat_event_attrs *attrs, void *user_data),
		   void *userdata);
	zoitechat_hook *(*zoitechat_hook_print_attrs) (zoitechat_plugin *ph,
		  const char *name,
		  int pri,
		  int (*callback) (char *word[], zoitechat_event_attrs *attrs,
						   void *user_data),
		  void *userdata);
	int (*zoitechat_emit_print_attrs) (zoitechat_plugin *ph, zoitechat_event_attrs *attrs,
									 const char *event_name, ...);
	zoitechat_event_attrs *(*zoitechat_event_attrs_create) (zoitechat_plugin *ph);
	void (*zoitechat_event_attrs_free) (zoitechat_plugin *ph,
									  zoitechat_event_attrs *attrs);

	/* PRIVATE FIELDS! */
	void *handle;		/* from dlopen */
	char *filename;	/* loaded from */
	char *name;
	char *desc;
	char *version;
	session *context;
	void *deinit_callback;	/* pointer to zoitechat_plugin_deinit */
	unsigned int fake:1;		/* fake plugin. Added by zoitechat_plugingui_add() */
	unsigned int free_strings:1;		/* free name,desc,version? */
};
#endif

GModule *module_load (char *filename);
char *plugin_load (session *sess, char *filename, char *arg);
int plugin_reload (session *sess, char *name, int by_filename);
void plugin_add (session *sess, char *filename, void *handle, void *init_func, void *deinit_func, char *arg, int fake);
int plugin_kill (char *name, int by_filename);
void plugin_kill_all (void);
void plugin_auto_load (session *sess);
int plugin_emit_command (session *sess, char *name, char *word[], char *word_eol[]);
int plugin_emit_server (session *sess, char *name, char *word[], char *word_eol[],
						time_t server_time);
int plugin_emit_print (session *sess, char *word[], time_t server_time);
int plugin_emit_dummy_print (session *sess, char *name);
int plugin_emit_keypress (session *sess, unsigned int state, unsigned int keyval, gunichar key);
GList* plugin_command_list(GList *tmp_list);
int plugin_show_help (session *sess, char *cmd);
void plugin_command_foreach (session *sess, void *userdata, void (*cb) (session *sess, void *userdata, char *name, char *usage));
session *plugin_find_context (const char *servname, const char *channel, server *current_server);

/* On macOS, G_MODULE_SUFFIX says "so" but meson uses "dylib"
 * https://github.com/mesonbuild/meson/issues/1160 */
#if defined(__APPLE__)
#  define PLUGIN_SUFFIX "dylib"
#else
#  define PLUGIN_SUFFIX G_MODULE_SUFFIX
#endif

#endif
