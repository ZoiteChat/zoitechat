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

/* You can distribute this header with your plugins for easy compilation */
#ifndef ZOITECHAT_PLUGIN_H
#define ZOITECHAT_PLUGIN_H

#include <time.h>

#define ZOITECHAT_PRI_HIGHEST	127
#define ZOITECHAT_PRI_HIGH		64
#define ZOITECHAT_PRI_NORM		0
#define ZOITECHAT_PRI_LOW		(-64)
#define ZOITECHAT_PRI_LOWEST	(-128)

#define ZOITECHAT_FD_READ		1
#define ZOITECHAT_FD_WRITE		2
#define ZOITECHAT_FD_EXCEPTION	4
#define ZOITECHAT_FD_NOTSOCKET	8

#define ZOITECHAT_EAT_NONE		0	/* pass it on through! */
#define ZOITECHAT_EAT_ZOITECHAT		1	/* don't let ZoiteChat see this event */
#define ZOITECHAT_EAT_PLUGIN	2	/* don't let other plugins see this event */
#define ZOITECHAT_EAT_ALL		(ZOITECHAT_EAT_ZOITECHAT|ZOITECHAT_EAT_PLUGIN)	/* don't let anything see this event */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _zoitechat_plugin zoitechat_plugin;
typedef struct _zoitechat_list zoitechat_list;
typedef struct _zoitechat_hook zoitechat_hook;
#ifndef PLUGIN_C
typedef struct _zoitechat_context zoitechat_context;
#endif
typedef struct
{
	time_t server_time_utc; /* 0 if not used */
} zoitechat_event_attrs;

#ifndef PLUGIN_C
struct _zoitechat_plugin
{
	/* these are only used on win32 */
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
	      const char *format, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 2, 3)))
#endif
	;
	void (*zoitechat_command) (zoitechat_plugin *ph,
	       const char *command);
	void (*zoitechat_commandf) (zoitechat_plugin *ph,
		const char *format, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 2, 3)))
#endif
	;
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
	int (*zoitechat_read_fd) (zoitechat_plugin *ph,
			void *src,
			char *buf,
			int *len);
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
};
#endif


zoitechat_hook *
zoitechat_hook_command (zoitechat_plugin *ph,
		    const char *name,
		    int pri,
		    int (*callback) (char *word[], char *word_eol[], void *user_data),
		    const char *help_text,
		    void *userdata);

zoitechat_event_attrs *zoitechat_event_attrs_create (zoitechat_plugin *ph);

void zoitechat_event_attrs_free (zoitechat_plugin *ph, zoitechat_event_attrs *attrs);

zoitechat_hook *
zoitechat_hook_server (zoitechat_plugin *ph,
		   const char *name,
		   int pri,
		   int (*callback) (char *word[], char *word_eol[], void *user_data),
		   void *userdata);

zoitechat_hook *
zoitechat_hook_server_attrs (zoitechat_plugin *ph,
		   const char *name,
		   int pri,
		   int (*callback) (char *word[], char *word_eol[],
							zoitechat_event_attrs *attrs, void *user_data),
		   void *userdata);

zoitechat_hook *
zoitechat_hook_print (zoitechat_plugin *ph,
		  const char *name,
		  int pri,
		  int (*callback) (char *word[], void *user_data),
		  void *userdata);

zoitechat_hook *
zoitechat_hook_print_attrs (zoitechat_plugin *ph,
		  const char *name,
		  int pri,
		  int (*callback) (char *word[], zoitechat_event_attrs *attrs,
						   void *user_data),
		  void *userdata);

zoitechat_hook *
zoitechat_hook_timer (zoitechat_plugin *ph,
		  int timeout,
		  int (*callback) (void *user_data),
		  void *userdata);

zoitechat_hook *
zoitechat_hook_fd (zoitechat_plugin *ph,
		int fd,
		int flags,
		int (*callback) (int fd, int flags, void *user_data),
		void *userdata);

void *
zoitechat_unhook (zoitechat_plugin *ph,
	      zoitechat_hook *hook);

void
zoitechat_print (zoitechat_plugin *ph,
	     const char *text);

void
zoitechat_printf (zoitechat_plugin *ph,
	      const char *format, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 2, 3)))
#endif
;

void
zoitechat_command (zoitechat_plugin *ph,
	       const char *command);

void
zoitechat_commandf (zoitechat_plugin *ph,
		const char *format, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 2, 3)))
#endif
;

int
zoitechat_nickcmp (zoitechat_plugin *ph,
	       const char *s1,
	       const char *s2);

int
zoitechat_set_context (zoitechat_plugin *ph,
		   zoitechat_context *ctx);

zoitechat_context *
zoitechat_find_context (zoitechat_plugin *ph,
		    const char *servname,
		    const char *channel);

zoitechat_context *
zoitechat_get_context (zoitechat_plugin *ph);

const char *
zoitechat_get_info (zoitechat_plugin *ph,
		const char *id);

int
zoitechat_get_prefs (zoitechat_plugin *ph,
		 const char *name,
		 const char **string,
		 int *integer);

zoitechat_list *
zoitechat_list_get (zoitechat_plugin *ph,
		const char *name);

void
zoitechat_list_free (zoitechat_plugin *ph,
		 zoitechat_list *xlist);

const char * const *
zoitechat_list_fields (zoitechat_plugin *ph,
		   const char *name);

int
zoitechat_list_next (zoitechat_plugin *ph,
		 zoitechat_list *xlist);

const char *
zoitechat_list_str (zoitechat_plugin *ph,
		zoitechat_list *xlist,
		const char *name);

int
zoitechat_list_int (zoitechat_plugin *ph,
		zoitechat_list *xlist,
		const char *name);

time_t
zoitechat_list_time (zoitechat_plugin *ph,
		 zoitechat_list *xlist,
		 const char *name);

void *
zoitechat_plugingui_add (zoitechat_plugin *ph,
		     const char *filename,
		     const char *name,
		     const char *desc,
		     const char *version,
		     char *reserved);

void
zoitechat_plugingui_remove (zoitechat_plugin *ph,
			void *handle);

int 
zoitechat_emit_print (zoitechat_plugin *ph,
		  const char *event_name, ...);

int 
zoitechat_emit_print_attrs (zoitechat_plugin *ph, zoitechat_event_attrs *attrs,
						  const char *event_name, ...);

char *
zoitechat_gettext (zoitechat_plugin *ph,
	       const char *msgid);

void
zoitechat_send_modes (zoitechat_plugin *ph,
		  const char **targets,
		  int ntargets,
		  int modes_per_line,
		  char sign,
		  char mode);

char *
zoitechat_strip (zoitechat_plugin *ph,
	     const char *str,
	     int len,
	     int flags);

void
zoitechat_free (zoitechat_plugin *ph,
	    void *ptr);

int
zoitechat_pluginpref_set_str (zoitechat_plugin *ph,
		const char *var,
		const char *value);

int
zoitechat_pluginpref_get_str (zoitechat_plugin *ph,
		const char *var,
		char *dest);

int
zoitechat_pluginpref_set_int (zoitechat_plugin *ph,
		const char *var,
		int value);
int
zoitechat_pluginpref_get_int (zoitechat_plugin *ph,
		const char *var);

int
zoitechat_pluginpref_delete (zoitechat_plugin *ph,
		const char *var);

int
zoitechat_pluginpref_list (zoitechat_plugin *ph,
		char *dest);

#if !defined(PLUGIN_C) && (defined(WIN32) || defined(__CYGWIN__))
#ifndef ZOITECHAT_PLUGIN_HANDLE
#define ZOITECHAT_PLUGIN_HANDLE (ph)
#endif
#define zoitechat_hook_command ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_hook_command)
#define zoitechat_event_attrs_create ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_event_attrs_create)
#define zoitechat_event_attrs_free ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_event_attrs_free)
#define zoitechat_hook_server ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_hook_server)
#define zoitechat_hook_server_attrs ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_hook_server_attrs)
#define zoitechat_hook_print ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_hook_print)
#define zoitechat_hook_print_attrs ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_hook_print_attrs)
#define zoitechat_hook_timer ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_hook_timer)
#define zoitechat_hook_fd ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_hook_fd)
#define zoitechat_unhook ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_unhook)
#define zoitechat_print ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_print)
#define zoitechat_printf ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_printf)
#define zoitechat_command ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_command)
#define zoitechat_commandf ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_commandf)
#define zoitechat_nickcmp ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_nickcmp)
#define zoitechat_set_context ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_set_context)
#define zoitechat_find_context ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_find_context)
#define zoitechat_get_context ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_get_context)
#define zoitechat_get_info ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_get_info)
#define zoitechat_get_prefs ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_get_prefs)
#define zoitechat_list_get ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_list_get)
#define zoitechat_list_free ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_list_free)
#define zoitechat_list_fields ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_list_fields)
#define zoitechat_list_next ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_list_next)
#define zoitechat_list_str ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_list_str)
#define zoitechat_list_int ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_list_int)
#define zoitechat_plugingui_add ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_plugingui_add)
#define zoitechat_plugingui_remove ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_plugingui_remove)
#define zoitechat_emit_print ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_emit_print)
#define zoitechat_emit_print_attrs ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_emit_print_attrs)
#define zoitechat_list_time ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_list_time)
#define zoitechat_gettext ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_gettext)
#define zoitechat_send_modes ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_send_modes)
#define zoitechat_strip ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_strip)
#define zoitechat_free ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_free)
#define zoitechat_pluginpref_set_str ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_pluginpref_set_str)
#define zoitechat_pluginpref_get_str ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_pluginpref_get_str)
#define zoitechat_pluginpref_set_int ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_pluginpref_set_int)
#define zoitechat_pluginpref_get_int ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_pluginpref_get_int)
#define zoitechat_pluginpref_delete ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_pluginpref_delete)
#define zoitechat_pluginpref_list ((ZOITECHAT_PLUGIN_HANDLE)->zoitechat_pluginpref_list)
#endif

#ifdef __cplusplus
}
#endif
#endif
