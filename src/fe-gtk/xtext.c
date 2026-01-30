/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
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
 * =========================================================================
 *
 * xtext, the text widget used by X-Chat.
 * By Peter Zelezny <zed@xchat.org>.
 *
 */

#define GDK_MULTIHEAD_SAFE
#define MARGIN 2						/* dont touch. */
#define REFRESH_TIMEOUT 20
#define WORDWRAP_LIMIT 24

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "../common/zoitechat.h"
#include "../common/fe.h"
#include "../common/util.h"
#include "../common/zoitechatc.h"
#include "../common/url.h"

#ifdef WIN32
#include "marshal.h"
#else
#include "../common/marshal.h"
#endif

#include "fe-gtk.h"
#include "xtext.h"
#include "fkeys.h"

#define charlen(str) g_utf8_skip[*(guchar *)(str)]

#ifdef WIN32
#include <windows.h>
#include <io.h>
#include <gdk/gdk.h>
#include <gdk/gdkcairo.h>
#include <gdk/gdkwin32.h>
#else
#include <unistd.h>
#include <gdk/gdkcairo.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#endif
#include <pango/pangocairo.h>

/* is delimiter */
#define is_del(c) \
	(c == ' ' || c == '\n' || c == '>' || c == '<' || c == 0)

/* force scrolling off */
#define dontscroll(buf) (buf)->last_pixel_pos = 0x7fffffff

static GtkWidgetClass *parent_class = NULL;

struct textentry
{
	struct textentry *next;
	struct textentry *prev;
	unsigned char *str;
	time_t stamp;
	gint16 str_width;
	gint16 str_len;
	gint16 mark_start;
	gint16 mark_end;
	gint16 indent;
	gint16 left_len;
	GSList *slp;
	GSList *sublines;
	guchar tag;
	guchar pad1;
	guchar pad2;	/* 32-bit align : 44 bytes total */
	GList *marks;	/* List of found strings */
};

enum
{
	WORD_CLICK,
	SET_SCROLL_ADJUSTMENTS,
	LAST_SIGNAL
};

/* values for selection info */
enum
{
	TARGET_UTF8_STRING,
	TARGET_STRING,
	TARGET_TEXT,
	TARGET_COMPOUND_TEXT
};

static guint xtext_signals[LAST_SIGNAL];

char *nocasestrstr (const char *text, const char *tofind);	/* util.c */
int xtext_get_stamp_str (time_t, char **);
static void gtk_xtext_render_page (GtkXText * xtext);
static void gtk_xtext_calc_lines (xtext_buffer *buf, int);
static gboolean gtk_xtext_is_selecting (GtkXText *xtext);
static char *gtk_xtext_selection_get_text (GtkXText *xtext, int *len_ret);
static textentry *gtk_xtext_nth (GtkXText *xtext, int line, int *subline);
static void gtk_xtext_adjustment_changed (GtkAdjustment * adj,
														GtkXText * xtext);
static void gtk_xtext_scroll_adjustments (GtkXText *xtext, GtkAdjustment *hadj,
										GtkAdjustment *vadj);
static int gtk_xtext_render_ents (GtkXText * xtext, textentry *, textentry *);
static void gtk_xtext_recalc_widths (xtext_buffer *buf, int);
static void gtk_xtext_fix_indent (xtext_buffer *buf);
static int gtk_xtext_find_subline (GtkXText *xtext, textentry *ent, int line);
/* static char *gtk_xtext_conv_color (unsigned char *text, int len, int *newlen); */
/* For use by gtk_xtext_strip_color() and its callers -- */
struct offlen_s {
	guint16 off;
	guint16 len;
	guint16 emph;
	guint16 width;
};
typedef struct offlen_s offlen_t;
static unsigned char *
gtk_xtext_strip_color (unsigned char *text, int len, unsigned char *outbuf,
							  int *newlen, GSList **slp, int strip_hidden);
static gboolean gtk_xtext_check_ent_visibility (GtkXText * xtext, textentry *find_ent, int add);
static int gtk_xtext_render_page_timeout (GtkXText * xtext);
static int gtk_xtext_search_offset (xtext_buffer *buf, textentry *ent, unsigned int off);
static GList * gtk_xtext_search_textentry (xtext_buffer *, textentry *);
static void gtk_xtext_search_textentry_add (xtext_buffer *, textentry *, GList *, gboolean);
static void gtk_xtext_search_textentry_del (xtext_buffer *, textentry *);
static void gtk_xtext_search_textentry_fini (gpointer, gpointer);
static void gtk_xtext_search_fini (xtext_buffer *);
static gboolean gtk_xtext_search_init (xtext_buffer *buf, const gchar *text, gtk_xtext_search_flags flags, GError **perr);
static char * gtk_xtext_get_word (GtkXText * xtext, int x, int y, textentry ** ret_ent, int *ret_off, int *ret_len, GSList **slp);

static inline void
gtk_xtext_cursor_unref (GdkCursor *cursor)
{
	if (!cursor)
		return;
#if !HAVE_GTK3
	gdk_cursor_unref (cursor);
#else
	g_object_unref (cursor);
#endif
}

static inline void
xtext_set_source_color (cairo_t *cr, const XTextColor *color, gdouble alpha)
{
	cairo_set_source_rgba (cr, color->red, color->green,
		color->blue, color->alpha * alpha);
}

static inline gdouble
xtext_adj_get_value (GtkAdjustment *adj)
{
#if !HAVE_GTK3
	return adj->value;
#else
	return gtk_adjustment_get_value (adj);
#endif
}

static inline void
xtext_adj_set_value (GtkAdjustment *adj, gdouble value)
{
#if !HAVE_GTK3
	adj->value = value;
#else
	gtk_adjustment_set_value (adj, value);
#endif
}

static inline gdouble
xtext_adj_get_upper (GtkAdjustment *adj)
{
#if !HAVE_GTK3
	return adj->upper;
#else
	return gtk_adjustment_get_upper (adj);
#endif
}

static inline void
xtext_adj_set_upper (GtkAdjustment *adj, gdouble upper)
{
#if !HAVE_GTK3
	adj->upper = upper;
#else
	gtk_adjustment_set_upper (adj, upper);
#endif
}

static inline gdouble
xtext_adj_get_lower (GtkAdjustment *adj)
{
#if !HAVE_GTK3
	return adj->lower;
#else
	return gtk_adjustment_get_lower (adj);
#endif
}

static inline void
xtext_adj_set_lower (GtkAdjustment *adj, gdouble lower)
{
#if !HAVE_GTK3
	adj->lower = lower;
#else
	gtk_adjustment_set_lower (adj, lower);
#endif
}

static inline gdouble
xtext_adj_get_page_size (GtkAdjustment *adj)
{
#if !HAVE_GTK3
	return adj->page_size;
#else
	return gtk_adjustment_get_page_size (adj);
#endif
}

static inline void
xtext_adj_set_page_size (GtkAdjustment *adj, gdouble page_size)
{
#if !HAVE_GTK3
	adj->page_size = page_size;
#else
	gtk_adjustment_set_page_size (adj, page_size);
#endif
}

static inline gdouble
xtext_adj_get_page_increment (GtkAdjustment *adj)
{
#if !HAVE_GTK3
	return adj->page_increment;
#else
	return gtk_adjustment_get_page_increment (adj);
#endif
}

static void
xtext_adjustment_apply (GtkAdjustment *adj, gdouble lower, gdouble upper, gdouble value, gdouble page_size)
{
#if HAVE_GTK3
	gtk_adjustment_set_lower (adj, lower);
	gtk_adjustment_set_upper (adj, upper);
	gtk_adjustment_set_page_size (adj, page_size);
	gtk_adjustment_set_value (adj, value);
#else
	adj->lower = lower;
	adj->upper = upper;
	adj->page_size = page_size;
	adj->value = value;
#endif
}

static inline void
xtext_adj_set_page_increment (GtkAdjustment *adj, gdouble page_increment)
{
#if !HAVE_GTK3
	adj->page_increment = page_increment;
#else
	gtk_adjustment_set_page_increment (adj, page_increment);
#endif
}

static cairo_surface_t *
xtext_surface_from_window (GdkWindow *window)
{
	cairo_surface_t *surface = NULL;
	int width;
	int height;
	cairo_t *cr;

	if (!window)
		return NULL;

	width = gdk_window_get_width (window);
	height = gdk_window_get_height (window);
	if (width <= 0 || height <= 0)
		return NULL;

	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
	{
		cairo_surface_destroy (surface);
		return NULL;
	}

	cr = cairo_create (surface);
	gdk_cairo_set_source_window (cr, window, 0.0, 0.0);
	cairo_paint (cr);
	cairo_destroy (cr);

	return surface;
}

static cairo_t *
xtext_create_context (GtkXText *xtext)
{
	if (xtext->draw_surface)
		return cairo_create (xtext->draw_surface);

	return gdk_cairo_create (xtext->draw_window);
}

static inline void
xtext_draw_rectangle (GtkXText *xtext, cairo_t *cr, const XTextColor *color, int x, int y, int width, int height)
{
	cairo_save (cr);

	xtext_set_source_color (cr, color, 1.0);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_rectangle (cr, (double)x, (double)y, (double)width, (double)height);
	cairo_fill (cr);

	cairo_restore (cr);
}

static inline void
xtext_draw_line (GtkXText *xtext, cairo_t *cr, const XTextColor *color, int x1, int y1, int x2, int y2)
{
	cairo_save (cr);

	/* Disable antialiasing for crispy 1-pixel lines */
	cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
	xtext_set_source_color (cr, color, 1.0);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_line_width (cr, 1.0);
	cairo_move_to (cr, x1, y1);
	cairo_line_to (cr, x2, y2);
	cairo_stroke (cr);

	cairo_restore (cr);
}

static inline void
xtext_draw_bg_offset (GtkXText *xtext, int x, int y, int width, int height, int tile_x, int tile_y)
{
	cairo_t *cr = xtext_create_context (xtext);

	if (xtext->background_surface)
	{
		cairo_set_source_surface (cr, xtext->background_surface, tile_x, tile_y);
		cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
		cairo_rectangle (cr, (double)x, (double)y, (double)width, (double)height);
		cairo_fill (cr);
	}
	else
	{
		xtext_draw_rectangle (xtext, cr, &xtext->bgc, x, y, width, height);
	}

	cairo_destroy (cr);
}

static inline void
xtext_draw_bg (GtkXText *xtext, int x, int y, int width, int height)
{
	xtext_draw_bg_offset (xtext, x, y, width, height, xtext->ts_x, xtext->ts_y);
}

/* ======================================= */
/* ============ PANGO BACKEND ============ */
/* ======================================= */

#define EMPH_ITAL 1
#define EMPH_BOLD 2
#define EMPH_HIDDEN 4
static PangoAttrList *attr_lists[4];
static int fontwidths[4][128];

static PangoAttribute *
xtext_pango_attr (PangoAttribute *attr)
{
	attr->start_index = PANGO_ATTR_INDEX_FROM_TEXT_BEGINNING;
	attr->end_index = PANGO_ATTR_INDEX_TO_TEXT_END;
	return attr;
}

static void
xtext_pango_init (GtkXText *xtext)
{
	size_t i;
	int j;
	char buf[2] = "\000";

	if (attr_lists[0])
	{
		for (i = 0; i < (EMPH_ITAL | EMPH_BOLD); i++)
			pango_attr_list_unref (attr_lists[i]);
	}

	for (i = 0; i < sizeof attr_lists / sizeof attr_lists[0]; i++)
	{
		attr_lists[i] = pango_attr_list_new ();
		switch (i)
		{
		case 0:		/* Roman */
			break;
		case EMPH_ITAL:		/* Italic */
			pango_attr_list_insert (attr_lists[i],
				xtext_pango_attr (pango_attr_style_new (PANGO_STYLE_ITALIC)));
			break;
		case EMPH_BOLD:		/* Bold */
			pango_attr_list_insert (attr_lists[i],
				xtext_pango_attr (pango_attr_weight_new (PANGO_WEIGHT_BOLD)));
			break;
		case EMPH_ITAL | EMPH_BOLD:		/* Italic Bold */
			pango_attr_list_insert (attr_lists[i],
				xtext_pango_attr (pango_attr_style_new (PANGO_STYLE_ITALIC)));
			pango_attr_list_insert (attr_lists[i],
				xtext_pango_attr (pango_attr_weight_new (PANGO_WEIGHT_BOLD)));
			break;
		}

		/* Now initialize fontwidths[i] */
		pango_layout_set_attributes (xtext->layout, attr_lists[i]);
		for (j = 0; j < 128; j++)
		{
			buf[0] = j;
			pango_layout_set_text (xtext->layout, buf, 1);
			pango_layout_get_pixel_size (xtext->layout, &fontwidths[i][j], NULL);
		}
	}
	xtext->space_width = fontwidths[0][' '];
}

static void
backend_font_close (GtkXText *xtext)
{
	pango_font_description_free (xtext->font->font);
}

static void
backend_init (GtkXText *xtext)
{
	if (xtext->layout == NULL)
	{
		xtext->layout = gtk_widget_create_pango_layout (GTK_WIDGET (xtext), 0); 
		if (xtext->font)
			pango_layout_set_font_description (xtext->layout, xtext->font->font);
	}
}

static void
backend_deinit (GtkXText *xtext)
{
	if (xtext->layout)
	{
		g_object_unref (xtext->layout);
		xtext->layout = NULL;
	}
}

static PangoFontDescription *
backend_font_open_real (char *name)
{
	PangoFontDescription *font;

	font = pango_font_description_from_string (name);
	if (font && pango_font_description_get_size (font) == 0)
	{
		pango_font_description_free (font);
		font = pango_font_description_from_string ("sans 11");
	}
	if (!font)
		font = pango_font_description_from_string ("sans 11");

	return font;
}

static void
backend_font_open (GtkXText *xtext, char *name)
{
	PangoLanguage *lang;
	PangoContext *context;
	PangoFontMetrics *metrics;

	xtext->font = &xtext->pango_font;
	xtext->font->font = backend_font_open_real (name);
	if (!xtext->font->font)
	{
		xtext->font = NULL;
		return;
	}

	backend_init (xtext);
	pango_layout_set_font_description (xtext->layout, xtext->font->font);
	xtext_pango_init (xtext);

	/* vte does it this way */
	context = gtk_widget_get_pango_context (GTK_WIDGET (xtext));
	lang = pango_context_get_language (context);
	metrics = pango_context_get_metrics (context, xtext->font->font, lang);
	xtext->font->ascent = pango_font_metrics_get_ascent (metrics) / PANGO_SCALE;
	xtext->font->descent = pango_font_metrics_get_descent (metrics) / PANGO_SCALE;

	/*
	 * In later versions of pango, a font's height should be calculated like
	 * this to account for line gap; a typical symptom of not doing so is
	 * cutting off the underscore on some fonts.
	 */
#if PANGO_VERSION_CHECK(1, 44, 0)
	xtext->fontsize = pango_font_metrics_get_height (metrics) / PANGO_SCALE + 1;

	if (xtext->fontsize == 0)
		xtext->fontsize = xtext->font->ascent + xtext->font->descent;
#else
	xtext->fontsize = xtext->font->ascent + xtext->font->descent;
#endif

	pango_font_metrics_unref (metrics);
}

static int
backend_get_text_width_emph (GtkXText *xtext, guchar *str, int len, int emphasis)
{
	int width;
	int deltaw;
	int mbl;

	if (*str == 0)
		return 0;

	if ((emphasis & EMPH_HIDDEN))
		return 0;
	emphasis &= (EMPH_ITAL | EMPH_BOLD);

	width = 0;
	pango_layout_set_attributes (xtext->layout, attr_lists[emphasis]);
	while (len > 0)
	{
		mbl = charlen(str);
		if (*str < 128)
			deltaw = fontwidths[emphasis][*str];
		else
		{
			pango_layout_set_text (xtext->layout, str, mbl);
			pango_layout_get_pixel_size (xtext->layout, &deltaw, NULL);
		}
		width += deltaw;
		str += mbl;
		len -= mbl;
	}

	return width;
}

static int
backend_get_text_width_slp (GtkXText *xtext, guchar *str, GSList *slp)
{
	int width = 0;

	while (slp)
	{
		offlen_t *meta;

		meta = slp->data;
		width += backend_get_text_width_emph (xtext, str, meta->len, meta->emph);
		str += meta->len;
		slp = g_slist_next (slp);
	}

	return width;
}

static void
backend_draw_text_emph (GtkXText *xtext, gboolean dofill, int x, int y,
						 char *str, int len, int str_width, int emphasis)
{
	cairo_t *cr;
	PangoLayoutLine *line;

	cr = xtext_create_context (xtext);

	pango_layout_set_attributes (xtext->layout, attr_lists[emphasis]);
	pango_layout_set_text (xtext->layout, str, len);

	if (dofill)
	{
		xtext_draw_rectangle (xtext, cr, &xtext->bgc, x, y -
									xtext->font->ascent, str_width, xtext->fontsize);
	}

	xtext_set_source_color (cr, &xtext->fgc, 1.0);
	line = pango_layout_get_line_readonly (xtext->layout, 0);

	cairo_save (cr);
	cairo_move_to (cr, x, y);
	pango_cairo_show_layout_line (cr, line);
	cairo_restore (cr);

	cairo_destroy (cr);
}

static void
xtext_set_fg (GtkXText *xtext, int index)
{
	xtext->fgc = xtext->palette[index];
}

static void
xtext_set_bg (GtkXText *xtext, int index)
{
	xtext->bgc = xtext->palette[index];
}

static void
gtk_xtext_init (GtkXText * xtext)
{
	xtext->background_surface = NULL;
	xtext->draw_window = NULL;
	xtext->draw_surface = NULL;
	xtext->io_tag = 0;
	xtext->add_io_tag = 0;
	xtext->scroll_tag = 0;
	xtext->max_lines = 0;
	xtext->col_back = XTEXT_BG;
	xtext->col_fore = XTEXT_FG;
	xtext->nc = 0;
	xtext->pixel_offset = 0;
	xtext->underline = FALSE;
	xtext->strikethrough = FALSE;
	xtext->hidden = FALSE;
	xtext->font = NULL;
	xtext->layout = NULL;
	xtext->jump_out_offset = 0;
	xtext->jump_in_offset = 0;
	xtext->ts_x = 0;
	xtext->ts_y = 0;
	xtext->clip_x = 0;
	xtext->clip_x2 = 1000000;
	xtext->clip_y = 0;
	xtext->clip_y2 = 1000000;
	xtext->urlcheck_function = NULL;
	xtext->color_paste = FALSE;
	xtext->skip_border_fills = FALSE;
	xtext->skip_stamp = FALSE;
	xtext->render_hilights_only = FALSE;
	xtext->un_hilight = FALSE;
	xtext->recycle = FALSE;
	xtext->dont_render = FALSE;
	xtext->dont_render2 = FALSE;
	gtk_xtext_scroll_adjustments (xtext, NULL, NULL);

	{
		static const GtkTargetEntry targets[] = {
			{ "UTF8_STRING", 0, TARGET_UTF8_STRING },
			{ "STRING", 0, TARGET_STRING },
			{ "TEXT",   0, TARGET_TEXT }, 
			{ "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT }
		};
		static const gint n_targets = sizeof (targets) / sizeof (targets[0]);

		gtk_selection_add_targets (GTK_WIDGET (xtext), GDK_SELECTION_PRIMARY,
											targets, n_targets);
	}
}

static void
gtk_xtext_adjustment_set (xtext_buffer *buf, int fire_signal)
{
	GtkAdjustment *adj = buf->xtext->adj;

	if (buf->xtext->buffer == buf)
	{
		gdouble lower = 0;
		gdouble upper = buf->num_lines;
		gdouble value = xtext_adj_get_value (adj);
		gdouble page_size;

		if (upper == 0)
			upper = 1;

#if HAVE_GTK3
		GtkAllocation allocation;

		gtk_widget_get_allocation (GTK_WIDGET (buf->xtext), &allocation);
		page_size = allocation.height / buf->xtext->fontsize;
#else
		page_size = GTK_WIDGET (buf->xtext)->allocation.height /
							  buf->xtext->fontsize;
#endif

		if (value > upper - page_size)
		{
			buf->scrollbar_down = TRUE;
			value = upper - page_size;
		}

		if (value < 0)
			value = 0;

		xtext_adjustment_apply (adj, lower, upper, value, page_size);
		xtext_adj_set_page_increment (adj, page_size);

		if (fire_signal)
		{
#if HAVE_GTK3
			gtk_adjustment_value_changed (adj);
#else
			gtk_adjustment_changed (adj);
#endif
		}
	}
}

static gint
gtk_xtext_adjustment_timeout (GtkXText * xtext)
{
	gtk_xtext_render_page (xtext);
	xtext->io_tag = 0;
	return 0;
}

static void
gtk_xtext_adjustment_changed (GtkAdjustment * adj, GtkXText * xtext)
{
	gdouble value = xtext_adj_get_value (xtext->adj);
	gdouble upper = xtext_adj_get_upper (xtext->adj);
	gdouble page_size = xtext_adj_get_page_size (xtext->adj);

	if (!gtk_widget_get_realized (GTK_WIDGET (xtext)))
		return;

	if (xtext->buffer->old_value != value)
	{
		if (value >= upper - page_size)
			xtext->buffer->scrollbar_down = TRUE;
		else
			xtext->buffer->scrollbar_down = FALSE;

		if (value + 1 == xtext->buffer->old_value ||
			 value - 1 == xtext->buffer->old_value)	/* clicked an arrow? */
		{
			if (xtext->io_tag)
			{
				g_source_remove (xtext->io_tag);
				xtext->io_tag = 0;
			}
			gtk_xtext_render_page (xtext);
		} else
		{
			if (!xtext->io_tag)
				xtext->io_tag = g_timeout_add (REFRESH_TIMEOUT,
															(GSourceFunc)
															gtk_xtext_adjustment_timeout,
															xtext);
		}
	}
	xtext->buffer->old_value = xtext_adj_get_value (adj);
}

GtkWidget *
gtk_xtext_new (const XTextColor *palette, int separator)
{
	GtkXText *xtext;

	xtext = g_object_new (gtk_xtext_get_type (), NULL);
	xtext->separator = separator;
	xtext->wordwrap = TRUE;
	xtext->buffer = gtk_xtext_buffer_new (xtext);
	xtext->orig_buffer = xtext->buffer;

#if !HAVE_GTK3
	gtk_widget_set_double_buffered (GTK_WIDGET (xtext), FALSE);
#else
	/* GTK3 already uses the GTK render pipeline; no manual double-buffering toggle. */
#endif
	gtk_xtext_set_palette (xtext, palette);

	return GTK_WIDGET (xtext);
}

static void
gtk_xtext_cleanup (GtkXText *xtext)
{
	if (xtext->add_io_tag)
	{
		g_source_remove (xtext->add_io_tag);
		xtext->add_io_tag = 0;
	}

	if (xtext->scroll_tag)
	{
		g_source_remove (xtext->scroll_tag);
		xtext->scroll_tag = 0;
	}

	if (xtext->io_tag)
	{
		g_source_remove (xtext->io_tag);
		xtext->io_tag = 0;
	}

	if (xtext->background_surface)
	{
		cairo_surface_destroy (xtext->background_surface);
		xtext->background_surface = NULL;
	}

	if (xtext->font)
	{
		backend_font_close (xtext);
		xtext->font = NULL;
	}

	if (xtext->adj)
	{
		g_signal_handlers_disconnect_matched (G_OBJECT (xtext->adj),
					G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, xtext);
	/*	gtk_signal_disconnect_by_data (G_OBJECT (xtext->adj), xtext);*/
		g_object_unref (G_OBJECT (xtext->adj));
		xtext->adj = NULL;
	}


	if (xtext->hand_cursor)
	{
		gtk_xtext_cursor_unref (xtext->hand_cursor);
		xtext->hand_cursor = NULL;
	}

	if (xtext->resize_cursor)
	{
		gtk_xtext_cursor_unref (xtext->resize_cursor);
		xtext->resize_cursor = NULL;
	}

	if (xtext->orig_buffer)
	{
		gtk_xtext_buffer_free (xtext->orig_buffer);
		xtext->orig_buffer = NULL;
	}
}

#if !HAVE_GTK3
static void
gtk_xtext_destroy (GtkObject * object)
{
	GtkXText *xtext = GTK_XTEXT (object);

	gtk_xtext_cleanup (xtext);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}
#endif

#if HAVE_GTK3
static void
gtk_xtext_dispose (GObject *object)
{
	GtkXText *xtext = GTK_XTEXT (object);

	gtk_xtext_cleanup (xtext);

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(*G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
gtk_xtext_finalize (GObject *object)
{
	if (G_OBJECT_CLASS (parent_class)->finalize)
		(*G_OBJECT_CLASS (parent_class)->finalize) (object);
}
#endif

static void
gtk_xtext_unrealize (GtkWidget * widget)
{
#if HAVE_GTK3
	GdkWindow *window = gtk_widget_get_window (widget);
#endif
	backend_deinit (GTK_XTEXT (widget));

	/* if there are still events in the queue, this'll avoid segfault */
#if HAVE_GTK3
	if (window)
		gdk_window_set_user_data (window, NULL);
#endif
#if !HAVE_GTK3
	gdk_window_set_user_data (widget->window, NULL);
#endif

	if (parent_class->unrealize)
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);

#if HAVE_GTK3
	gtk_widget_set_window (widget, NULL);
	gtk_widget_set_realized (widget, FALSE);
#endif
}

static void
gtk_xtext_get_pointer (GdkWindow *window, gint *x, gint *y, GdkModifierType *mask)
{
#if !HAVE_GTK3
	gdk_window_get_pointer (window, x, y, mask);
#else
	GdkDisplay *display = gdk_window_get_display (window);
	GdkSeat *seat = gdk_display_get_default_seat (display);
	GdkDevice *device = gdk_seat_get_pointer (seat);
	gint root_x = 0;
	gint root_y = 0;
	gint win_x = 0;
	gint win_y = 0;

	if (!device)
	{
		if (x)
			*x = 0;
		if (y)
			*y = 0;
		if (mask)
			*mask = 0;
		return;
	}

	gdk_device_get_position (device, NULL, &root_x, &root_y);
	gdk_window_get_origin (window, &win_x, &win_y);

	if (x)
		*x = root_x - win_x;
	if (y)
		*y = root_y - win_y;
	if (mask)
		gdk_device_get_state (device, window, NULL, mask);
#endif
}

static inline void
gtk_xtext_clear_background (GtkWidget *widget)
{
#if !HAVE_GTK3
	gdk_window_set_back_pixmap (widget->window, NULL, FALSE);
#else
	GdkWindow *window = gtk_widget_get_window (widget);
	if (window)
		gdk_window_set_background_pattern (window, NULL);
#endif
}

static void
gtk_xtext_realize (GtkWidget * widget)
{
	GtkXText *xtext;
	GdkWindowAttr attributes;
	GdkWindow *window;
	GtkAllocation allocation;
	GdkWindow *parent_window;
	gint attributes_mask;

	xtext = GTK_XTEXT (widget);

	gtk_widget_set_realized (widget, TRUE);
#if HAVE_GTK3
	gtk_widget_get_allocation (widget, &allocation);
	parent_window = gtk_widget_get_parent_window (widget);
	attributes.visual = gtk_widget_get_visual (widget);
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
#endif
#if !HAVE_GTK3
	allocation = widget->allocation;
	parent_window = widget->parent->window;
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.visual = gtk_widget_get_visual (widget);
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
#endif

	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.event_mask = gtk_widget_get_events (widget) |
		GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
		| GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK;

	window = gdk_window_new (parent_window, &attributes, attributes_mask);

#if HAVE_GTK3
	gtk_widget_set_window (widget, window);
#else
	widget->window = window;
#endif

	gdk_window_set_user_data (window, widget);

	xtext->depth = gdk_window_get_visual (window)->depth;

	/* for the separator bar (light) */
	xtext->light_gc.red = 1.0;
	xtext->light_gc.green = 1.0;
	xtext->light_gc.blue = 1.0;
	xtext->light_gc.alpha = 1.0;

	/* for the separator bar (dark) */
	xtext->dark_gc.red = 0x1111 / 65535.0;
	xtext->dark_gc.green = 0x1111 / 65535.0;
	xtext->dark_gc.blue = 0x1111 / 65535.0;
	xtext->dark_gc.alpha = 1.0;

	/* for the separator bar (thinline) */
	xtext->thin_gc.red = 0x8e38 / 65535.0;
	xtext->thin_gc.green = 0x8e38 / 65535.0;
	xtext->thin_gc.blue = 0x9f38 / 65535.0;
	xtext->thin_gc.alpha = 1.0;

	/* for the marker bar (marker) */
	xtext->marker_gc = xtext->palette[XTEXT_MARKER];

	xtext_set_fg (xtext, XTEXT_FG);
	xtext_set_bg (xtext, XTEXT_BG);

	/* draw directly to window */
	xtext->draw_window = window;

	if (xtext->background_surface)
	{
		xtext->ts_x = xtext->ts_y = 0;
	}

	xtext->hand_cursor = gdk_cursor_new_for_display (gdk_window_get_display (window), GDK_HAND1);
	xtext->resize_cursor = gdk_cursor_new_for_display (gdk_window_get_display (window), GDK_LEFT_SIDE);

	gtk_xtext_clear_background (widget);

	backend_init (xtext);
}

static void
gtk_xtext_size_request (GtkWidget * widget, GtkRequisition * requisition)
{
	requisition->width = 200;
	requisition->height = 90;
}

static void
gtk_xtext_size_allocate (GtkWidget * widget, GtkAllocation * allocation)
{
	GtkXText *xtext = GTK_XTEXT (widget);
	int height_only = FALSE;
	GdkWindow *window;

	if (allocation->width == xtext->buffer->window_width)
		height_only = TRUE;

#if HAVE_GTK3
	gtk_widget_set_allocation (widget, allocation);
#endif
#if !HAVE_GTK3
	widget->allocation = *allocation;
#endif
	if (gtk_widget_get_realized (GTK_WIDGET(widget)))
	{
		xtext->buffer->window_width = allocation->width;
		xtext->buffer->window_height = allocation->height;

#if HAVE_GTK3
		window = gtk_widget_get_window (widget);
#endif
#if !HAVE_GTK3
		window = widget->window;
#endif
		if (window)
			gdk_window_move_resize (window, allocation->x, allocation->y,
									allocation->width, allocation->height);
		dontscroll (xtext->buffer);	/* force scrolling off */
		if (!height_only)
			gtk_xtext_calc_lines (xtext->buffer, FALSE);
		else
		{
			xtext->buffer->pagetop_ent = NULL;
			gtk_xtext_adjustment_set (xtext->buffer, FALSE);
		}
		if (xtext->buffer->scrollbar_down)
			xtext_adj_set_value (xtext->adj,
				xtext_adj_get_upper (xtext->adj) -
				xtext_adj_get_page_size (xtext->adj));
	}
}

static int
gtk_xtext_selection_clear (xtext_buffer *buf)
{
	textentry *ent;
	int ret = 0;

	ent = buf->last_ent_start;
	while (ent)
	{
		if (ent->mark_start != -1)
			ret = 1;
		ent->mark_start = -1;
		ent->mark_end = -1;
		if (ent == buf->last_ent_end)
			break;
		ent = ent->next;
	}

	return ret;
}

static int
find_x (GtkXText *xtext, textentry *ent, int x, int subline, int indent)
{
	int xx = indent;
	int suboff;
	GSList *list;
	GSList *hid = NULL;
	offlen_t *meta;
	int off, len, wid, mbl, mbw;

	/* Skip to the first chunk of stuff for the subline */
	if (subline > 0)
	{
		suboff = GPOINTER_TO_INT (g_slist_nth_data (ent->sublines, subline - 1));
		for (list = ent->slp; list; list = g_slist_next (list))
		{
			meta = list->data;
			if (meta->off + meta->len > suboff)
				break;
		}
	}
	else
	{
		suboff = 0;
		list = ent->slp;
	} 
	/* Step to the first character of the subline */
	if (list == NULL)
		return 0;
	meta = list->data;
	off = meta->off;
	len = meta->len;
	if (meta->emph & EMPH_HIDDEN)
		hid = list;
	while (len > 0)
	{
		if (off >= suboff)
			break;
		mbl = charlen (ent->str + off);
		len -= mbl;
		off += mbl;
	}
	if (len < 0)
		return ent->str_len;		/* Bad char -- return max offset. */

	/* Step through characters to find the one at the x position */
	wid = x - indent;
	len = meta->len - (off - meta->off);
	while (wid > 0)
	{
		mbl = charlen (ent->str + off);
		mbw = backend_get_text_width_emph (xtext, ent->str + off, mbl, meta->emph);
		wid -= mbw;
		xx += mbw;
		if (xx >= x)
			return off;
		len -= mbl;
		off += mbl;
		if (len <= 0)
		{
			if (meta->emph & EMPH_HIDDEN)
				hid = list;
			list = g_slist_next (list);
			if (list == NULL)
				return ent->str_len;
			meta = list->data;
			off = meta->off;
			len = meta->len;
		}
	}

	/* If previous chunk exists and is marked hidden, regard it as unhidden */
	if (hid && list && hid->next == list)
	{
		meta = hid->data;
		off = meta->off;
	}

	/* Return offset of character at x within subline */
	return off;
}

static int
gtk_xtext_find_x (GtkXText * xtext, int x, textentry * ent, int subline,
						int line, int *out_of_bounds)
{
	int indent;
	unsigned char *str;

	if (subline < 1)
		indent = ent->indent;
	else
		indent = xtext->buffer->indent;

	if (line > xtext_adj_get_page_size (xtext->adj) || line < 0)
	{
		*out_of_bounds = TRUE;
		return 0;
	}

	str = ent->str + gtk_xtext_find_subline (xtext, ent, subline);
	if (str >= ent->str + ent->str_len)
		return 0;

	/* Let user select left a few pixels to grab hidden text e.g. '<' */
	if (x < indent - xtext->space_width)
	{
		*out_of_bounds = 1;
		return (str - ent->str);
	}

	*out_of_bounds = 0;

	return find_x (xtext, ent, x, subline, indent);
}

static textentry *
gtk_xtext_find_char (GtkXText * xtext, int x, int y, int *off, int *out_of_bounds)
{
	textentry *ent;
	int line;
	int subline;
	int outofbounds = FALSE;

	/* Adjust y value for negative rounding, double to int */
	if (y < 0)
		y -= xtext->fontsize;

	line = (y + xtext->pixel_offset) / xtext->fontsize;
	ent = gtk_xtext_nth (xtext,
		line + (int)xtext_adj_get_value (xtext->adj), &subline);
	if (!ent)
		return NULL;

	if (off)
		*off = gtk_xtext_find_x (xtext, x, ent, subline, line, &outofbounds);
	if (out_of_bounds)
		*out_of_bounds = outofbounds;

	return ent;
}

static void
gtk_xtext_draw_sep (GtkXText * xtext, int y)
{
	int x, height;
	cairo_t *cr;
	GtkAllocation allocation;

	if (y == -1)
	{
		y = 0;
#if HAVE_GTK3
		gtk_widget_get_allocation (GTK_WIDGET (xtext), &allocation);
		height = allocation.height;
#endif
#if !HAVE_GTK3
		height = GTK_WIDGET (xtext)->allocation.height;
#endif
	} else
	{
		height = xtext->fontsize;
	}

	/* draw the separator line */
	if (xtext->separator && xtext->buffer->indent)
	{
		const XTextColor *light = &xtext->light_gc;
		const XTextColor *dark = &xtext->dark_gc;
		cr = xtext_create_context (xtext);

		x = xtext->buffer->indent - ((xtext->space_width + 1) / 2);
		if (x < 1)
		{
			cairo_destroy (cr);
			return;
		}

		if (xtext->thinline)
		{
			if (xtext->moving_separator)
				xtext_draw_line (xtext, cr, light, x, y, x, y + height);
			else
				xtext_draw_line (xtext, cr, &xtext->thin_gc, x, y, x, y + height);
		} else
		{
			if (xtext->moving_separator)
			{
				xtext_draw_line (xtext, cr, light, x - 1, y, x - 1, y + height);
				xtext_draw_line (xtext, cr, dark, x, y, x, y + height);
			} else
			{
				xtext_draw_line (xtext, cr, dark, x - 1, y, x - 1, y + height);
				xtext_draw_line (xtext, cr, light, x, y, x, y + height);
			}
		}

		cairo_destroy (cr);
	}
}

static void
gtk_xtext_draw_marker (GtkXText * xtext, textentry * ent, int y)
{
	int x, width, render_y;
	cairo_t *cr;
	GtkAllocation allocation;

	if (!xtext->marker) return;

	if (xtext->buffer->marker_pos == ent)
	{
		render_y = y + xtext->font->descent;
	}
	else if (xtext->buffer->marker_pos == ent->next && ent->next != NULL)
	{
		render_y = y + xtext->font->descent + xtext->fontsize * g_slist_length (ent->sublines);
	}
	else return;

	x = 0;
#if HAVE_GTK3
	gtk_widget_get_allocation (GTK_WIDGET (xtext), &allocation);
#endif
#if !HAVE_GTK3
	allocation = GTK_WIDGET (xtext)->allocation;
#endif
	width = allocation.width;

	cr = xtext_create_context (xtext);
	xtext_draw_line (xtext, cr, &xtext->marker_gc, x, render_y, x + width, render_y);
	cairo_destroy (cr);

	if (gtk_window_has_toplevel_focus (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (xtext)))))
	{
		xtext->buffer->marker_seen = TRUE;
	}
}

static void
gtk_xtext_paint (GtkWidget *widget, GdkRectangle *area)
{
	GtkXText *xtext = GTK_XTEXT (widget);
	textentry *ent_start, *ent_end;
	int x, y;
	GtkAllocation allocation;

#if HAVE_GTK3
	gtk_widget_get_allocation (widget, &allocation);
#endif
#if !HAVE_GTK3
	allocation = widget->allocation;
#endif

	if (area->x == 0 && area->y == 0 &&
		 area->height == allocation.height &&
		 area->width == allocation.width)
	{
		dontscroll (xtext->buffer);	/* force scrolling off */
		gtk_xtext_render_page (xtext);
		return;
	}

	ent_start = gtk_xtext_find_char (xtext, area->x, area->y, NULL, NULL);
	if (!ent_start)
	{
		xtext_draw_bg (xtext, area->x, area->y, area->width, area->height);
		goto xit;
	}
	ent_end = gtk_xtext_find_char (xtext, area->x + area->width,
											 area->y + area->height, NULL, NULL);
	if (!ent_end)
		ent_end = xtext->buffer->text_last;

	/* can't set a clip here, because fgc/bgc are used to draw the DB too */
/*	backend_set_clip (xtext, area);*/
	xtext->clip_x = area->x;
	xtext->clip_x2 = area->x + area->width;
	xtext->clip_y = area->y;
	xtext->clip_y2 = area->y + area->height;

	/* y is the last pixel y location it rendered text at */
	y = gtk_xtext_render_ents (xtext, ent_start, ent_end);

	if (y && y < allocation.height && !ent_end->next)
	{
		GdkRectangle rect;

		rect.x = 0;
		rect.y = y;
		rect.width = allocation.width;
		rect.height = allocation.height - y;

		/* fill any space below the last line that also intersects with
			the exposure rectangle */
		if (gdk_rectangle_intersect (area, &rect, &rect))
		{
			xtext_draw_bg (xtext, rect.x, rect.y, rect.width, rect.height);
		}
	}

	/*backend_clear_clip (xtext);*/
	xtext->clip_x = 0;
	xtext->clip_x2 = 1000000;
	xtext->clip_y = 0;
	xtext->clip_y2 = 1000000;

xit:
	x = xtext->buffer->indent - ((xtext->space_width + 1) / 2);
	if (area->x <= x)
		gtk_xtext_draw_sep (xtext, -1);
}

static gboolean
gtk_xtext_expose (GtkWidget * widget, GdkEventExpose * event)
{
	gtk_xtext_paint (widget, &event->area);
	return FALSE;
}

/* render a selection that has extended or contracted upward */

static void
gtk_xtext_selection_up (GtkXText *xtext, textentry *start, textentry *end,
								int start_offset)
{
	/* render all the complete lines */
	if (start->next == end)
		gtk_xtext_render_ents (xtext, end, NULL);
	else
		gtk_xtext_render_ents (xtext, start->next, end);

	/* now the incomplete upper line */
	if (start == xtext->buffer->last_ent_start)
		xtext->jump_in_offset = xtext->buffer->last_offset_start;
	else
		xtext->jump_in_offset = start_offset;
	gtk_xtext_render_ents (xtext, start, NULL);
	xtext->jump_in_offset = 0;
}

/* render a selection that has extended or contracted downward */

static void
gtk_xtext_selection_down (GtkXText *xtext, textentry *start, textentry *end,
								  int end_offset)
{
	/* render all the complete lines */
	if (end->prev == start)
		gtk_xtext_render_ents (xtext, start, NULL);
	else
		gtk_xtext_render_ents (xtext, start, end->prev);

	/* now the incomplete bottom line */
	if (end == xtext->buffer->last_ent_end)
		xtext->jump_out_offset = xtext->buffer->last_offset_end;
	else
		xtext->jump_out_offset = end_offset;
	gtk_xtext_render_ents (xtext, end, NULL);
	xtext->jump_out_offset = 0;
}

static void
gtk_xtext_selection_render (GtkXText *xtext, textentry *start_ent, textentry *end_ent)
{
	textentry *ent;
	int start_offset = start_ent->mark_start;
	int end_offset = end_ent->mark_end;
	int start, end;

	xtext->skip_border_fills = TRUE;
	xtext->skip_stamp = TRUE;

	/* force an optimized render if there was no previous selection */
	if (xtext->buffer->last_ent_start == NULL && start_ent == end_ent)
	{
		xtext->buffer->last_offset_start = start_offset;
		xtext->buffer->last_offset_end = end_offset;
		goto lamejump;
	}

	/* mark changed within 1 ent only? */
	if (xtext->buffer->last_ent_start == start_ent &&
		 xtext->buffer->last_ent_end == end_ent)
	{
		/* when only 1 end of the selection is changed, we can really
			save on rendering */
		if (xtext->buffer->last_offset_start == start_offset ||
			 xtext->buffer->last_offset_end == end_offset)
		{
lamejump:
			ent = end_ent;
			/* figure out where to start and end the rendering */
			if (end_offset > xtext->buffer->last_offset_end)
			{
				end = end_offset;
				start = xtext->buffer->last_offset_end;
			} else if (end_offset < xtext->buffer->last_offset_end)
			{
				end = xtext->buffer->last_offset_end;
				start = end_offset;
			} else if (start_offset < xtext->buffer->last_offset_start)
			{
				end = xtext->buffer->last_offset_start;
				start = start_offset;
				ent = start_ent;
			} else if (start_offset > xtext->buffer->last_offset_start)
			{
				end = start_offset;
				start = xtext->buffer->last_offset_start;
				ent = start_ent;
			} else
			{	/* WORD selects end up here */
				end = end_offset;
				start = start_offset;
			}
		} else
		{
			/* LINE selects end up here */
			/* so which ent actually changed? */
			ent = start_ent;
			if (xtext->buffer->last_offset_start == start_offset)
				ent = end_ent;

			end = MAX (xtext->buffer->last_offset_end, end_offset);
			start = MIN (xtext->buffer->last_offset_start, start_offset);
		}

		xtext->jump_out_offset = end;
		xtext->jump_in_offset = start;
		gtk_xtext_render_ents (xtext, ent, NULL);
		xtext->jump_out_offset = 0;
		xtext->jump_in_offset = 0;
	}
	/* marking downward? */
	else if (xtext->buffer->last_ent_start == start_ent &&
				xtext->buffer->last_offset_start == start_offset)
	{
		/* find the range that covers both old and new selection */
		ent = start_ent;
		while (ent)
		{
			if (ent == xtext->buffer->last_ent_end)
			{
				gtk_xtext_selection_down (xtext, ent, end_ent, end_offset);
				/*gtk_xtext_render_ents (xtext, ent, end_ent);*/
				break;
			}
			if (ent == end_ent)
			{
				gtk_xtext_selection_down (xtext, ent, xtext->buffer->last_ent_end, end_offset);
				/*gtk_xtext_render_ents (xtext, ent, xtext->buffer->last_ent_end);*/
				break;
			}
			ent = ent->next;
		}
	}
	/* marking upward? */
	else if (xtext->buffer->last_ent_start != NULL &&
				xtext->buffer->last_ent_end == end_ent &&
				xtext->buffer->last_offset_end == end_offset)
	{
		ent = end_ent;
		while (ent)
		{
			if (ent == start_ent && xtext->buffer->last_ent_start)
			{
				gtk_xtext_selection_up (xtext, xtext->buffer->last_ent_start, ent, start_offset);
				/*gtk_xtext_render_ents (xtext, xtext->buffer->last_ent_start, ent);*/
				break;
			}
			if (ent == xtext->buffer->last_ent_start)
			{
				gtk_xtext_selection_up (xtext, start_ent, ent, start_offset);
				/*gtk_xtext_render_ents (xtext, start_ent, ent);*/
				break;
			}
			ent = ent->prev;
		}
	}
	else	/* cross-over mark (stretched or shrunk at both ends) */
	{
		/* unrender the old mark */
		gtk_xtext_render_ents (xtext, xtext->buffer->last_ent_start, xtext->buffer->last_ent_end);
		/* now render the new mark, but skip overlaps */
		if (start_ent == xtext->buffer->last_ent_start)
		{
			/* if the new mark is a sub-set of the old, do nothing */
			if (start_ent != end_ent)
				gtk_xtext_render_ents (xtext, start_ent->next, end_ent);
		} else if (end_ent == xtext->buffer->last_ent_end)
		{
			/* if the new mark is a sub-set of the old, do nothing */
			if (start_ent != end_ent)
				gtk_xtext_render_ents (xtext, start_ent, end_ent->prev);
		} else
			gtk_xtext_render_ents (xtext, start_ent, end_ent);
	}

	xtext->buffer->last_ent_start = start_ent;
	xtext->buffer->last_ent_end = end_ent;
	xtext->buffer->last_offset_start = start_offset;
	xtext->buffer->last_offset_end = end_offset;

	xtext->skip_border_fills = FALSE;
	xtext->skip_stamp = FALSE;
}

static void
gtk_xtext_selection_draw (GtkXText * xtext, GdkEventMotion * event, gboolean render)
{
	textentry *ent;
	textentry *ent_end;
	textentry *ent_start;
	int offset_start = 0;
	int offset_end = 0;
	textentry *low_ent, *high_ent;
	int low_x, low_y, low_offs;
	int high_x, high_y, high_offs, high_len;

	if (xtext->buffer->text_first == NULL)
		return;

	ent_start = gtk_xtext_find_char (xtext, xtext->select_start_x, xtext->select_start_y, &offset_start, NULL);
	ent_end = gtk_xtext_find_char (xtext, xtext->select_end_x, xtext->select_end_y, &offset_end, NULL);
	if (ent_start == NULL && ent_end == NULL)
		return;

	if	((ent_start != ent_end && xtext->select_start_y > xtext->select_end_y) || /* different entries */
		(ent_start == ent_end && offset_start > offset_end))	/* same entry, different character offsets */
	{
		/* marking up */
		low_ent = ent_end;
		low_x = xtext->select_end_x;
		low_y = xtext->select_end_y;
		low_offs = offset_end;
		high_ent = ent_start;
		high_x = xtext->select_start_x;
		high_y = xtext->select_start_y;
		high_offs = offset_start;
	}
	else
	{
		/* marking down */
		low_ent = ent_start;
		low_x = xtext->select_start_x;
		low_y = xtext->select_start_y;
		low_offs = offset_start;
		high_ent = ent_end;
		high_x = xtext->select_end_x;
		high_y = xtext->select_end_y;
		high_offs = offset_end;
	}
	if (low_ent == NULL)
	{
		low_ent = xtext->buffer->text_first;
		low_offs = 0;
	}
	if (high_ent == NULL)
	{
		high_ent = xtext->buffer->text_last;
		high_offs = high_ent->str_len;
	}

	/* word selection */
	if (xtext->word_select)
	{
		/* a word selection cannot be started if the cursor is out of bounds in gtk_xtext_button_press */
		gtk_xtext_get_word (xtext, low_x, low_y, NULL, &low_offs, NULL, NULL);

		/* in case the cursor is out of bounds we keep offset_end from gtk_xtext_find_char and fix the length */
		if (gtk_xtext_get_word (xtext, high_x, high_y, NULL, &high_offs, &high_len, NULL) == NULL)
			high_len = high_offs == high_ent->str_len? 0: -1; /* -1 for the space, 0 if at the end */
		high_offs += high_len;
		if (low_y < 0)
			low_offs = xtext->buffer->last_offset_start;
		if (high_y > xtext->buffer->window_height)
			high_offs = xtext->buffer->last_offset_end;
	}
	/* line/ent selection */
	else if (xtext->line_select)
	{
		low_offs = 0;
		high_offs = high_ent->str_len;
	}
	/* character selection */
	else
	{
		if (low_y < 0)
			low_offs = xtext->buffer->last_offset_start;
		if (high_y > xtext->buffer->window_height)
			high_offs = xtext->buffer->last_offset_end;
	}

	/* set all the old mark_ fields to -1 */
	gtk_xtext_selection_clear (xtext->buffer);

	low_ent->mark_start = low_offs;
	low_ent->mark_end = high_offs;

	if (low_ent != high_ent)
	{
		low_ent->mark_end = low_ent->str_len;
		if (high_offs != 0)
		{
			high_ent->mark_start = 0;
			high_ent->mark_end = high_offs;
		}

		/* set all the mark_ fields of the ents within the selection */
		ent = low_ent->next;
		while (ent && ent != high_ent)
		{
			ent->mark_start = 0;
			ent->mark_end = ent->str_len;
			ent = ent->next;
		}
	}

	if (render)
		gtk_xtext_selection_render (xtext, low_ent, high_ent);
}

static int
gtk_xtext_timeout_ms (GtkXText *xtext, int pixes)
{
	int apixes = abs(pixes);

	if (apixes < 6) return 100;
	if (apixes < 12) return 50;
	if (apixes < 20) return 20;
	return 10;
}
static gint
gtk_xtext_scrolldown_timeout (GtkXText * xtext)
{
	int p_y, win_height;
	xtext_buffer *buf = xtext->buffer;
	GtkAdjustment *adj = xtext->adj;
	GdkWindow *window;

#if HAVE_GTK3
	window = gtk_widget_get_window (GTK_WIDGET (xtext));
#endif
#if !HAVE_GTK3
	window = GTK_WIDGET (xtext)->window;
#endif
	if (!window)
		return 0;
	gtk_xtext_get_pointer (window, NULL, &p_y, NULL);
	win_height = gdk_window_get_height (window);

	if (buf->last_ent_end == NULL ||	/* If context has changed OR */
		 buf->pagetop_ent == NULL ||	/* pagetop_ent is reset OR */
		 p_y <= win_height ||			/* pointer not below bottom margin OR */
		 xtext_adj_get_value (adj) >=
		 (xtext_adj_get_upper (adj) - xtext_adj_get_page_size (adj)))
	{
		xtext->scroll_tag = 0;
		return 0;
	}

	xtext->select_start_y -= xtext->fontsize;
	xtext->select_start_adj++;
	xtext_adj_set_value (adj, xtext_adj_get_value (adj) + 1);
	gtk_adjustment_value_changed (adj);
	gtk_xtext_selection_draw (xtext, NULL, TRUE);
	gtk_xtext_render_ents (xtext, buf->pagetop_ent->next, buf->last_ent_end);
	xtext->scroll_tag = g_timeout_add (gtk_xtext_timeout_ms (xtext, p_y - win_height),
													(GSourceFunc)
													gtk_xtext_scrolldown_timeout,
													xtext);

	return 0;
}

static gint
gtk_xtext_scrollup_timeout (GtkXText * xtext)
{
	int p_y;
	xtext_buffer *buf = xtext->buffer;
	GtkAdjustment *adj = xtext->adj;
	int delta_y;
	GdkWindow *window;

#if HAVE_GTK3
	window = gtk_widget_get_window (GTK_WIDGET (xtext));
#endif
#if !HAVE_GTK3
	window = GTK_WIDGET (xtext)->window;
#endif
	if (!window)
		return 0;
	gtk_xtext_get_pointer (window, NULL, &p_y, NULL);

	if (buf->last_ent_start == NULL ||	/* If context has changed OR */
		 buf->pagetop_ent == NULL ||		/* pagetop_ent is reset OR */
		 p_y >= 0 ||							/* not above top margin OR */
		 xtext_adj_get_value (adj) == 0)	/* we're scrolled to the top */
	{
		xtext->scroll_tag = 0;
		return 0;
	}

	if (xtext_adj_get_value (adj) < 0)
	{
		delta_y = xtext_adj_get_value (adj) * xtext->fontsize;
		xtext_adj_set_value (adj, 0);
	} else {
		delta_y = xtext->fontsize;
		xtext_adj_set_value (adj, xtext_adj_get_value (adj) - 1);
	}
	xtext->select_start_y += delta_y;
	xtext->select_start_adj = xtext_adj_get_value (adj);
	gtk_adjustment_value_changed (adj);
	gtk_xtext_selection_draw (xtext, NULL, TRUE);
	gtk_xtext_render_ents (xtext, buf->pagetop_ent->prev, buf->last_ent_end);
	xtext->scroll_tag = g_timeout_add (gtk_xtext_timeout_ms (xtext, p_y),
													(GSourceFunc)
													gtk_xtext_scrollup_timeout,
													xtext);

	return 0;
}

static void
gtk_xtext_selection_update (GtkXText * xtext, GdkEventMotion * event, int p_y, gboolean render)
{
	int win_height;
	int moved;
	GdkWindow *window;

	if (xtext->scroll_tag)
	{
		return;
	}

#if HAVE_GTK3
	window = gtk_widget_get_window (GTK_WIDGET (xtext));
#endif
#if !HAVE_GTK3
	window = GTK_WIDGET (xtext)->window;
#endif
	if (!window)
		return;
	win_height = gdk_window_get_height (window);

	/* selecting past top of window, scroll up! */
	if (p_y < 0 && xtext_adj_get_value (xtext->adj) >= 0)
	{
		gtk_xtext_scrollup_timeout (xtext);
	}

	/* selecting past bottom of window, scroll down! */
	else if (p_y > win_height &&
		 xtext_adj_get_value (xtext->adj) <
		 (xtext_adj_get_upper (xtext->adj) -
		  xtext_adj_get_page_size (xtext->adj)))
	{
		gtk_xtext_scrolldown_timeout (xtext);
	}
	else
	{
		moved = (int)xtext_adj_get_value (xtext->adj) -
			xtext->select_start_adj;
		xtext->select_start_y -= (moved * xtext->fontsize);
		xtext->select_start_adj = xtext_adj_get_value (xtext->adj);
		gtk_xtext_selection_draw (xtext, event, render);
	}
}

static char *
gtk_xtext_get_word (GtkXText * xtext, int x, int y, textentry ** ret_ent,
						  int *ret_off, int *ret_len, GSList **slp)
{
	textentry *ent;
	int offset;
	unsigned char *word;
	unsigned char *last, *end;
	int len;
	int out_of_bounds = 0;
	int len_to_offset = 0;

	ent = gtk_xtext_find_char (xtext, x, y, &offset, &out_of_bounds);
	if (ent == NULL || out_of_bounds || offset < 0 || offset >= ent->str_len)
		return NULL;

	word = ent->str + offset;
	while ((word = g_utf8_find_prev_char (ent->str, word)))
	{
		if (is_del (*word))
		{
			word++;
			len_to_offset--;
			break;
		}
		len_to_offset += charlen (word);
	}
	if (!word)
		word = ent->str;

	/* remove color characters from the length */
	gtk_xtext_strip_color (word, len_to_offset, xtext->scratch_buffer, &len_to_offset, NULL, FALSE);

	last = word;
	end = ent->str + ent->str_len;
	len = 0;
	do
	{
		if (is_del (*last))
			break;
		len += charlen (last);
		last = g_utf8_find_next_char (last, end);
	}
	while (last);

	if (len > 0 && word[len-1]=='.')
		len--;

	if (ret_ent)
		*ret_ent = ent;
	if (ret_off)
		*ret_off = word - ent->str;
	if (ret_len)
		*ret_len = len;		/* Length before stripping */

	word = gtk_xtext_strip_color (word, len, xtext->scratch_buffer, NULL, slp, FALSE);

	/* avoid turning the cursor into a hand for non-url part of the word */
	if (xtext->urlcheck_function && xtext->urlcheck_function (GTK_WIDGET (xtext), word))
	{
		int start, end;
		url_last (&start, &end);

		/* make sure we're not before the start of the match */
		if (len_to_offset < start)
			return NULL;

		/* and not after it */
		if (len_to_offset - start >= end - start)
			return NULL;
	}

	return word;
}

static void
gtk_xtext_unrender_hilight (GtkXText *xtext)
{
	xtext->render_hilights_only = TRUE;
	xtext->skip_border_fills = TRUE;
	xtext->skip_stamp = TRUE;
	xtext->un_hilight = TRUE;

	gtk_xtext_render_ents (xtext, xtext->hilight_ent, NULL);

	xtext->render_hilights_only = FALSE;
	xtext->skip_border_fills = FALSE;
	xtext->skip_stamp = FALSE;
	xtext->un_hilight = FALSE;
}

static gboolean
gtk_xtext_leave_notify (GtkWidget * widget, GdkEventCrossing * event)
{
	GtkXText *xtext = GTK_XTEXT (widget);
#if HAVE_GTK3
	GdkWindow *window = gtk_widget_get_window (widget);
#endif

	if (xtext->cursor_hand)
	{
		gtk_xtext_unrender_hilight (xtext);
		xtext->hilight_start = -1;
		xtext->hilight_end = -1;
		xtext->cursor_hand = FALSE;
#if HAVE_GTK3
		if (window)
			gdk_window_set_cursor (window, 0);
#endif
#if !HAVE_GTK3
		gdk_window_set_cursor (widget->window, 0);
#endif
		xtext->hilight_ent = NULL;
	}

	if (xtext->cursor_resize)
	{
		gtk_xtext_unrender_hilight (xtext);
		xtext->hilight_start = -1;
		xtext->hilight_end = -1;
		xtext->cursor_resize = FALSE;
#if HAVE_GTK3
		if (window)
			gdk_window_set_cursor (window, 0);
#endif
#if !HAVE_GTK3
		gdk_window_set_cursor (widget->window, 0);
#endif
		xtext->hilight_ent = NULL;
	}

	return FALSE;
}

/* check if we should mark time stamps, and if a redraw is needed */

static gboolean
gtk_xtext_check_mark_stamp (GtkXText *xtext, GdkModifierType mask)
{
	gboolean redraw = FALSE;

	if ((mask & STATE_SHIFT || prefs.hex_text_autocopy_stamp)
	    && (!prefs.hex_stamp_text || prefs.hex_text_indent))
	{
		if (!xtext->mark_stamp)
		{
			redraw = TRUE;	/* must redraw all */
			xtext->mark_stamp = TRUE;
		}
	} else
	{
		if (xtext->mark_stamp)
		{
			redraw = TRUE;	/* must redraw all */
			xtext->mark_stamp = FALSE;
		}
	}
	return redraw;
}

static int
gtk_xtext_get_word_adjust (GtkXText *xtext, int x, int y, textentry **word_ent, int *offset, int *len)
{
	GSList *slp = NULL;
	unsigned char *word;
	int word_type = 0;

	word = gtk_xtext_get_word (xtext, x, y, word_ent, offset, len, &slp);
	if (word)
	{
		int laststart, lastend;

		word_type = xtext->urlcheck_function (GTK_WIDGET (xtext), word);
		if (word_type > 0)
		{
			if (url_last (&laststart, &lastend))
			{
				int cumlen, startadj = 0, endadj = 0;
				offlen_t *meta;
				GSList *sl;

				for (sl = slp, cumlen = 0; sl; sl = g_slist_next (sl))
				{
					meta = sl->data;
					startadj = meta->off - cumlen;
					cumlen += meta->len;
					if (laststart < cumlen)
						break;
				}
				for (sl = slp, cumlen = 0; sl; sl = g_slist_next (sl))
				{
					meta = sl->data;
					endadj = meta->off - cumlen;
					cumlen += meta->len;
					if (lastend < cumlen)
						break;
				}
				laststart += startadj;
				*offset += laststart;
				*len = lastend + endadj - laststart;
			}
		}
	}
	g_slist_free_full (slp, g_free);

	return word_type;
}

static gboolean
gtk_xtext_motion_notify (GtkWidget * widget, GdkEventMotion * event)
{
	GtkXText *xtext = GTK_XTEXT (widget);
	GdkModifierType mask;
	int redraw, tmp, x, y, offset, len, line_x;
	textentry *word_ent;
	int word_type;
	GdkWindow *window;
	GtkAllocation allocation;

#if HAVE_GTK3
	window = gtk_widget_get_window (widget);
	gtk_widget_get_allocation (widget, &allocation);
#endif
#if !HAVE_GTK3
	window = widget->window;
	allocation = widget->allocation;
#endif
	if (!window)
		return FALSE;

	gtk_xtext_get_pointer (window, &x, &y, &mask);

	if (xtext->moving_separator)
	{
		if (x < (3 * allocation.width) / 5 && x > 15)
		{
			tmp = xtext->buffer->indent;
			xtext->buffer->indent = x;
			gtk_xtext_fix_indent (xtext->buffer);
			if (tmp != xtext->buffer->indent)
			{
				gtk_xtext_recalc_widths (xtext->buffer, FALSE);
				if (xtext->buffer->scrollbar_down)
					xtext_adj_set_value (xtext->adj,
						xtext_adj_get_upper (xtext->adj) -
						xtext_adj_get_page_size (xtext->adj));
				if (!xtext->io_tag)
					xtext->io_tag = g_timeout_add (REFRESH_TIMEOUT,
																(GSourceFunc)
																gtk_xtext_adjustment_timeout,
																xtext);
			}
		}
		return FALSE;
	}

	if (xtext->button_down)
	{
		redraw = gtk_xtext_check_mark_stamp (xtext, mask);
		gtk_grab_add (widget);
		/*gdk_pointer_grab (widget->window, TRUE,
									GDK_BUTTON_RELEASE_MASK |
									GDK_BUTTON_MOTION_MASK, NULL, NULL, 0);*/
		xtext->select_end_x = x;
		xtext->select_end_y = y;
		gtk_xtext_selection_update (xtext, event, y, !redraw);

		/* user has pressed or released SHIFT, must redraw entire selection */
		if (redraw)
		{
			xtext->force_stamp = TRUE;
			gtk_xtext_render_ents (xtext, xtext->buffer->last_ent_start,
										  xtext->buffer->last_ent_end);
			xtext->force_stamp = FALSE;
		}
		return FALSE;
	}

	if (xtext->separator && xtext->buffer->indent)
	{
		line_x = xtext->buffer->indent - ((xtext->space_width + 1) / 2);
		if (line_x == x || line_x == x + 1 || line_x == x - 1)
		{
			if (!xtext->cursor_resize)
			{
				gdk_window_set_cursor (window, xtext->resize_cursor);
				xtext->cursor_hand = FALSE;
				xtext->cursor_resize = TRUE;
			}
			return FALSE;
		}
	}

	if (xtext->urlcheck_function == NULL)
		return FALSE;

	word_type = gtk_xtext_get_word_adjust (xtext, x, y, &word_ent, &offset, &len);
	if (word_type > 0)
	{
		if (!xtext->cursor_hand ||
			 xtext->hilight_ent != word_ent ||
			 xtext->hilight_start != offset ||
			 xtext->hilight_end != offset + len)
		{
			if (!xtext->cursor_hand)
			{
				gdk_window_set_cursor (window, xtext->hand_cursor);
				xtext->cursor_hand = TRUE;
				xtext->cursor_resize = FALSE;
			}

			/* un-render the old hilight */
			if (xtext->hilight_ent)
				gtk_xtext_unrender_hilight (xtext);

			xtext->hilight_ent = word_ent;
			xtext->hilight_start = offset;
			xtext->hilight_end = offset + len;

			xtext->skip_border_fills = TRUE;
			xtext->render_hilights_only = TRUE;
			xtext->skip_stamp = TRUE;

			gtk_xtext_render_ents (xtext, word_ent, NULL);

			xtext->skip_border_fills = FALSE;
			xtext->render_hilights_only = FALSE;
			xtext->skip_stamp = FALSE;
		}
		return FALSE;
	}

	gtk_xtext_leave_notify (widget, NULL);

	return FALSE;
}

static void
gtk_xtext_set_clip_owner (GtkWidget * xtext, GdkEventButton * event)
{
	char *str;
	int len;

	if (GTK_XTEXT (xtext)->selection_buffer &&
		GTK_XTEXT (xtext)->selection_buffer != GTK_XTEXT (xtext)->buffer)
		gtk_xtext_selection_clear (GTK_XTEXT (xtext)->selection_buffer);

	GTK_XTEXT (xtext)->selection_buffer = GTK_XTEXT (xtext)->buffer;

	str = gtk_xtext_selection_get_text (GTK_XTEXT (xtext), &len);
	if (str)
	{
		if (str[0])
		{
			gtk_clipboard_set_text (gtk_widget_get_clipboard (xtext, GDK_SELECTION_CLIPBOARD), str, len);
			
			gtk_selection_owner_set (xtext, GDK_SELECTION_PRIMARY, event ? event->time : GDK_CURRENT_TIME);
			gtk_selection_owner_set (xtext, GDK_SELECTION_SECONDARY, event ? event->time : GDK_CURRENT_TIME);
		}

		g_free (str);
	}
}

void
gtk_xtext_copy_selection (GtkXText *xtext)
{
	gtk_xtext_set_clip_owner (GTK_WIDGET (xtext), NULL);
}

static void
gtk_xtext_unselect (GtkXText *xtext)
{
	xtext_buffer *buf = xtext->buffer;

	xtext->skip_border_fills = TRUE;
	xtext->skip_stamp = TRUE;

	xtext->jump_in_offset = buf->last_ent_start->mark_start;
	/* just a single ent was marked? */
	if (buf->last_ent_start == buf->last_ent_end)
	{
		xtext->jump_out_offset = buf->last_ent_start->mark_end;
		buf->last_ent_end = NULL;
	}

	gtk_xtext_selection_clear (xtext->buffer);

	/* FIXME: use jump_out on multi-line selects too! */
	xtext->jump_in_offset = 0;
	xtext->jump_out_offset = 0;
	gtk_xtext_render_ents (xtext, buf->last_ent_start, buf->last_ent_end);

	xtext->skip_border_fills = FALSE;
	xtext->skip_stamp = FALSE;

	xtext->buffer->last_ent_start = NULL;
	xtext->buffer->last_ent_end = NULL;
}

static gboolean
gtk_xtext_button_release (GtkWidget * widget, GdkEventButton * event)
{
	GtkXText *xtext = GTK_XTEXT (widget);
	unsigned char *word;
	int old;
	GtkAllocation allocation;

#if HAVE_GTK3
	gtk_widget_get_allocation (widget, &allocation);
#endif
#if !HAVE_GTK3
	allocation = widget->allocation;
#endif

	if (xtext->moving_separator)
	{
		xtext->moving_separator = FALSE;
		old = xtext->buffer->indent;
		if (event->x < (4 * allocation.width) / 5 && event->x > 15)
			xtext->buffer->indent = event->x;
		gtk_xtext_fix_indent (xtext->buffer);
		if (xtext->buffer->indent != old)
		{
			gtk_xtext_recalc_widths (xtext->buffer, FALSE);
			gtk_xtext_adjustment_set (xtext->buffer, TRUE);
			gtk_xtext_render_page (xtext);
		} else
			gtk_xtext_draw_sep (xtext, -1);
		return FALSE;
	}

	if (event->button == 1)
	{
		xtext->button_down = FALSE;
		if (xtext->scroll_tag)
		{
			g_source_remove (xtext->scroll_tag);
			xtext->scroll_tag = 0;
		}

		gtk_grab_remove (widget);
		/*gdk_pointer_ungrab (0);*/

		/* got a new selection? */
		if (xtext->buffer->last_ent_start)
		{
			xtext->color_paste = FALSE;
			if (event->state & STATE_CTRL || prefs.hex_text_autocopy_color)
				xtext->color_paste = TRUE;
			if (prefs.hex_text_autocopy_text)
			{
				gtk_xtext_set_clip_owner (GTK_WIDGET (xtext), event);
			}
		}

		if (xtext->word_select || xtext->line_select)
		{
			xtext->word_select = FALSE;
			xtext->line_select = FALSE;
			return FALSE;
		}

		if (xtext->select_start_x == event->x &&
			 xtext->select_start_y == event->y &&
			 xtext->buffer->last_ent_start)
		{
			gtk_xtext_unselect (xtext);
			xtext->mark_stamp = FALSE;
			return FALSE;
		}

		if (!gtk_xtext_is_selecting (xtext))
		{
			word = gtk_xtext_get_word (xtext, event->x, event->y, 0, 0, 0, 0);
			g_signal_emit (G_OBJECT (xtext), xtext_signals[WORD_CLICK], 0, word ? word : NULL, event);
		}
	}

	return FALSE;
}

static gboolean
gtk_xtext_button_press (GtkWidget * widget, GdkEventButton * event)
{
	GtkXText *xtext = GTK_XTEXT (widget);
	GdkModifierType mask;
	textentry *ent;
	unsigned char *word;
	int line_x, x, y, offset, len;
	GdkWindow *window;

#if HAVE_GTK3
	window = gtk_widget_get_window (widget);
#endif
#if !HAVE_GTK3
	window = widget->window;
#endif
	if (!window)
		return FALSE;

	gtk_xtext_get_pointer (window, &x, &y, &mask);

	if (event->button == 3 || event->button == 2) /* right/middle click */
	{
		word = gtk_xtext_get_word (xtext, x, y, 0, 0, 0, 0);
		if (word)
		{
			g_signal_emit (G_OBJECT (xtext), xtext_signals[WORD_CLICK], 0,
								word, event);
		} else
			g_signal_emit (G_OBJECT (xtext), xtext_signals[WORD_CLICK], 0,
								"", event);
		return FALSE;
	}

	if (event->button != 1)		  /* we only want left button */
		return FALSE;

	if (event->type == GDK_2BUTTON_PRESS)	/* WORD select */
	{
		gtk_xtext_check_mark_stamp (xtext, mask);
		if (gtk_xtext_get_word (xtext, x, y, &ent, &offset, &len, 0))
		{
			if (len == 0)
				return FALSE;
			gtk_xtext_selection_clear (xtext->buffer);
			ent->mark_start = offset;
			ent->mark_end = offset + len;
			gtk_xtext_selection_render (xtext, ent, ent);
			xtext->word_select = TRUE;
		}

		return FALSE;
	}

	if (event->type == GDK_3BUTTON_PRESS)	/* LINE select */
	{
		gtk_xtext_check_mark_stamp (xtext, mask);
		if (gtk_xtext_get_word (xtext, x, y, &ent, 0, 0, 0))
		{
			gtk_xtext_selection_clear (xtext->buffer);
			ent->mark_start = 0;
			ent->mark_end = ent->str_len;
			gtk_xtext_selection_render (xtext, ent, ent);
			xtext->line_select = TRUE;
		}

		return FALSE;
	}

	/* check if it was a separator-bar click */
	if (xtext->separator && xtext->buffer->indent)
	{
		line_x = xtext->buffer->indent - ((xtext->space_width + 1) / 2);
		if (line_x == x || line_x == x + 1 || line_x == x - 1)
		{
			xtext->moving_separator = TRUE;
			/* draw the separator line */
			gtk_xtext_draw_sep (xtext, -1);
			return FALSE;
		}
	}

	xtext->button_down = TRUE;
	xtext->select_start_x = x;
	xtext->select_start_y = y;
	xtext->select_start_adj = xtext_adj_get_value (xtext->adj);

	return FALSE;
}

/* another program has claimed the selection */

static gboolean
gtk_xtext_selection_kill (GtkXText *xtext, GdkEventSelection *event)
{
#ifndef WIN32
	if (xtext->buffer->last_ent_start)
		gtk_xtext_unselect (xtext);
#endif
	return TRUE;
}

static gboolean
gtk_xtext_is_selecting (GtkXText *xtext)
{
	textentry *ent;
	xtext_buffer *buf;

	buf = xtext->selection_buffer;
	if (!buf)
		return FALSE;

	for (ent = buf->last_ent_start; ent; ent = ent->next)
	{
		if (ent->mark_start != -1 && ent->mark_end - ent->mark_start > 0)
			return TRUE;

		if (ent == buf->last_ent_end)
			break;
	}

	return FALSE;
}

static char *
gtk_xtext_selection_get_text (GtkXText *xtext, int *len_ret)
{
	textentry *ent;
	char *txt;
	char *pos;
	char *stripped;
	int len;
	int first = TRUE;
	xtext_buffer *buf;

	buf = xtext->selection_buffer;
	if (!buf)
		return NULL;

	/* first find out how much we need to malloc ... */
	len = 0;
	ent = buf->last_ent_start;
	while (ent)
	{
		if (ent->mark_start != -1)
		{
			/* include timestamp? */
			if (ent->mark_start == 0 && xtext->mark_stamp)
			{
				char *time_str;
				int stamp_size = xtext_get_stamp_str (ent->stamp, &time_str);
				g_free (time_str);
				len += stamp_size;
			}

			if (ent->mark_end - ent->mark_start > 0)
				len += (ent->mark_end - ent->mark_start) + 1;
			else
				len++;
		}
		if (ent == buf->last_ent_end)
			break;
		ent = ent->next;
	}

	if (len < 1)
		return NULL;

	/* now allocate mem and copy buffer */
	pos = txt = g_malloc (len);
	ent = buf->last_ent_start;
	while (ent)
	{
		if (ent->mark_start != -1)
		{
			if (!first)
			{
				*pos = '\n';
				pos++;
			}
			first = FALSE;
			if (ent->mark_end - ent->mark_start > 0)
			{
				/* include timestamp? */
				if (ent->mark_start == 0 && xtext->mark_stamp)
				{
					char *time_str;
					int stamp_size = xtext_get_stamp_str (ent->stamp, &time_str);
					memcpy (pos, time_str, stamp_size);
					g_free (time_str);
					pos += stamp_size;
				}

				memcpy (pos, ent->str + ent->mark_start,
						  ent->mark_end - ent->mark_start);
				pos += ent->mark_end - ent->mark_start;
			}
		}
		if (ent == buf->last_ent_end)
			break;
		ent = ent->next;
	}
	*pos = 0;

	if (xtext->color_paste)
	{
		/*stripped = gtk_xtext_conv_color (txt, strlen (txt), &len);*/
		stripped = txt;
		len = strlen (txt);
	}
	else
	{
		stripped = gtk_xtext_strip_color (txt, strlen (txt), NULL, &len, NULL, FALSE);
		g_free (txt);
	}

	*len_ret = len;
	return stripped;
}

/* another program is asking for our selection */

static void
gtk_xtext_selection_get (GtkWidget * widget,
								 GtkSelectionData * selection_data_ptr,
								 guint info, guint time)
{
	GtkXText *xtext = GTK_XTEXT (widget);
	char *stripped;
	guchar *new_text;
	int len;
	gsize glen;

	stripped = gtk_xtext_selection_get_text (xtext, &len);
	if (!stripped)
		return;

	switch (info)
	{
	case TARGET_UTF8_STRING:
		/* it's already in utf8 */
		gtk_selection_data_set_text (selection_data_ptr, stripped, len);
		break;
	case TARGET_TEXT:
	case TARGET_COMPOUND_TEXT:
#ifdef GDK_WINDOWING_X11
		{
			GdkDisplay *display;
#if HAVE_GTK3
			GdkWindow *window = gtk_widget_get_window (widget);

			if (!window)
				break;
			display = gdk_window_get_display (window);
#endif
#if !HAVE_GTK3
			display = gdk_window_get_display (widget->window);
#endif
			GdkAtom encoding;
			gint format;
			gint new_length;

			gdk_x11_display_string_to_compound_text (display, stripped, &encoding,
												&format, &new_text, &new_length);
			gtk_selection_data_set (selection_data_ptr, encoding, format,
											new_text, new_length);
			gdk_x11_free_compound_text (new_text);

		}
		break;
#endif
	default:
		new_text = g_locale_from_utf8 (stripped, len, NULL, &glen, NULL);
		gtk_selection_data_set (selection_data_ptr, GDK_SELECTION_TYPE_STRING,
										8, new_text, glen);
		g_free (new_text);
	}

	g_free (stripped);
}

static gboolean
gtk_xtext_scroll (GtkWidget *widget, GdkEventScroll *event)
{
	GtkXText *xtext = GTK_XTEXT (widget);
	gfloat new_value;

	if (event->direction == GDK_SCROLL_UP)		/* mouse wheel pageUp */
	{
		new_value = xtext_adj_get_value (xtext->adj) -
			(xtext_adj_get_page_increment (xtext->adj) / 10);
		if (new_value < xtext_adj_get_lower (xtext->adj))
			new_value = xtext_adj_get_lower (xtext->adj);
		xtext_adj_set_value (xtext->adj, new_value);
	}
	else if (event->direction == GDK_SCROLL_DOWN)	/* mouse wheel pageDn */
	{
		new_value = xtext_adj_get_value (xtext->adj) +
			(xtext_adj_get_page_increment (xtext->adj) / 10);
		if (new_value > (xtext_adj_get_upper (xtext->adj) -
			xtext_adj_get_page_size (xtext->adj)))
			new_value = xtext_adj_get_upper (xtext->adj) -
				xtext_adj_get_page_size (xtext->adj);
		xtext_adj_set_value (xtext->adj, new_value);
	}

	return FALSE;
}

static void
gtk_xtext_scroll_adjustments (GtkXText *xtext, GtkAdjustment *hadj, GtkAdjustment *vadj)
{
	/* hadj is ignored entirely */

	if (vadj)
		g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
	else
		vadj = GTK_ADJUSTMENT(gtk_adjustment_new (0, 0, 1, 1, 1, 1));

	if (xtext->adj && (xtext->adj != vadj))
	{
		g_signal_handlers_disconnect_by_func (xtext->adj,
								gtk_xtext_adjustment_changed,
								xtext);
		g_object_unref (xtext->adj);
	}

	if (xtext->adj != vadj)
	{
		xtext->adj = vadj;
		g_object_ref_sink (xtext->adj);

		xtext->vc_signal_tag = g_signal_connect (xtext->adj, "value-changed",
							G_CALLBACK (gtk_xtext_adjustment_changed),
							xtext);

		gtk_xtext_adjustment_changed (xtext->adj, xtext);
	}
}

static void
gtk_xtext_class_init (GtkXTextClass * class)
{
	GtkWidgetClass *widget_class;
	GtkXTextClass *xtext_class;
#if HAVE_GTK3
	GObjectClass *object_class;
#endif
#if !HAVE_GTK3
	GtkObjectClass *object_class;
#endif

#if HAVE_GTK3
	object_class = G_OBJECT_CLASS (class);
#endif
#if !HAVE_GTK3
	object_class = (GtkObjectClass *) class;
#endif
	widget_class = (GtkWidgetClass *) class;
	xtext_class = (GtkXTextClass *) class;

	parent_class = g_type_class_peek (gtk_widget_get_type ());

	xtext_signals[WORD_CLICK] =
		g_signal_new ("word_click",
							G_TYPE_FROM_CLASS (object_class),
							G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
							G_STRUCT_OFFSET (GtkXTextClass, word_click),
							NULL, NULL,
							_zoitechat_marshal_VOID__POINTER_POINTER,
							G_TYPE_NONE,
							2, G_TYPE_POINTER, G_TYPE_POINTER);
	xtext_signals[SET_SCROLL_ADJUSTMENTS] =
		g_signal_new ("set_scroll_adjustments",
							G_OBJECT_CLASS_TYPE (object_class),
							G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
							G_STRUCT_OFFSET (GtkXTextClass, set_scroll_adjustments),
							NULL, NULL,
							_zoitechat_marshal_VOID__OBJECT_OBJECT,
							G_TYPE_NONE,
							2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);

#if HAVE_GTK3
	object_class->dispose = gtk_xtext_dispose;
	object_class->finalize = gtk_xtext_finalize;
#else
	object_class->destroy = gtk_xtext_destroy;
#endif

	widget_class->realize = gtk_xtext_realize;
	widget_class->unrealize = gtk_xtext_unrealize;
	widget_class->size_request = gtk_xtext_size_request;
	widget_class->size_allocate = gtk_xtext_size_allocate;
	widget_class->button_press_event = gtk_xtext_button_press;
	widget_class->button_release_event = gtk_xtext_button_release;
	widget_class->motion_notify_event = gtk_xtext_motion_notify;
	widget_class->selection_clear_event = (void *)gtk_xtext_selection_kill;
	widget_class->selection_get = gtk_xtext_selection_get;
	widget_class->expose_event = gtk_xtext_expose;
	widget_class->scroll_event = gtk_xtext_scroll;
	widget_class->leave_notify_event = gtk_xtext_leave_notify;
	widget_class->set_scroll_adjustments_signal = xtext_signals[SET_SCROLL_ADJUSTMENTS];

	xtext_class->word_click = NULL;
	xtext_class->set_scroll_adjustments = gtk_xtext_scroll_adjustments;
}

GType
gtk_xtext_get_type (void)
{
	static GType xtext_type = 0;

	if (!xtext_type)
	{
		static const GTypeInfo xtext_info =
		{
			sizeof (GtkXTextClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gtk_xtext_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GtkXText),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gtk_xtext_init,
		};

		xtext_type = g_type_register_static (GTK_TYPE_WIDGET, "GtkXText",
														 &xtext_info, 0);
	}

	return xtext_type;
}

/* strip MIRC colors and other attribs. */

/* CL: needs to strip hidden when called by gtk_xtext_text_width, but not when copying text */

typedef struct chunk_s {
	GSList *slp;
	int off1, len1, emph;
	offlen_t meta;
} chunk_t;

static void
xtext_do_chunk(chunk_t *c)
{
	offlen_t *meta;

	if (c->len1 == 0)
		return;

	meta = g_new (offlen_t, 1);
	meta->off = c->off1;
	meta->len = c->len1;
	meta->emph = c->emph;
	meta->width = 0;
	c->slp = g_slist_append (c->slp, meta);

	c->len1 = 0;
}

static unsigned char *
gtk_xtext_strip_color (unsigned char *text, int len, unsigned char *outbuf,
							  int *newlen, GSList **slpp, int strip_hidden)
{
	chunk_t c;
	int i = 0;
	int rcol = 0, bgcol = 0;
	int hidden = FALSE;
	unsigned char *new_str;
	unsigned char *text0 = text;
	int mbl;	/* multi-byte length */

	if (outbuf == NULL)
		new_str = g_malloc (len + 2);
	else
		new_str = outbuf;

	c.slp = NULL;
	c.off1 = 0;
	c.len1 = 0;
	c.emph = 0;
	while (len > 0)
	{
		mbl = charlen (text);
		if (mbl > len)
			goto bad_utf8;

		if (rcol > 0 && (isdigit (*text) || (*text == ',' && isdigit (text[1]) && !bgcol)))
		{
			if (text[1] != ',') rcol--;
			if (*text == ',')
			{
				rcol = 2;
				bgcol = 1;
			}
		} else
		{
			rcol = bgcol = 0;
			switch (*text)
			{
			case ATTR_COLOR:
				xtext_do_chunk (&c);
				rcol = 2;
				break;
			case ATTR_BEEP:
			case ATTR_RESET:
			case ATTR_REVERSE:
			case ATTR_BOLD:
			case ATTR_UNDERLINE:
			case ATTR_STRIKETHROUGH:
			case ATTR_ITALICS:
				xtext_do_chunk (&c);
				if (*text == ATTR_RESET)
					c.emph = 0;
				if (*text == ATTR_ITALICS)
					c.emph ^= EMPH_ITAL;
				if (*text == ATTR_BOLD)
					c.emph ^= EMPH_BOLD;
				break;
			case ATTR_HIDDEN:
				xtext_do_chunk (&c);
				c.emph ^= EMPH_HIDDEN;
				hidden = !hidden;
				break;
			default:
				if (strip_hidden == 2 || (!(hidden && strip_hidden)))
				{
					if (c.len1 == 0)
						c.off1 = text - text0;
					memcpy (new_str + i, text, mbl);
					i += mbl;
					c.len1 += mbl;
				}
			}
		}
		text += mbl;
		len -= mbl;
	}

bad_utf8:		/* Normal ending sequence, and give up if bad utf8 */
	xtext_do_chunk (&c);

	new_str[i] = 0;

	if (newlen != NULL)
		*newlen = i;

	if (slpp)
		*slpp = c.slp;
	else
		g_slist_free_full (c.slp, g_free);

	return new_str;
}

/* gives width of a string, excluding the mIRC codes */

static int
gtk_xtext_text_width_ent (GtkXText *xtext, textentry *ent)
{
	unsigned char *new_buf;
	GSList *slp0, *slp;
	int width;

	if (ent->slp)
	{
		g_slist_free_full (ent->slp, g_free);
		ent->slp = NULL;
	}

	new_buf = gtk_xtext_strip_color (ent->str, ent->str_len, xtext->scratch_buffer,
												NULL, &slp0, 2);

	width =  backend_get_text_width_slp (xtext, new_buf, slp0);
	ent->slp = slp0;

	for (slp = slp0; slp; slp = g_slist_next (slp))
	{
		offlen_t *meta;

		meta = slp->data;
		meta->width = backend_get_text_width_emph (xtext, ent->str + meta->off, meta->len, meta->emph);
	}
	return width;
}

static int
gtk_xtext_text_width (GtkXText *xtext, unsigned char *text, int len)
{
	unsigned char *new_buf;
	int new_len;
	GSList *slp;
	int width;

	new_buf = gtk_xtext_strip_color (text, len, xtext->scratch_buffer,
												&new_len, &slp, !xtext->ignore_hidden);

	width =  backend_get_text_width_slp (xtext, new_buf, slp);
	g_slist_free_full (slp, g_free);

	return width;
}

/* actually draw text to screen (one run with the same color/attribs) */

static int
gtk_xtext_render_flush (GtkXText * xtext, int x, int y, unsigned char *str,
								int len, int *emphasis)
{
	int str_width, dofill;
	cairo_surface_t *surface = NULL;
	int dest_x = 0, dest_y = 0;
	int tile_x = xtext->ts_x;
	int tile_y = xtext->ts_y;
#if HAVE_GTK3
	GdkWindow *window = gtk_widget_get_window (GTK_WIDGET (xtext));
#endif

	if (xtext->dont_render || len < 1 || xtext->hidden)
		return 0;

	str_width = backend_get_text_width_emph (xtext, str, len, *emphasis);

	if (xtext->dont_render2)
		return str_width;

	/* roll-your-own clipping (avoiding XftDrawString is always good!) */
	if (x > xtext->clip_x2 || x + str_width < xtext->clip_x)
		return str_width;
	if (y - xtext->font->ascent > xtext->clip_y2 || (y - xtext->font->ascent) + xtext->fontsize < xtext->clip_y)
		return str_width;

	if (xtext->render_hilights_only)
	{
		if (!xtext->in_hilight)	/* is it a hilight prefix? */
			return str_width;
		if (!xtext->un_hilight)	/* doing a hilight? no need to draw the text */
			goto dounder;
	}

#if HAVE_GTK3
	if (!window)
		return str_width;
	surface = gdk_window_create_similar_surface (window,
		CAIRO_CONTENT_COLOR_ALPHA, str_width, xtext->fontsize);
#endif
#if !HAVE_GTK3
	surface = gdk_window_create_similar_surface (GTK_WIDGET (xtext)->window,
		CAIRO_CONTENT_COLOR_ALPHA, str_width, xtext->fontsize);
#endif
	if (surface)
	{
		dest_x = x;
		dest_y = y - xtext->font->ascent;
		tile_x = xtext->ts_x - x;
		tile_y = xtext->ts_y - dest_y;

		x = 0;
		y = xtext->font->ascent;
		xtext->draw_surface = surface;
	}

	dofill = TRUE;

	/* backcolor is always handled by XDrawImageString */
	if (!xtext->backcolor && xtext->background_surface)
	{
	/* draw the background surface behind the text - CAUSES FLICKER HERE!! */
		xtext_draw_bg_offset (xtext, x, y - xtext->font->ascent, str_width,
							xtext->fontsize, tile_x, tile_y);
		dofill = FALSE;	/* already drawn the background */
	}

	backend_draw_text_emph (xtext, dofill, x, y, str, len, str_width, *emphasis);

	if (surface)
	{
		GdkRectangle clip;
		GdkRectangle dest;
		cairo_t *cr;

		xtext->draw_surface = NULL;
		clip.x = xtext->clip_x;
		clip.y = xtext->clip_y;
		clip.width = xtext->clip_x2 - xtext->clip_x;
		clip.height = xtext->clip_y2 - xtext->clip_y;

		dest.x = dest_x;
		dest.y = dest_y;
		dest.width = str_width;
		dest.height = xtext->fontsize;

		if (gdk_rectangle_intersect (&clip, &dest, &dest))
			/* dump the DB to window, but only within the clip_x/x2/y/y2 */
		{
			cr = xtext_create_context (xtext);
			cairo_save (cr);
			cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
			cairo_set_source_surface (cr, surface, dest_x, dest_y);
			cairo_rectangle (cr, dest.x, dest.y, dest.width, dest.height);
			cairo_fill (cr);
			cairo_restore (cr);
			cairo_destroy (cr);
		}
		cairo_surface_destroy (surface);
	}

	if (xtext->strikethrough)
	{
		cairo_t *cr;
		/* pango_attr_strikethrough_new does not render in the custom widget so we need to reinvent the wheel */
		y = dest_y + (xtext->fontsize / 2);
		cr = xtext_create_context (xtext);
		xtext_draw_line (xtext, cr, &xtext->fgc, dest_x, y, dest_x + str_width - 1, y);
		cairo_destroy (cr);
	}

	if (xtext->underline)
	{
dounder:
	{
		cairo_t *cr;

		if (surface)
			y = dest_y + xtext->font->ascent + 1;
		else
		{
			y++;
			dest_x = x;
		}
		/* draw directly to window, it's out of the range of our DB */
		cr = xtext_create_context (xtext);
		xtext_draw_line (xtext, cr, &xtext->fgc, dest_x, y, dest_x + str_width - 1, y);
		cairo_destroy (cr);
	}
	}

	return str_width;
}

static void
gtk_xtext_reset (GtkXText * xtext, int mark, int attribs)
{
	if (attribs)
	{
		xtext->underline = FALSE;
		xtext->strikethrough = FALSE;
		xtext->hidden = FALSE;
	}
	if (!mark)
	{
		xtext->backcolor = FALSE;
		if (xtext->col_fore != XTEXT_FG)
			xtext_set_fg (xtext, XTEXT_FG);
		if (xtext->col_back != XTEXT_BG)
			xtext_set_bg (xtext, XTEXT_BG);
	}
	xtext->col_fore = XTEXT_FG;
	xtext->col_back = XTEXT_BG;
	xtext->parsing_color = FALSE;
	xtext->parsing_backcolor = FALSE;
	xtext->nc = 0;
}

/*
 * gtk_xtext_search_offset (buf, ent, off) --
 * Look for arg offset in arg textentry
 * Return one or more flags:
 * 	GTK_MATCH_MID if we are in a match
 * 	GTK_MATCH_START if we're at the first byte of it
 * 	GTK_MATCH_END if we're at the first byte past it
 * 	GTK_MATCH_CUR if it is the current match
 */
#define GTK_MATCH_START	1
#define GTK_MATCH_MID	2
#define GTK_MATCH_END	4
#define GTK_MATCH_CUR	8
static int
gtk_xtext_search_offset (xtext_buffer *buf, textentry *ent, unsigned int off)
{
	GList *gl;
	offsets_t o;
	int flags = 0;

	for (gl = g_list_first (ent->marks); gl; gl = g_list_next (gl))
	{
		o.u = GPOINTER_TO_UINT (gl->data);
		if (off < o.o.start || off > o.o.end)
			continue;
		flags = GTK_MATCH_MID;
		if (off == o.o.start)
			flags |= GTK_MATCH_START;
		if (off == o.o.end)
		{
			gl = g_list_next (gl);
			if (gl)
			{
				o.u = GPOINTER_TO_UINT (gl->data);
				if (off ==  o.o.start)	/* If subseq match is adjacent */
				{
					flags |= (gl == buf->curmark)? GTK_MATCH_CUR: 0;
				}
				else		/* If subseq match is not adjacent */
				{
					flags |= GTK_MATCH_END;
				}
			}
			else		/* If there is no subseq match */
			{
				flags |= GTK_MATCH_END;
			}
		}
		else if (gl == buf->curmark)	/* If not yet at the end of this match */
		{
			flags |= GTK_MATCH_CUR;
		}
		break;
	}
	return flags;
}

/* render a single line, which WONT wrap, and parse mIRC colors */

#define RENDER_FLUSH x += gtk_xtext_render_flush (xtext, x, y, pstr, j, emphasis)

static int
gtk_xtext_render_str (GtkXText * xtext, int y, textentry * ent,
							 unsigned char *str, int len, int win_width, int indent,
							 int line, int left_only, int *x_size_ret, int *emphasis)
{
	int i = 0, x = indent, j = 0;
	unsigned char *pstr = str;
	int col_num, tmp;
	int offset;
	int mark = FALSE;
	int ret = 1;
	int k;
	int srch_underline = FALSE;
	int srch_mark = FALSE;

	xtext->in_hilight = FALSE;

	offset = str - ent->str;

	if (ent->mark_start != -1 &&
		 ent->mark_start <= i + offset && ent->mark_end > i + offset)
	{
		xtext_set_bg (xtext, XTEXT_MARK_BG);
		xtext_set_fg (xtext, XTEXT_MARK_FG);
		xtext->backcolor = TRUE;
		mark = TRUE;
	}
	if (xtext->hilight_ent == ent &&
		 xtext->hilight_start <= i + offset && xtext->hilight_end > i + offset)
	{
		if (!xtext->un_hilight)
		{
			xtext->underline = TRUE;
		}
		xtext->in_hilight = TRUE;
	}

	if (!xtext->skip_border_fills && !xtext->dont_render)
	{
		/* draw background to the left of the text */
		if (str == ent->str && indent > MARGIN && xtext->buffer->time_stamp)
		{
			/* don't overwrite the timestamp */
			if (indent > xtext->stamp_width)
			{
				xtext_draw_bg (xtext, xtext->stamp_width, y - xtext->font->ascent,
									indent - xtext->stamp_width, xtext->fontsize);
			}
		} else
		{
			/* fill the indent area with background gc */
			if (indent >= xtext->clip_x)
			{
				xtext_draw_bg (xtext, 0, y - xtext->font->ascent,
									MIN (indent, xtext->clip_x2), xtext->fontsize);
			}
		}
	}

	if (xtext->jump_in_offset > 0 && offset < xtext->jump_in_offset)
		xtext->dont_render2 = TRUE;

	while (i < len)
	{

		if (xtext->hilight_ent == ent && xtext->hilight_start == (i + offset))
		{
			RENDER_FLUSH;
			pstr += j;
			j = 0;
			if (!xtext->un_hilight)
			{
				xtext->underline = TRUE;
			}

			xtext->in_hilight = TRUE;
		}

		if ((xtext->parsing_color && isdigit (str[i]) && xtext->nc < 2) ||
			 (xtext->parsing_color && str[i] == ',' && isdigit (str[i+1]) && xtext->nc < 3 && !xtext->parsing_backcolor))
		{
			pstr++;
			if (str[i] == ',')
			{
				xtext->parsing_backcolor = TRUE;
				if (xtext->nc)
				{
					xtext->num[xtext->nc] = 0;
					xtext->nc = 0;
					col_num = atoi (xtext->num);
					if (col_num == 99)	/* mIRC lameness */
						col_num = XTEXT_FG;
					else
					if (col_num > XTEXT_MAX_COLOR)
						col_num = col_num % XTEXT_MIRC_COLS;
					xtext->col_fore = col_num;
					if (!mark)
						xtext_set_fg (xtext, col_num);
				}
			} else
			{
				xtext->num[xtext->nc] = str[i];
				if (xtext->nc < 7)
					xtext->nc++;
			}
		} else
		{
			if (xtext->parsing_color)
			{
				xtext->parsing_color = FALSE;
				if (xtext->nc)
				{
					xtext->num[xtext->nc] = 0;
					xtext->nc = 0;
					col_num = atoi (xtext->num);
					if (xtext->parsing_backcolor)
					{
						if (col_num == 99)	/* mIRC lameness */
							col_num = XTEXT_BG;
						else
						if (col_num > XTEXT_MAX_COLOR)
							col_num = col_num % XTEXT_MIRC_COLS;
						if (col_num == XTEXT_BG)
							xtext->backcolor = FALSE;
						else
							xtext->backcolor = TRUE;
						if (!mark)
							xtext_set_bg (xtext, col_num);
						xtext->col_back = col_num;
					} else
					{
						if (col_num == 99)	/* mIRC lameness */
							col_num = XTEXT_FG;
						else
						if (col_num > XTEXT_MAX_COLOR)
							col_num = col_num % XTEXT_MIRC_COLS;
						if (!mark)
							xtext_set_fg (xtext, col_num);
						xtext->col_fore = col_num;
					}
					xtext->parsing_backcolor = FALSE;
				} else
				{
					/* got a \003<non-digit>... i.e. reset colors */
					RENDER_FLUSH;
					pstr += j;
					j = 0;
					gtk_xtext_reset (xtext, mark, FALSE);
				}
			}

			if (!left_only && !mark &&
				 (k = gtk_xtext_search_offset (xtext->buffer, ent, offset + i)))
			{
				RENDER_FLUSH;
				pstr += j;
				j = 0;
				if (!(xtext->buffer->search_flags & highlight))
				{
					if (k & GTK_MATCH_CUR)
					{
						xtext_set_bg (xtext, XTEXT_MARK_BG);
						xtext_set_fg (xtext, XTEXT_MARK_FG);
						xtext->backcolor = TRUE;
						srch_mark = TRUE;
					} else
					{
						xtext_set_bg (xtext, xtext->col_back);
						xtext_set_fg (xtext, xtext->col_fore);
						xtext->backcolor = (xtext->col_back != XTEXT_BG)? TRUE: FALSE;
						srch_mark = FALSE;
					}
				}
				else
				{
					xtext->underline = (k & GTK_MATCH_CUR)? TRUE: FALSE;
					if (k & (GTK_MATCH_START | GTK_MATCH_MID))
					{
						xtext_set_bg (xtext, XTEXT_MARK_BG);
						xtext_set_fg (xtext, XTEXT_MARK_FG);
						xtext->backcolor = TRUE;
						srch_mark = TRUE;
					}
					if (k & GTK_MATCH_END)
					{
						xtext_set_bg (xtext, xtext->col_back);
						xtext_set_fg (xtext, xtext->col_fore);
						xtext->backcolor = (xtext->col_back != XTEXT_BG)? TRUE: FALSE;
						srch_mark = FALSE;
						xtext->underline = FALSE;
					}
					srch_underline = xtext->underline;
				}
			}

			switch (str[i])
			{
			case '\n':
			/*case ATTR_BEEP:*/
				break;
			case ATTR_REVERSE:
				RENDER_FLUSH;
				pstr += j + 1;
				j = 0;
				tmp = xtext->col_fore;
				xtext->col_fore = xtext->col_back;
				xtext->col_back = tmp;
				if (!mark)
				{
					xtext_set_fg (xtext, xtext->col_fore);
					xtext_set_bg (xtext, xtext->col_back);
				}
				if (xtext->col_back != XTEXT_BG)
					xtext->backcolor = TRUE;
				else
					xtext->backcolor = FALSE;
				break;
			case ATTR_BOLD:
				RENDER_FLUSH;
				*emphasis ^= EMPH_BOLD;
				pstr += j + 1;
				j = 0;
				break;
			case ATTR_UNDERLINE:
				RENDER_FLUSH;
				xtext->underline = !xtext->underline;
				pstr += j + 1;
				j = 0;
				break;
			case ATTR_STRIKETHROUGH:
				RENDER_FLUSH;
				xtext->strikethrough = !xtext->strikethrough;
				pstr += j + 1;
				j = 0;
				break;
			case ATTR_ITALICS:
				RENDER_FLUSH;
				*emphasis ^= EMPH_ITAL;
				pstr += j + 1;
				j = 0;
				break;
			case ATTR_HIDDEN:
				RENDER_FLUSH;
				xtext->hidden = (!xtext->hidden) & (!xtext->ignore_hidden);
				pstr += j + 1;
				j = 0;
				break;
			case ATTR_RESET:
				RENDER_FLUSH;
				*emphasis = 0;
				pstr += j + 1;
				j = 0;
				gtk_xtext_reset (xtext, mark, !xtext->in_hilight);
				break;
			case ATTR_COLOR:
				RENDER_FLUSH;
				xtext->parsing_color = TRUE;
				pstr += j + 1;
				j = 0;
				break;
			default:
				tmp = charlen (str + i);
				/* invalid utf8 safe guard */
				if (tmp + i > len)
					tmp = len - i;
				j += tmp;	/* move to the next utf8 char */
			}
		}
		i += charlen (str + i);	/* move to the next utf8 char */
		/* invalid utf8 safe guard */
		if (i > len)
			i = len;

		/* Separate the left part, the space and the right part
		   into separate runs, and reset bidi state inbetween.
		   Perform this only on the first line of the message.
                */
		if (offset == 0)
		{
			/* we've reached the end of the left part? */
			if ((pstr-str)+j == ent->left_len)
			{
				RENDER_FLUSH;
				pstr += j;
				j = 0;
			}
			else if ((pstr-str)+j == ent->left_len+1)
			{
				RENDER_FLUSH;
				pstr += j;
				j = 0;
			}
		}

		/* have we been told to stop rendering at this point? */
		if (xtext->jump_out_offset > 0 && xtext->jump_out_offset <= (i + offset))
		{
			gtk_xtext_render_flush (xtext, x, y, pstr, j, emphasis);
			ret = 0;	/* skip the rest of the lines, we're done. */
			j = 0;
			break;
		}

		if (xtext->jump_in_offset > 0 && xtext->jump_in_offset == (i + offset))
		{
			RENDER_FLUSH;
			pstr += j;
			j = 0;
			xtext->dont_render2 = FALSE;
		}

		if (xtext->hilight_ent == ent && xtext->hilight_end == (i + offset))
		{
			RENDER_FLUSH;
			pstr += j;
			j = 0;
			xtext->underline = FALSE;
			xtext->in_hilight = FALSE;
			if (xtext->render_hilights_only)
			{
				/* stop drawing this ent */
				ret = 0;
				break;
			}
		}

		if (!mark && ent->mark_start == (i + offset))
		{
			RENDER_FLUSH;
			pstr += j;
			j = 0;
			xtext_set_bg (xtext, XTEXT_MARK_BG);
			xtext_set_fg (xtext, XTEXT_MARK_FG);
			xtext->backcolor = TRUE;
			if (srch_underline)
			{
				xtext->underline = FALSE;
				srch_underline = FALSE;
			}
			mark = TRUE;
		}

		if (mark && ent->mark_end == (i + offset))
		{
			RENDER_FLUSH;
			pstr += j;
			j = 0;
			xtext_set_bg (xtext, xtext->col_back);
			xtext_set_fg (xtext, xtext->col_fore);
			if (xtext->col_back != XTEXT_BG)
				xtext->backcolor = TRUE;
			else
				xtext->backcolor = FALSE;
			mark = FALSE;
		}

	}

	if (j)
		RENDER_FLUSH;

	if (mark || srch_mark)
	{
		xtext_set_bg (xtext, xtext->col_back);
		xtext_set_fg (xtext, xtext->col_fore);
		if (xtext->col_back != XTEXT_BG)
			xtext->backcolor = TRUE;
		else
			xtext->backcolor = FALSE;
	}

	/* draw background to the right of the text */
	if (!left_only && !xtext->dont_render)
	{
		/* draw separator now so it doesn't appear to flicker */
		gtk_xtext_draw_sep (xtext, y - xtext->font->ascent);
		if (!xtext->skip_border_fills && xtext->clip_x2 >= x)
		{
			int xx = MAX (x, xtext->clip_x);

			xtext_draw_bg (xtext,
								xx,	/* x */
								y - xtext->font->ascent, /* y */
				MIN (xtext->clip_x2 - xx, (win_width + MARGIN) - xx), /* width */
								xtext->fontsize);		/* height */
		}
	}

	xtext->dont_render2 = FALSE;

	/* return how much we drew in the x direction */
	if (x_size_ret)
		*x_size_ret = x - indent;

	return ret;
}

/* walk through str until this line doesn't fit anymore */

static int
find_next_wrap (GtkXText * xtext, textentry * ent, unsigned char *str,
					 int win_width, int indent)
{
	unsigned char *last_space = str;
	unsigned char *orig_str = str;
	int str_width = indent;
	int rcol = 0, bgcol = 0;
	int hidden = FALSE;
	int mbl;
	int char_width;
	int ret;
	int limit_offset = 0;
	int emphasis = 0;
	GSList *lp;

	/* single liners */
	if (win_width >= ent->str_width + ent->indent)
		return ent->str_len;

	/* it does happen! */
	if (win_width < 1)
	{
		ret = ent->str_len - (str - ent->str);
		goto done;
	}

	/* Find emphasis value for the offset that is the first byte of our string */
	for (lp = ent->slp; lp; lp = g_slist_next (lp))
	{
		offlen_t *meta = lp->data;
		unsigned char *start, *end;

		start = ent->str + meta->off;
		end = start + meta->len;
		if (str >= start && str < end)
		{
			emphasis = meta->emph;
			break;
		}
	}

	while (1)
	{
		if (rcol > 0 && (isdigit (*str) || (*str == ',' && isdigit (str[1]) && !bgcol)))
		{
			if (str[1] != ',') rcol--;
			if (*str == ',')
			{
				rcol = 2;
				bgcol = 1;
			}
			limit_offset++;
			str++;
		} else
		{
			rcol = bgcol = 0;
			switch (*str)
			{
			case ATTR_COLOR:
				rcol = 2;
			case ATTR_BEEP:
			case ATTR_RESET:
			case ATTR_REVERSE:
			case ATTR_BOLD:
			case ATTR_UNDERLINE:
			case ATTR_STRIKETHROUGH:
			case ATTR_ITALICS:
				if (*str == ATTR_RESET)
					emphasis = 0;
				if (*str == ATTR_ITALICS)
					emphasis ^= EMPH_ITAL;
				if (*str == ATTR_BOLD)
					emphasis ^= EMPH_BOLD;
				limit_offset++;
				str++;
				break;
			case ATTR_HIDDEN:
				if (xtext->ignore_hidden)
					goto def;
				hidden = !hidden;
				limit_offset++;
				str++;
				break;
			default:
			def:
				mbl = charlen (str);
				char_width = backend_get_text_width_emph (xtext, str, mbl, emphasis);
				if (!hidden) str_width += char_width;
				if (str_width > win_width)
				{
					if (xtext->wordwrap)
					{
						if (str - last_space > WORDWRAP_LIMIT + limit_offset)
							ret = str - orig_str; /* fall back to character wrap */
						else
						{
							if (*last_space == ' ')
								last_space++;
							ret = last_space - orig_str;
							if (ret == 0) /* fall back to character wrap */
								ret = str - orig_str;
						}
						goto done;
					}
					ret = str - orig_str;
					goto done;
				}

				/* keep a record of the last space, for wordwrapping */
				if (is_del (*str))
				{
					last_space = str;
					limit_offset = 0;
				}

				/* progress to the next char */
				str += mbl;

			}
		}

		if (str >= ent->str + ent->str_len)
		{
			ret = str - orig_str;
			goto done;
		}
	}

done:

	/* must make progress */
	if (ret < 1)
		ret = 1;

	return ret;
}

/* find the offset, in bytes, that wrap number 'line' starts at */

static int
gtk_xtext_find_subline (GtkXText *xtext, textentry *ent, int line)
{
	int rlen = 0;

	if (line > 0)
	{
		rlen = GPOINTER_TO_UINT (g_slist_nth_data (ent->sublines, line - 1));
		if (rlen == 0)
			rlen = ent->str_len;
	}
	return rlen;
}

/* horrible hack for drawing time stamps */

static void
gtk_xtext_render_stamp (GtkXText * xtext, textentry * ent,
								char *text, int len, int line, int win_width)
{
	textentry tmp_ent;
	int jo, ji, hs;
	int xsize, y, emphasis;

	/* trashing ent here, so make a backup first */
	memcpy (&tmp_ent, ent, sizeof (tmp_ent));
	jo = xtext->jump_out_offset;	/* back these up */
	ji = xtext->jump_in_offset;
	hs = xtext->hilight_start;
	xtext->jump_out_offset = 0;
	xtext->jump_in_offset = 0;
	xtext->hilight_start = 0xffff;	/* temp disable */
	emphasis = 0;

	if (xtext->mark_stamp)
	{
		/* if this line is marked, mark this stamp too */
		if (ent->mark_start == 0)	
		{
			ent->mark_start = 0;
			ent->mark_end = len;
		}
		else
		{
			ent->mark_start = -1;
			ent->mark_end = -1;
		}
		ent->str = text;
	}

	y = (xtext->fontsize * line) + xtext->font->ascent - xtext->pixel_offset;
	gtk_xtext_render_str (xtext, y, ent, text, len,
								 win_width, 2, line, TRUE, &xsize, &emphasis);

	/* restore everything back to how it was */
	memcpy (ent, &tmp_ent, sizeof (tmp_ent));
	xtext->jump_out_offset = jo;
	xtext->jump_in_offset = ji;
	xtext->hilight_start = hs;

	/* with a non-fixed-width font, sometimes we don't draw enough
		background i.e. when this stamp is shorter than xtext->stamp_width */
	xsize += MARGIN;
	if (xsize < xtext->stamp_width)
	{
		y -= xtext->font->ascent;
		xtext_draw_bg (xtext,
							xsize,	/* x */
							y,			/* y */
							xtext->stamp_width - xsize,	/* width */
							xtext->fontsize					/* height */);
	}
}

/* render a single line, which may wrap to more lines */

static int
gtk_xtext_render_line (GtkXText * xtext, textentry * ent, int line,
							  int lines_max, int subline, int win_width)
{
	unsigned char *str;
	int indent, taken, entline, len, y, start_subline;
	int emphasis = 0;

	entline = taken = 0;
	str = ent->str;
	indent = ent->indent;
	start_subline = subline;

	/* draw the timestamp */
	if (xtext->auto_indent && xtext->buffer->time_stamp &&
		 (!xtext->skip_stamp || xtext->mark_stamp || xtext->force_stamp))
	{
		char *time_str;
		int len;

		len = xtext_get_stamp_str (ent->stamp, &time_str);
		gtk_xtext_render_stamp (xtext, ent, time_str, len, line, win_width);
		g_free (time_str);
	}

	/* draw each line one by one */
	do
	{
		if (entline > 0)
			len = GPOINTER_TO_INT (g_slist_nth_data (ent->sublines, entline)) - GPOINTER_TO_INT (g_slist_nth_data (ent->sublines, entline - 1));
		else
			len = GPOINTER_TO_INT (g_slist_nth_data (ent->sublines, entline));

		entline++;

		y = (xtext->fontsize * line) + xtext->font->ascent - xtext->pixel_offset;
		if (!subline)
		{
			if (!gtk_xtext_render_str (xtext, y, ent, str, len, win_width,
												indent, line, FALSE, NULL, &emphasis))
			{
				/* small optimization */
				gtk_xtext_draw_marker (xtext, ent, y - xtext->fontsize * (taken + start_subline + 1));
				return g_slist_length (ent->sublines) - subline;
			}
		} else
		{
			xtext->dont_render = TRUE;
			gtk_xtext_render_str (xtext, y, ent, str, len, win_width,
										 indent, line, FALSE, NULL, &emphasis);
			xtext->dont_render = FALSE;
			subline--;
			line--;
			taken--;
		}

		indent = xtext->buffer->indent;
		line++;
		taken++;
		str += len;

		if (line >= lines_max)
			break;

	}
	while (str < ent->str + ent->str_len);

	gtk_xtext_draw_marker (xtext, ent, y - xtext->fontsize * (taken + start_subline));

	return taken;
}

void
gtk_xtext_set_palette (GtkXText * xtext, const XTextColor *palette)
{
	int i;

	for (i = (XTEXT_COLS-1); i >= 0; i--)
	{
		xtext->palette[i] = palette[i];
	}

	if (gtk_widget_get_realized (GTK_WIDGET(xtext)))
	{
		xtext_set_fg (xtext, XTEXT_FG);
		xtext_set_bg (xtext, XTEXT_BG);

		xtext->marker_gc = xtext->palette[XTEXT_MARKER];
	}
	xtext->col_fore = XTEXT_FG;
	xtext->col_back = XTEXT_BG;
}

static void
gtk_xtext_fix_indent (xtext_buffer *buf)
{
	int j;

	/* make indent a multiple of the space width */
	if (buf->indent && buf->xtext->space_width)
	{
		j = 0;
		while (j < buf->indent)
		{
			j += buf->xtext->space_width;
		}
		buf->indent = j;
	}

	dontscroll (buf);	/* force scrolling off */
}

static void
gtk_xtext_recalc_widths (xtext_buffer *buf, int do_str_width)
{
	textentry *ent;

	/* since we have a new font, we have to recalc the text widths */
	ent = buf->text_first;
	while (ent)
	{
		if (do_str_width)
		{
			ent->str_width = gtk_xtext_text_width_ent (buf->xtext, ent);
		}
		if (ent->left_len != -1)
		{
			ent->indent =
				(buf->indent -
				 gtk_xtext_text_width (buf->xtext, ent->str,
										ent->left_len)) - buf->xtext->space_width;
			if (ent->indent < MARGIN)
				ent->indent = MARGIN;
		}
		ent = ent->next;
	}

	gtk_xtext_calc_lines (buf, FALSE);
}

int
gtk_xtext_set_font (GtkXText *xtext, char *name)
{

	if (xtext->font)
		backend_font_close (xtext);

	/* realize now, so that font_open has a XDisplay */
	gtk_widget_realize (GTK_WIDGET (xtext));

	backend_font_open (xtext, name);
	if (xtext->font == NULL)
		return FALSE;

	{
		char *time_str;
		int stamp_size = xtext_get_stamp_str (time(0), &time_str);
		xtext->stamp_width =
			gtk_xtext_text_width (xtext, time_str, stamp_size) + MARGIN;
		g_free (time_str);
	}

	gtk_xtext_fix_indent (xtext->buffer);

	if (gtk_widget_get_realized (GTK_WIDGET(xtext)))
		gtk_xtext_recalc_widths (xtext->buffer, TRUE);

	return TRUE;
}

void
gtk_xtext_set_background (GtkXText * xtext, cairo_surface_t *surface)
{
	if (xtext->background_surface)
	{
		cairo_surface_destroy (xtext->background_surface);
		xtext->background_surface = NULL;
	}

	dontscroll (xtext->buffer);
	if (surface)
	{
		xtext->background_surface = cairo_surface_reference (surface);
	}

	if (surface && gtk_widget_get_realized (GTK_WIDGET(xtext)))
	{
		xtext->ts_x = xtext->ts_y = 0;
	}
}

void
gtk_xtext_save (GtkXText * xtext, int fh)
{
	textentry *ent;
	int newlen;
	char *buf;

	ent = xtext->buffer->text_first;
	while (ent)
	{
		buf = gtk_xtext_strip_color (ent->str, ent->str_len, NULL,
											  &newlen, NULL, FALSE);
		write (fh, buf, newlen);
		write (fh, "\n", 1);
		g_free (buf);
		ent = ent->next;
	}
}

/* count how many lines 'ent' will take (with wraps) */

static int
gtk_xtext_lines_taken (xtext_buffer *buf, textentry * ent)
{
	unsigned char *str;
	int indent, len;
	int win_width;

	g_slist_free (ent->sublines);
	ent->sublines = NULL;
	win_width = buf->window_width - MARGIN;

	if (win_width >= ent->indent + ent->str_width)
	{
		ent->sublines = g_slist_append (ent->sublines, GINT_TO_POINTER (ent->str_len));
		return 1;
	}

	indent = ent->indent;
	str = ent->str;

	do
	{
		len = find_next_wrap (buf->xtext, ent, str, win_width, indent);
		ent->sublines = g_slist_append (ent->sublines, GINT_TO_POINTER (str + len - ent->str));
		indent = buf->indent;
		str += len;
	}
	while (str < ent->str + ent->str_len);

	return g_slist_length (ent->sublines);
}

/* Calculate number of actual lines (with wraps), to set adj->lower. *
 * This should only be called when the window resizes.               */

static void
gtk_xtext_calc_lines (xtext_buffer *buf, int fire_signal)
{
	textentry *ent;
	int width;
	int height;
	int lines;
	GdkWindow *window;

#if HAVE_GTK3
	window = gtk_widget_get_window (GTK_WIDGET (buf->xtext));
#endif
#if !HAVE_GTK3
	window = GTK_WIDGET (buf->xtext)->window;
#endif
	if (!window)
		return;
	height = gdk_window_get_height (window);
	width = gdk_window_get_width (window);
	width -= MARGIN;

	if (width < 30 || height < buf->xtext->fontsize || width < buf->indent + 30)
		return;

	lines = 0;
	ent = buf->text_first;
	while (ent)
	{
		lines += gtk_xtext_lines_taken (buf, ent);
		ent = ent->next;
	}

	buf->pagetop_ent = NULL;
	buf->num_lines = lines;
	gtk_xtext_adjustment_set (buf, fire_signal);
}

/* find the n-th line in the linked list, this includes wrap calculations */

static textentry *
gtk_xtext_nth (GtkXText *xtext, int line, int *subline)
{
	int lines = 0;
	textentry *ent;

	ent = xtext->buffer->text_first;

	/* -- optimization -- try to make a short-cut using the pagetop ent */
	if (xtext->buffer->pagetop_ent)
	{
		if (line == xtext->buffer->pagetop_line)
		{
			*subline = xtext->buffer->pagetop_subline;
			return xtext->buffer->pagetop_ent;
		}
		if (line > xtext->buffer->pagetop_line)
		{
			/* lets start from the pagetop instead of the absolute beginning */
			ent = xtext->buffer->pagetop_ent;
			lines = xtext->buffer->pagetop_line - xtext->buffer->pagetop_subline;
		}
		else if (line > xtext->buffer->pagetop_line - line)
		{
			/* move backwards from pagetop */
			ent = xtext->buffer->pagetop_ent;
			lines = xtext->buffer->pagetop_line - xtext->buffer->pagetop_subline;
			while (1)
			{
				if (lines <= line)
				{
					*subline = line - lines;
					return ent;
				}
				ent = ent->prev;
				if (!ent)
					break;
				lines -= g_slist_length (ent->sublines);
			}
			return NULL;
		}
	}
	/* -- end of optimization -- */

	while (ent)
	{
		lines += g_slist_length (ent->sublines);
		if (lines > line)
		{
			*subline = g_slist_length (ent->sublines) - (lines - line);
			return ent;
		}
		ent = ent->next;
	}
	return NULL;
}

/* render enta (or an inclusive range enta->entb) */

static int
gtk_xtext_render_ents (GtkXText * xtext, textentry * enta, textentry * entb)
{
	textentry *ent, *orig_ent, *tmp_ent;
	int line;
	int lines_max;
	int width;
	int height;
	int subline;
	int drawing = FALSE;
	GdkWindow *window;

	if (xtext->buffer->indent < MARGIN)
		xtext->buffer->indent = MARGIN;	  /* 2 pixels is our left margin */

#if HAVE_GTK3
	window = gtk_widget_get_window (GTK_WIDGET (xtext));
#endif
#if !HAVE_GTK3
	window = GTK_WIDGET (xtext)->window;
#endif
	if (!window)
		return 0;
	height = gdk_window_get_height (window);
	width = gdk_window_get_width (window);
	width -= MARGIN;

	if (width < 32 || height < xtext->fontsize || width < xtext->buffer->indent + 30)
		return 0;

	lines_max = ((height + xtext->pixel_offset) / xtext->fontsize) + 1;
	line = 0;
	orig_ent = xtext->buffer->pagetop_ent;
	subline = xtext->buffer->pagetop_subline;

	/* used before a complete page is in buffer */
	if (orig_ent == NULL)
		orig_ent = xtext->buffer->text_first;

	/* check if enta is before the start of this page */
	if (entb)
	{
		tmp_ent = orig_ent;
		while (tmp_ent)
		{
			if (tmp_ent == enta)
				break;
			if (tmp_ent == entb)
			{
				drawing = TRUE;
				break;
			}
			tmp_ent = tmp_ent->next;
		}
	}

	ent = orig_ent;
	while (ent)
	{
		if (entb && ent == enta)
			drawing = TRUE;

		if (drawing || ent == entb || ent == enta)
		{
			gtk_xtext_reset (xtext, FALSE, TRUE);
			line += gtk_xtext_render_line (xtext, ent, line, lines_max,
													 subline, width);
			subline = 0;
			xtext->jump_in_offset = 0;	/* jump_in_offset only for the 1st */
		} else
		{
			if (ent == orig_ent)
			{
				line -= subline;
				subline = 0;
			}
			line += g_slist_length (ent->sublines);
		}

		if (ent == entb)
			break;

		if (line >= lines_max)
			break;

		ent = ent->next;
	}

	/* space below last line */
	return (xtext->fontsize * line) - xtext->pixel_offset;
}

/* render a whole page/window, starting from 'startline' */

static void
gtk_xtext_render_page (GtkXText * xtext)
{
	textentry *ent;
	int line;
	int lines_max;
	int width;
	int height;
	int subline;
	gdouble adj_value = xtext_adj_get_value (xtext->adj);
	gdouble adj_page_size = xtext_adj_get_page_size (xtext->adj);
	int startline = adj_value;
	int pos, overlap;
	GdkWindow *window;

	if(!gtk_widget_get_realized(GTK_WIDGET(xtext)))
	  return;

	if (xtext->buffer->indent < MARGIN)
		xtext->buffer->indent = MARGIN;	  /* 2 pixels is our left margin */

#if HAVE_GTK3
	window = gtk_widget_get_window (GTK_WIDGET (xtext));
#endif
#if !HAVE_GTK3
	window = GTK_WIDGET (xtext)->window;
#endif
	if (!window)
		return;
	width = gdk_window_get_width (window);
	height = gdk_window_get_height (window);

	if (width < 34 || height < xtext->fontsize || width < xtext->buffer->indent + 32)
		return;

	xtext->pixel_offset = (adj_value - startline) *
		xtext->fontsize;

	subline = line = 0;
	ent = xtext->buffer->text_first;

	if (startline > 0)
		ent = gtk_xtext_nth (xtext, startline, &subline);

	xtext->buffer->pagetop_ent = ent;
	xtext->buffer->pagetop_subline = subline;
	xtext->buffer->pagetop_line = startline;

	if (xtext->buffer->num_lines <= adj_page_size)
		dontscroll (xtext->buffer);

	pos = adj_value * xtext->fontsize;
	overlap = xtext->buffer->last_pixel_pos - pos;
	xtext->buffer->last_pixel_pos = pos;

#ifndef __APPLE__
	if (!xtext->background_surface && abs (overlap) < height)
	{
		GdkRectangle area;
		cairo_t *cr;
		int src_x = 0;
		int src_y = 0;
		int dest_x = 0;
		int dest_y = 0;
		int copy_height = 0;

		if (overlap < 1)	/* DOWN */
		{
			int remainder;
			cairo_surface_t *surface;

			src_y = -overlap;
			dest_y = 0;
			copy_height = height + overlap;
			cr = xtext_create_context (xtext);
			cairo_save (cr);
			cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
#if HAVE_GTK3
			surface = xtext_surface_from_window (window);
#endif
#if !HAVE_GTK3
			surface = xtext_surface_from_window (GTK_WIDGET (xtext)->window);
#endif
			if (!surface)
			{
				cairo_restore (cr);
				cairo_destroy (cr);
				goto full_redraw;
			}
			cairo_set_source_surface (cr, surface, dest_x - src_x, dest_y - src_y);
			cairo_surface_destroy (surface);
			cairo_rectangle (cr, dest_x, dest_y, width, copy_height);
			cairo_fill (cr);
			cairo_restore (cr);
			cairo_destroy (cr);
			remainder = ((height - xtext->font->descent) % xtext->fontsize) +
							xtext->font->descent;
			area.y = (height + overlap) - remainder;
			area.height = remainder - overlap;
		} else
		{
			cairo_surface_t *surface;

			src_y = 0;
			dest_y = overlap;
			copy_height = height - overlap;
			cr = xtext_create_context (xtext);
			cairo_save (cr);
			cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
#if HAVE_GTK3
			surface = xtext_surface_from_window (window);
#endif
#if !HAVE_GTK3
			surface = xtext_surface_from_window (GTK_WIDGET (xtext)->window);
#endif
			if (!surface)
			{
				cairo_restore (cr);
				cairo_destroy (cr);
				goto full_redraw;
			}
			cairo_set_source_surface (cr, surface, dest_x - src_x, dest_y - src_y);
			cairo_surface_destroy (surface);
			cairo_rectangle (cr, dest_x, dest_y, width, copy_height);
			cairo_fill (cr);
			cairo_restore (cr);
			cairo_destroy (cr);
			area.y = 0;
			area.height = overlap;
		}

		if (area.height > 0)
		{
			area.x = 0;
			area.width = width;
			gtk_xtext_paint (GTK_WIDGET (xtext), &area);
		}

		return;
	}
#endif

full_redraw:
	width -= MARGIN;
	lines_max = ((height + xtext->pixel_offset) / xtext->fontsize) + 1;

	while (ent)
	{
		gtk_xtext_reset (xtext, FALSE, TRUE);
		line += gtk_xtext_render_line (xtext, ent, line, lines_max,
												 subline, width);
		subline = 0;

		if (line >= lines_max)
			break;

		ent = ent->next;
	}

	line = (xtext->fontsize * line) - xtext->pixel_offset;
	/* fill any space below the last line with our background GC */
	xtext_draw_bg (xtext, 0, line, width + MARGIN, height - line);

	/* draw the separator line */
	gtk_xtext_draw_sep (xtext, -1);
}

void
gtk_xtext_refresh (GtkXText * xtext)
{
	if (gtk_widget_get_realized (GTK_WIDGET (xtext)))
	{
		gtk_xtext_render_page (xtext);
	}
}

static int
gtk_xtext_kill_ent (xtext_buffer *buffer, textentry *ent)
{
	int visible;

	/* Set visible to TRUE if this is the current buffer */
	/* and this ent shows up on the screen now */
	visible = buffer->xtext->buffer == buffer &&
				 gtk_xtext_check_ent_visibility (buffer->xtext, ent, 0);

	if (ent == buffer->pagetop_ent)
		buffer->pagetop_ent = NULL;

	if (ent == buffer->last_ent_start)
	{
		buffer->last_ent_start = ent->next;
		buffer->last_offset_start = 0;
	}

	if (ent == buffer->last_ent_end)
	{
		buffer->last_ent_start = NULL;
		buffer->last_ent_end = NULL;
	}

	if (buffer->marker_pos == ent)
	{
		/* Allow for "Marker line reset because exceeded scrollback limit. to appear. */
		buffer->marker_pos = ent->next;
		buffer->marker_state = MARKER_RESET_BY_KILL;
	}

	if (ent->marks)
	{
		gtk_xtext_search_textentry_del (buffer, ent);
	}

	g_slist_free_full (ent->slp, g_free);
	g_slist_free (ent->sublines);

	g_free (ent);
	return visible;
}

/* remove the topline from the list */

static void
gtk_xtext_remove_top (xtext_buffer *buffer)
{
	textentry *ent;

	ent = buffer->text_first;
	if (!ent)
		return;
	buffer->num_lines -= g_slist_length (ent->sublines);
	buffer->pagetop_line -= g_slist_length (ent->sublines);
	buffer->last_pixel_pos -= (g_slist_length (ent->sublines) * buffer->xtext->fontsize);
	buffer->text_first = ent->next;
	if (buffer->text_first)
		buffer->text_first->prev = NULL;
	else
		buffer->text_last = NULL;

	buffer->old_value -= g_slist_length (ent->sublines);
	if (buffer->xtext->buffer == buffer)	/* is it the current buffer? */
	{
		xtext_adj_set_value (buffer->xtext->adj,
			xtext_adj_get_value (buffer->xtext->adj) -
			g_slist_length (ent->sublines));
		buffer->xtext->select_start_adj -= g_slist_length (ent->sublines);
	}

	if (gtk_xtext_kill_ent (buffer, ent))
	{
		if (!buffer->xtext->add_io_tag)
		{
			/* remove scrolling events */
			if (buffer->xtext->io_tag)
			{
				g_source_remove (buffer->xtext->io_tag);
				buffer->xtext->io_tag = 0;
			}
			buffer->xtext->force_render = TRUE;
			buffer->xtext->add_io_tag = g_timeout_add (REFRESH_TIMEOUT * 2,
														(GSourceFunc)
														gtk_xtext_render_page_timeout,
														buffer->xtext);
		}
	}
}

static void
gtk_xtext_remove_bottom (xtext_buffer *buffer)
{
	textentry *ent;

	ent = buffer->text_last;
	if (!ent)
		return;
	buffer->num_lines -= g_slist_length (ent->sublines);
	buffer->text_last = ent->prev;
	if (buffer->text_last)
		buffer->text_last->next = NULL;
	else
		buffer->text_first = NULL;

	if (gtk_xtext_kill_ent (buffer, ent))
	{
		if (!buffer->xtext->add_io_tag)
		{
			/* remove scrolling events */
			if (buffer->xtext->io_tag)
			{
				g_source_remove (buffer->xtext->io_tag);
				buffer->xtext->io_tag = 0;
			}
			buffer->xtext->force_render = TRUE;
			buffer->xtext->add_io_tag = g_timeout_add (REFRESH_TIMEOUT * 2,
														(GSourceFunc)
														gtk_xtext_render_page_timeout,
														buffer->xtext);
		}
	}
}

/* If lines=0 => clear all */

void
gtk_xtext_clear (xtext_buffer *buf, int lines)
{
	textentry *next;
	int marker_reset = FALSE;

	if (lines != 0)
	{
		if (lines < 0)
		{
			/* delete lines from bottom */
			lines *= -1;
			while (lines)
			{
				if (buf->text_last == buf->marker_pos)
					marker_reset = TRUE;
				gtk_xtext_remove_bottom (buf);
				lines--;
			}
		}
		else
		{
			/* delete lines from top */
			while (lines)
			{
				if (buf->text_first == buf->marker_pos)
					marker_reset = TRUE;
				gtk_xtext_remove_top (buf);
				lines--;
			}
		}
	}
	else
	{
		/* delete all */
		if (buf->search_found)
			gtk_xtext_search_fini (buf);
		if (buf->xtext->auto_indent)
			buf->indent = MARGIN;
		buf->scrollbar_down = TRUE;
		buf->last_ent_start = NULL;
		buf->last_ent_end = NULL;
		buf->marker_pos = NULL;
		if (buf->text_first)
			marker_reset = TRUE;
		dontscroll (buf);

		while (buf->text_first)
		{
			next = buf->text_first->next;
			g_free (buf->text_first);
			buf->text_first = next;
		}
		buf->text_last = NULL;
	}

	if (buf->xtext->buffer == buf)
	{
		gtk_xtext_calc_lines (buf, TRUE);
		gtk_xtext_refresh (buf->xtext);
	} else
	{
		gtk_xtext_calc_lines (buf, FALSE);
	}

	if (marker_reset)
		buf->marker_state = MARKER_RESET_BY_CLEAR;
}

static gboolean
gtk_xtext_check_ent_visibility (GtkXText * xtext, textentry *find_ent, int add)
{
	textentry *ent;
	int lines;
	xtext_buffer *buf = xtext->buffer;
	int height;
	GdkWindow *window;

	if (find_ent == NULL)
	{
		return FALSE;
	}

#if HAVE_GTK3
	window = gtk_widget_get_window (GTK_WIDGET (xtext));
#endif
#if !HAVE_GTK3
	window = GTK_WIDGET (xtext)->window;
#endif
	if (!window)
		return FALSE;
	height = gdk_window_get_height (window);

	ent = buf->pagetop_ent;
	/* If top line not completely displayed return FALSE */
	if (ent == find_ent && buf->pagetop_subline > 0)
	{
		return FALSE;
	}
	/* Loop through line positions looking for find_ent */
	lines = ((height + xtext->pixel_offset) / xtext->fontsize) + buf->pagetop_subline + add;
	while (ent)	
	{
		lines -= g_slist_length (ent->sublines);
		if (lines <= 0)
		{
			return FALSE;
		}
		if (ent == find_ent)
		{
			return TRUE;
		}
		ent = ent->next;
	}

	return FALSE;
}

void
gtk_xtext_check_marker_visibility (GtkXText * xtext)
{
	if (gtk_xtext_check_ent_visibility (xtext, xtext->buffer->marker_pos, 1))
		xtext->buffer->marker_seen = TRUE;
}

static void
gtk_xtext_unstrip_color (gint start, gint end, GSList *slp, GList **gl, gint maxo)
{
	gint off1, off2, curlen;
	GSList *cursl;
	offsets_t marks;
	offlen_t *meta;

	off1 = 0;
	curlen = 0;
	cursl = slp;
	while (cursl)
	{
		meta = cursl->data;
		if (start < meta->len)
		{
			off1 = meta->off + start;
			break;
		}
		curlen += meta->len;
		start -= meta->len;
		end -= meta->len;
		cursl = g_slist_next (cursl);
	}

	off2 = off1;
	while (cursl)
	{
		meta = cursl->data;
		if (end < meta->len)
		{
			off2 = meta->off + end;
			break;
		}
		curlen += meta->len;
		end -= meta->len;
		cursl = g_slist_next (cursl);
	}
	if (!cursl)
	{
		off2 = maxo;
	}

	marks.o.start = off1;
	marks.o.end = off2;
	*gl = g_list_append (*gl, GUINT_TO_POINTER (marks.u));
}

/* Search a single textentry for occurrence(s) of search arg string */
static GList *
gtk_xtext_search_textentry (xtext_buffer *buf, textentry *ent)
{
	gchar *str;								/* text string to be searched */
	GList *gl = NULL;
	GSList *slp;
	gint lstr;

	if (buf->search_text == NULL)
	{
		return gl;
	}

	str = gtk_xtext_strip_color (ent->str, ent->str_len, buf->xtext->scratch_buffer,
										  &lstr, &slp, !buf->xtext->ignore_hidden);

	/* Regular-expression matching --- */
	if (buf->search_flags & regexp)
	{
		GMatchInfo *gmi;
		gint start, end;

		if (buf->search_re == NULL)
		{
			return gl;
		}
		g_regex_match (buf->search_re, str, 0, &gmi);
		while (g_match_info_matches (gmi))
		{
			g_match_info_fetch_pos (gmi, 0,  &start, &end);
			gtk_xtext_unstrip_color (start, end, slp, &gl, ent->str_len);
			g_match_info_next (gmi, NULL);
		}
		g_match_info_free (gmi);

	/* Non-regular-expression matching --- */
	} else {
		gchar *hay, *pos;
		gint lhay, off, len;
		gint match = buf->search_flags & case_match;

		hay = match? g_strdup (str): g_utf8_casefold (str, lstr);
		lhay = strlen (hay);

		for (pos = hay, len = lhay; len;
			  off += buf->search_lnee, pos = hay + off, len = lhay - off)
		{
			str = g_strstr_len (pos, len, buf->search_nee);
			if (str == NULL)
			{
				break;
			}
			off = str - hay;
			gtk_xtext_unstrip_color (off, off + buf->search_lnee,
											 slp, &gl, ent->str_len);
		}

		g_free (hay);
	}

	/* Common processing --- */
	g_slist_free_full (slp, g_free);
	return gl;
}

/* Add a list of found search results to an entry, maybe NULL */
static void
gtk_xtext_search_textentry_add (xtext_buffer *buf, textentry *ent, GList *gl, gboolean pre)
{
	ent->marks = gl;
	if (gl)
	{
		buf->search_found = (pre? g_list_prepend: g_list_append) (buf->search_found, ent);
		if (pre == FALSE && buf->hintsearch == NULL)
		{
			buf->hintsearch = ent;
		}
	}
}

/* Free all search information for a textentry */
static void
gtk_xtext_search_textentry_del (xtext_buffer *buf, textentry *ent)
{
	g_list_free (ent->marks);
	ent->marks = NULL;
	if (buf->cursearch && buf->cursearch->data == ent)
	{
		buf->cursearch = NULL;
		buf->curmark = NULL;
		buf->curdata.u = 0;
	}
	if (buf->pagetop_ent == ent)
	{
		buf->pagetop_ent = NULL;
	}
	if (buf->hintsearch == ent)
	{
		buf->hintsearch = NULL;
	}
	buf->search_found = g_list_remove (buf->search_found, ent);
}

/* Used only by glist_foreach */
static void
gtk_xtext_search_textentry_fini (gpointer entp, gpointer dummy)
{
	textentry *ent = entp;

	g_list_free (ent->marks);
	ent->marks = NULL;
}

/* Free all search information for all textentrys and the xtext_buffer */
static void
gtk_xtext_search_fini (xtext_buffer *buf)
{
	g_list_foreach (buf->search_found, gtk_xtext_search_textentry_fini, 0);
	g_list_free (buf->search_found);
	buf->search_found = NULL;
	g_free (buf->search_text);
	buf->search_text = NULL;
	g_free (buf->search_nee);
	buf->search_nee = NULL;
	buf->search_flags = 0;
	buf->cursearch = NULL;
	buf->curmark = NULL;
	/* but leave buf->curdata.u alone! */
	if (buf->search_re)
	{
		g_regex_unref (buf->search_re);
		buf->search_re = NULL;
	}
}

/* Returns TRUE if the base search information exists and is still okay to use */
static gboolean
gtk_xtext_search_init (xtext_buffer *buf, const gchar *text, gtk_xtext_search_flags flags, GError **perr)
{
	/* Of the five flags, backward and highlight_all do not need a new search */
	if (buf->search_found &&
		 strcmp (buf->search_text, text) == 0 &&
		 (buf->search_flags & case_match) == (flags & case_match) &&
		 (buf->search_flags & follow) == (flags & follow) &&
		 (buf->search_flags & regexp) == (flags & regexp))
	{
		return TRUE;
	}
	buf->hintsearch = buf->cursearch? buf->cursearch->data: NULL;
	gtk_xtext_search_fini (buf);
	buf->search_text = g_strdup (text);
	if (flags & regexp)
	{
		buf->search_re = g_regex_new (text, (flags & case_match)? 0: G_REGEX_CASELESS, 0, perr);
		if (perr && *perr)
		{
			return FALSE;
		}
	}
	else
	{
		if (flags & case_match)
		{
			buf->search_nee = g_strdup (text);
		}
		else
		{
			buf->search_nee = g_utf8_casefold (text, strlen (text));
		}
		buf->search_lnee = strlen (buf->search_nee);
	}
	buf->search_flags = flags;
	buf->cursearch = NULL;
	buf->curmark = NULL;
	/* but leave buf->curdata.u alone! */
	return FALSE;
}

#define BACKWARD (flags & backward)
#define FIRSTLAST(lp)  (BACKWARD? g_list_last(lp): g_list_first(lp))
#define NEXTPREVIOUS(lp) (BACKWARD? g_list_previous(lp): g_list_next(lp))
textentry *
gtk_xtext_search (GtkXText * xtext, const gchar *text, gtk_xtext_search_flags flags, GError **perr)
{
	textentry *ent = NULL;
	xtext_buffer *buf = xtext->buffer;
	GList *gl;

	if (buf->text_first == NULL)
	{
		return NULL;
	}

	/* If the text arg is NULL, one of these has been toggled: highlight follow */
	if (text == NULL)		/* Here on highlight or follow toggle */
	{
		gint oldfollow = buf->search_flags & follow;
		gint newfollow = flags & follow;

		/* If "Follow" has just been checked, search possible new textentries --- */
		if (newfollow && (newfollow != oldfollow))
		{
			gl = g_list_last (buf->search_found);
			ent = gl? gl->data: buf->text_first;
			for (; ent; ent = ent->next)
			{
				GList *gl;

				gl = gtk_xtext_search_textentry (buf, ent);
				gtk_xtext_search_textentry_add (buf, ent, gl, FALSE);
			}
		}
		buf->search_flags = flags;
		ent = buf->pagetop_ent;
	}

	/* if the text arg is "", the reset button has been clicked or Control-Shift-F has been hit */
	else if (text[0] == 0)		/* Let a null string do a reset. */
	{
		gtk_xtext_search_fini (buf);
	}

	/* If the text arg is neither NULL nor "", it's the search string */
	else
	{
		if (gtk_xtext_search_init (buf, text, flags, perr) == FALSE)	/* If a new search: */
		{
			if (perr && *perr)
			{
				return NULL;
			}
			for (ent = buf->text_first; ent; ent = ent->next)
			{
				GList *gl;

				gl = gtk_xtext_search_textentry (buf, ent);
				gtk_xtext_search_textentry_add (buf, ent, gl, TRUE);
			}
			buf->search_found = g_list_reverse (buf->search_found);
		}

		/* Now base search results are in place. */

		if (buf->search_found)
		{
			/* If we're in the midst of moving among found items */
			if (buf->cursearch)
			{
				ent = buf->cursearch->data;
				buf->curmark = NEXTPREVIOUS (buf->curmark);
				if (buf->curmark == NULL)
				{
					/* We've returned all the matches for this textentry. */
					buf->cursearch = NEXTPREVIOUS (buf->cursearch);
					if (buf->cursearch)
					{
						ent = buf->cursearch->data;
						buf->curmark = FIRSTLAST (ent->marks);
					}
					else	/* We've returned all the matches for all textentries */
					{
						ent = NULL;
					}
				}
			}

			/* If user changed the search, let's look starting where he was */
			else if (buf->hintsearch)
			{
				GList *mark;
				offsets_t last, this;
				/*
				 * If we already have a 'current' item from the last search, and if
				 * the first character of an occurrence on this line for this new search
				 * is within that former item, use the occurrence as current.
				 */
				ent = buf->hintsearch;
				last.u = buf->curdata.u;
				for (mark = ent->marks; mark; mark = mark->next)
				{
					this.u = GPOINTER_TO_UINT (mark->data);
					if (this.o.start >= last.o.start && this.o.start < last.o.end)
					break;
				}
				if (mark == NULL)
				{
					for (ent = buf->hintsearch; ent; ent = BACKWARD? ent->prev: ent->next)
						if (ent->marks)
							break;
					mark = ent? FIRSTLAST (ent->marks): NULL;
				}
				buf->cursearch = g_list_find (buf->search_found, ent);
				buf->curmark = mark;
			}

			/* This is a fresh search */
			else
			{
				buf->cursearch = FIRSTLAST (buf->search_found);
				ent = buf->cursearch->data;
				buf->curmark = FIRSTLAST (ent->marks);
			}
			buf->curdata.u = (buf->curmark)? GPOINTER_TO_UINT (buf->curmark->data): 0;
		}
	}
	buf->hintsearch = ent;

	if (!gtk_xtext_check_ent_visibility (xtext, ent, 1))
	{
		GtkAdjustment *adj = xtext->adj;
		float value;

		buf->pagetop_ent = NULL;
		for (value = 0, ent = buf->text_first;
			  ent && ent != buf->hintsearch; ent = ent->next)
		{
			value += g_slist_length (ent->sublines);
		}
		if (value > xtext_adj_get_upper (adj) - xtext_adj_get_page_size (adj))
		{
			value = xtext_adj_get_upper (adj) - xtext_adj_get_page_size (adj);
		}
		else if ((flags & backward)  && ent)
		{
			value -= xtext_adj_get_page_size (adj) -
				g_slist_length (ent->sublines);
			if (value < 0)
			{
				value = 0;
			}
		}
		xtext_adj_set_value (adj, value);
	}

	gtk_widget_queue_draw (GTK_WIDGET (xtext));

	return buf->hintsearch;
}
#undef BACKWARD
#undef FIRSTLAST
#undef NEXTPREVIOUS

static int
gtk_xtext_render_page_timeout (GtkXText * xtext)
{
	GtkAdjustment *adj = xtext->adj;

	xtext->add_io_tag = 0;

	/* less than a complete page? */
	if (xtext->buffer->num_lines <= xtext_adj_get_page_size (adj))
	{
		xtext->buffer->old_value = 0;
		xtext_adj_set_value (adj, 0);
		gtk_xtext_render_page (xtext);
	} else if (xtext->buffer->scrollbar_down)
	{
		g_signal_handler_block (xtext->adj, xtext->vc_signal_tag);
		gtk_xtext_adjustment_set (xtext->buffer, FALSE);
		xtext_adj_set_value (adj,
			xtext_adj_get_upper (adj) - xtext_adj_get_page_size (adj));
		g_signal_handler_unblock (xtext->adj, xtext->vc_signal_tag);
		xtext->buffer->old_value = xtext_adj_get_value (adj);
		gtk_xtext_render_page (xtext);
	} else
	{
		gtk_xtext_adjustment_set (xtext->buffer, TRUE);
		if (xtext->force_render)
		{
			xtext->force_render = FALSE;
			gtk_xtext_render_page (xtext);
		}
	}

	return 0;
}

/* append a textentry to our linked list */

static void
gtk_xtext_append_entry (xtext_buffer *buf, textentry * ent, time_t stamp)
{
	int i;

	/* we don't like tabs */
	i = 0;
	while (i < ent->str_len)
	{
		if (ent->str[i] == '\t')
			ent->str[i] = ' ';
		i++;
	}

	ent->stamp = stamp;
	if (stamp == 0)
		ent->stamp = time (0);
	ent->slp = NULL;
	ent->str_width = gtk_xtext_text_width_ent (buf->xtext, ent);
	ent->mark_start = -1;
	ent->mark_end = -1;
	ent->next = NULL;
	ent->marks = NULL;

	if (ent->indent < MARGIN)
		ent->indent = MARGIN;	  /* 2 pixels is the left margin */

	/* append to our linked list */
	if (buf->text_last)
		buf->text_last->next = ent;
	else
		buf->text_first = ent;
	ent->prev = buf->text_last;
	buf->text_last = ent;

	ent->sublines = NULL;
	buf->num_lines += gtk_xtext_lines_taken (buf, ent);

	if ((buf->marker_pos == NULL || buf->marker_seen) && (buf->xtext->buffer != buf || 
		!gtk_window_has_toplevel_focus (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (buf->xtext))))))
	{
		buf->marker_pos = ent;
		buf->marker_state = MARKER_IS_SET;
		dontscroll (buf); /* force scrolling off */
		buf->marker_seen = FALSE;
	}

	if (buf->xtext->max_lines > 2 && buf->xtext->max_lines < buf->num_lines)
	{
		gtk_xtext_remove_top (buf);
	}

	if (buf->xtext->buffer == buf)
	{
		/* this could be improved */
		if ((buf->num_lines - 1) <= xtext_adj_get_page_size (buf->xtext->adj))
			dontscroll (buf);

		if (!buf->xtext->add_io_tag)
		{
			/* remove scrolling events */
			if (buf->xtext->io_tag)
			{
				g_source_remove (buf->xtext->io_tag);
				buf->xtext->io_tag = 0;
			}
			buf->xtext->add_io_tag = g_timeout_add (REFRESH_TIMEOUT * 2,
															(GSourceFunc)
															gtk_xtext_render_page_timeout,
															buf->xtext);
		}
	}
	if (buf->scrollbar_down)
	{
		buf->old_value = buf->num_lines -
			xtext_adj_get_page_size (buf->xtext->adj);
		if (buf->old_value < 0)
			buf->old_value = 0;
	}
	if (buf->search_flags & follow)
	{
		GList *gl;

		gl = gtk_xtext_search_textentry (buf, ent);
		gtk_xtext_search_textentry_add (buf, ent, gl, FALSE);
	}
}

/* the main two public functions */

void
gtk_xtext_append_indent (xtext_buffer *buf,
								 unsigned char *left_text, int left_len,
								 unsigned char *right_text, int right_len,
								 time_t stamp)
{
	textentry *ent;
	unsigned char *str;
	int space;
	int tempindent;
	int left_width;

	if (left_len == -1)
		left_len = strlen (left_text);

	if (right_len == -1)
		right_len = strlen (right_text);

	if (left_len + right_len + 2 >= sizeof (buf->xtext->scratch_buffer))
		right_len = sizeof (buf->xtext->scratch_buffer) - left_len - 2;

	if (right_text[right_len-1] == '\n')
		right_len--;

	ent = g_malloc (left_len + right_len + 2 + sizeof (textentry));
	str = (unsigned char *) ent + sizeof (textentry);

	if (left_len)
		memcpy (str, left_text, left_len);
	str[left_len] = ' ';
	if (right_len)
		memcpy (str + left_len + 1, right_text, right_len);
	str[left_len + 1 + right_len] = 0;

	left_width = gtk_xtext_text_width (buf->xtext, left_text, left_len);

	ent->left_len = left_len;
	ent->str = str;
	ent->str_len = left_len + 1 + right_len;
	ent->indent = (buf->indent - left_width) - buf->xtext->space_width;

	/* This is copied into the scratch buffer later, double check math */
	g_assert (ent->str_len < sizeof (buf->xtext->scratch_buffer));

	if (buf->time_stamp)
		space = buf->xtext->stamp_width;
	else
		space = 0;

	/* do we need to auto adjust the separator position? */
	if (buf->xtext->auto_indent &&
		 buf->indent < buf->xtext->max_auto_indent &&
		 ent->indent < MARGIN + space)
	{
		tempindent = MARGIN + space + buf->xtext->space_width + left_width;

		if (tempindent > buf->indent)
			buf->indent = tempindent;

		if (buf->indent > buf->xtext->max_auto_indent)
			buf->indent = buf->xtext->max_auto_indent;

		gtk_xtext_fix_indent (buf);
		gtk_xtext_recalc_widths (buf, FALSE);

		ent->indent = (buf->indent - left_width) - buf->xtext->space_width;
		buf->xtext->force_render = TRUE;
	}

	gtk_xtext_append_entry (buf, ent, stamp);
}

void
gtk_xtext_append (xtext_buffer *buf, unsigned char *text, int len, time_t stamp)
{
	textentry *ent;
	gboolean truncate = FALSE;

	if (len == -1)
		len = strlen (text);

	if (text[len-1] == '\n')
		len--;

	if (len >= sizeof (buf->xtext->scratch_buffer))
	{
		len = sizeof (buf->xtext->scratch_buffer) - 1;
		truncate = TRUE;
	}

	ent = g_malloc (len + 1 + sizeof (textentry));
	ent->str = (unsigned char *) ent + sizeof (textentry);
	ent->str_len = len;
	if (len)
	{
		if (!truncate)
		{
			memcpy (ent->str, text, len);
			ent->str[len] = '\0';
		}
		else
		{
			safe_strcpy (ent->str, text, sizeof (buf->xtext->scratch_buffer));
			ent->str_len = strlen (ent->str);
		}
	}
	ent->indent = 0;
	ent->left_len = -1;

	gtk_xtext_append_entry (buf, ent, stamp);
}

gboolean
gtk_xtext_is_empty (xtext_buffer *buf)
{
	return buf->text_first == NULL;
}


int
gtk_xtext_lastlog (xtext_buffer *out, xtext_buffer *search_area)
{
	textentry *ent;
	int matches;
	GList *gl;

	ent = search_area->text_first;
	matches = 0;

	while (ent)
	{
		gl = gtk_xtext_search_textentry (out, ent);
		if (gl)
		{
			matches++;
			/* copy the text over */
			if (search_area->xtext->auto_indent)
			{
				gtk_xtext_append_indent (out, ent->str, ent->left_len,
												 ent->str + ent->left_len + 1,
												 ent->str_len - ent->left_len - 1, 0);
			}
			else
			{
				gtk_xtext_append (out, ent->str, ent->str_len, 0);
			}

			if (out->text_last)
			{
				out->text_last->stamp = ent->stamp;
				gtk_xtext_search_textentry_add (out, out->text_last, gl, TRUE);
			}
		}
		ent = ent->next;
	}
	out->search_found = g_list_reverse (out->search_found);

	return matches;
}

void
gtk_xtext_foreach (xtext_buffer *buf, GtkXTextForeach func, void *data)
{
	textentry *ent = buf->text_first;

	while (ent)
	{
		(*func) (buf->xtext, ent->str, data);
		ent = ent->next;
	}
}

void
gtk_xtext_set_indent (GtkXText *xtext, gboolean indent)
{
	xtext->auto_indent = indent;
}

void
gtk_xtext_set_max_indent (GtkXText *xtext, int max_auto_indent)
{
	xtext->max_auto_indent = max_auto_indent;
}

void
gtk_xtext_set_max_lines (GtkXText *xtext, int max_lines)
{
	xtext->max_lines = max_lines;
}

void
gtk_xtext_set_show_marker (GtkXText *xtext, gboolean show_marker)
{
	xtext->marker = show_marker;
}

void
gtk_xtext_set_show_separator (GtkXText *xtext, gboolean show_separator)
{
	xtext->separator = show_separator;
}

void
gtk_xtext_set_thin_separator (GtkXText *xtext, gboolean thin_separator)
{
	xtext->thinline = thin_separator;
}

void
gtk_xtext_set_time_stamp (xtext_buffer *buf, gboolean time_stamp)
{
	buf->time_stamp = time_stamp;
}

void
gtk_xtext_set_urlcheck_function (GtkXText *xtext, int (*urlcheck_function) (GtkWidget *, char *))
{
	xtext->urlcheck_function = urlcheck_function;
}

void
gtk_xtext_set_wordwrap (GtkXText *xtext, gboolean wordwrap)
{
	xtext->wordwrap = wordwrap;
}

void
gtk_xtext_set_marker_last (session *sess)
{
	xtext_buffer *buf = sess->res->buffer;

	buf->marker_pos = buf->text_last;
	buf->marker_state = MARKER_IS_SET;
}

void
gtk_xtext_reset_marker_pos (GtkXText *xtext)
{
	if (xtext->buffer->marker_pos)
	{
		xtext->buffer->marker_pos = NULL;
		dontscroll (xtext->buffer); /* force scrolling off */
		gtk_xtext_render_page (xtext);
		xtext->buffer->marker_state = MARKER_RESET_MANUALLY;
	}
}

int
gtk_xtext_moveto_marker_pos (GtkXText *xtext)
{
	gdouble value = 0;
	xtext_buffer *buf = xtext->buffer;
	textentry *ent = buf->text_first;
	GtkAdjustment *adj = xtext->adj;

	if (buf->marker_pos == NULL)
		return buf->marker_state;

	if (gtk_xtext_check_ent_visibility (xtext, buf->marker_pos, 1) == FALSE)
	{
		while (ent)
		{
			if (ent == buf->marker_pos)
				break;
			value += g_slist_length (ent->sublines);
			ent = ent->next;
		}
		if (value >= xtext_adj_get_value (adj) &&
			value < xtext_adj_get_value (adj) + xtext_adj_get_page_size (adj))
			return MARKER_IS_SET;
		value -= xtext_adj_get_page_size (adj) / 2;
		if (value < 0)
			value = 0;
		if (value > xtext_adj_get_upper (adj) - xtext_adj_get_page_size (adj))
			value = xtext_adj_get_upper (adj) - xtext_adj_get_page_size (adj);
		xtext_adj_set_value (adj, value);
		gtk_xtext_render_page (xtext);
	}

	/* If we previously lost marker position to scrollback limit -- */
	if (buf->marker_pos == buf->text_first &&
		 buf->marker_state == MARKER_RESET_BY_KILL)
		return MARKER_RESET_BY_KILL;
	else
		return MARKER_IS_SET;
}

void
gtk_xtext_buffer_show (GtkXText *xtext, xtext_buffer *buf, int render)
{
	int w, h;
	GdkWindow *window;

	buf->xtext = xtext;

	if (xtext->buffer == buf)
		return;

/*printf("text_buffer_show: xtext=%p buffer=%p\n", xtext, buf);*/

	if (xtext->add_io_tag)
	{
		g_source_remove (xtext->add_io_tag);
		xtext->add_io_tag = 0;
	}

	if (xtext->io_tag)
	{
		g_source_remove (xtext->io_tag);
		xtext->io_tag = 0;
	}

	if (!gtk_widget_get_realized (GTK_WIDGET (xtext)))
		gtk_widget_realize (GTK_WIDGET (xtext));

#if HAVE_GTK3
	window = gtk_widget_get_window (GTK_WIDGET (xtext));
#endif
#if !HAVE_GTK3
	window = GTK_WIDGET (xtext)->window;
#endif
	if (!window)
		return;
	h = gdk_window_get_height (window);
	w = gdk_window_get_width (window);

	/* after a font change */
	if (buf->needs_recalc)
	{
		buf->needs_recalc = FALSE;
		gtk_xtext_recalc_widths (buf, TRUE);
	}

	/* now change to the new buffer */
	xtext->buffer = buf;
	dontscroll (buf);	/* force scrolling off */
	{
		gdouble lower = 0;
		gdouble value = buf->old_value;
		gdouble upper = buf->num_lines;
		gdouble page_size = xtext_adj_get_page_size (xtext->adj);

		/* if the scrollbar was down, keep it down */
		if (xtext->buffer->scrollbar_down && value < upper - page_size)
			value = upper - page_size;

		if (upper == 0)
			upper = 1;
		/* sanity check */
		else if (value > upper - page_size)
		{
			/*buf->pagetop_ent = NULL;*/
			value = upper - page_size;
			if (value < 0)
				value = 0;
		}

		xtext_adjustment_apply (xtext->adj, lower, upper, value, page_size);
	}

	if (render)
	{
		/* did the window change size since this buffer was last shown? */
		if (buf->window_width != w)
		{
			buf->window_width = w;
			buf->window_height = h;
			gtk_xtext_calc_lines (buf, FALSE);
			if (buf->scrollbar_down)
				xtext_adj_set_value (xtext->adj,
					xtext_adj_get_upper (xtext->adj) -
					xtext_adj_get_page_size (xtext->adj));
		} else if (buf->window_height != h)
		{
			buf->window_height = h;
			buf->pagetop_ent = NULL;
			if (buf->scrollbar_down)
				xtext_adj_set_value (xtext->adj,
					xtext_adj_get_upper (xtext->adj));
			gtk_xtext_adjustment_set (buf, FALSE);
		}

		gtk_xtext_render_page (xtext);
#if HAVE_GTK3
		{
			GtkAllocation allocation;
			gdouble lower = 0;
			gdouble upper = buf->num_lines;
			gdouble value = gtk_adjustment_get_value (xtext->adj);
			gdouble page_size;

			if (upper == 0)
				upper = 1;

			gtk_widget_get_allocation (GTK_WIDGET (xtext), &allocation);
			page_size = allocation.height / xtext->fontsize;

			if (value > upper - page_size)
			{
				buf->scrollbar_down = TRUE;
				value = upper - page_size;
			}

			if (value < 0)
				value = 0;

			xtext_adjustment_apply (xtext->adj, lower, upper, value, page_size);
			gtk_adjustment_set_page_increment (xtext->adj, page_size);
			gtk_adjustment_value_changed (xtext->adj);
		}
#else
		gtk_adjustment_changed (xtext->adj);
#endif
	}
}

xtext_buffer *
gtk_xtext_buffer_new (GtkXText *xtext)
{
	xtext_buffer *buf;

	buf = g_new0 (xtext_buffer, 1);
	buf->old_value = -1;
	buf->xtext = xtext;
	buf->scrollbar_down = TRUE;
	buf->indent = xtext->space_width * 2;
	dontscroll (buf);

	return buf;
}

void
gtk_xtext_buffer_free (xtext_buffer *buf)
{
	textentry *ent, *next;

	if (buf->xtext->buffer == buf)
		buf->xtext->buffer = buf->xtext->orig_buffer;

	if (buf->xtext->selection_buffer == buf)
		buf->xtext->selection_buffer = NULL;

	if (buf->search_found)
	{
		gtk_xtext_search_fini (buf);
	}

	ent = buf->text_first;
	while (ent)
	{
		next = ent->next;
		g_free (ent);
		ent = next;
	}

	g_free (buf);
}
