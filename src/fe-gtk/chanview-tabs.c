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

/* file included in chanview.c */

typedef struct
{
	GtkWidget *outer;	/* outer box */
	GtkWidget *inner;	/* inner box */
	GtkWidget *b1;		/* button1 */
	GtkWidget *b2;		/* button2 */
} tabview;

static void chanview_populate (chanview *cv);

/* ignore "toggled" signal? */
static int ignore_toggle = FALSE;
static int tab_left_is_moving = 0;
static int tab_right_is_moving = 0;

typedef struct
{
	GtkAdjustment *adj;
	gdouble current_value;
	gdouble target_value;
	gint direction;
	gdouble step_size;
	guint source_id;
	int *moving_flag;
	gboolean is_left;
} tab_scroll_animation;

static tab_scroll_animation *tab_left_animation;
static tab_scroll_animation *tab_right_animation;

/* userdata for gobjects used here:
 *
 * tab (togglebuttons inside boxes):
 *   "u" userdata passed to tab-focus callback function (sess)
 *   "c" the tab's (chan *)
 *
 * box (family box)
 *   "f" family
 *
 */

static inline gint
cv_tabs_get_viewport_size (GdkWindow *parent_win, gboolean vertical)
{
	gint viewport_size = 0;

	if (vertical)
		viewport_size = gdk_window_get_height (parent_win);
	else
		viewport_size = gdk_window_get_width (parent_win);

	return viewport_size;
}

static void
cv_tabs_sizealloc (GtkWidget *widget, GtkAllocation *allocation, chanview *cv)
{
	GdkWindow *parent_win;
	GtkAdjustment *adj;
	GtkWidget *inner;
	gint viewport_size;

	inner = ((tabview *)cv)->inner;
	parent_win = gtk_widget_get_window (gtk_widget_get_parent (inner));

	if (cv->vertical)
	{
		adj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (gtk_widget_get_parent (inner)));
	} else
	{
		adj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (gtk_widget_get_parent (inner)));
	}

	viewport_size = cv_tabs_get_viewport_size (parent_win, cv->vertical);

	if (gtk_adjustment_get_upper (adj) <= viewport_size)
	{
		gtk_widget_hide (((tabview *)cv)->b1);
		gtk_widget_hide (((tabview *)cv)->b2);
	} else
	{
		gtk_widget_show (((tabview *)cv)->b1);
		gtk_widget_show (((tabview *)cv)->b2);
	}
}

static gint
tab_search_offset (GtkWidget *inner, gint start_offset,
				   gboolean forward, gboolean vertical)
{
	GList *boxes;
	GList *tabs;
	GtkWidget *box;
	GtkWidget *button;
	GtkAllocation allocation;
	gint found;

	boxes = gtk_container_get_children (GTK_CONTAINER (inner));
	if (!forward && boxes)
		boxes = g_list_last (boxes);

	while (boxes)
	{
		box = (GtkWidget *)boxes->data;
		boxes = (forward ? boxes->next : boxes->prev);

		tabs = gtk_container_get_children (GTK_CONTAINER (box));
		if (!forward && tabs)
			tabs = g_list_last (tabs);

		while (tabs)
		{
			button = (GtkWidget *)tabs->data;
			tabs = (forward ? tabs->next : tabs->prev);

			if (!GTK_IS_TOGGLE_BUTTON (button))
				continue;

			gtk_widget_get_allocation (button, &allocation);
			found = (vertical ? allocation.y : allocation.x);
			if ((forward && found > start_offset) ||
				(!forward && found < start_offset))
				return found;
		}
	}

	return 0;
}

static gboolean
tab_scroll_animation_tick (gpointer userdata)
{
	tab_scroll_animation *animation = userdata;
	gboolean reached_target;

	animation->current_value += animation->step_size * animation->direction;

	if (animation->direction < 0)
		reached_target = animation->current_value <= animation->target_value;
	else
		reached_target = animation->current_value >= animation->target_value;

	if (reached_target)
		animation->current_value = animation->target_value;

	gtk_adjustment_set_value (animation->adj, animation->current_value);

	if (!reached_target)
		return G_SOURCE_CONTINUE;

	*animation->moving_flag = 0;
	animation->source_id = 0;

	if (animation->is_left)
		tab_left_animation = NULL;
	else
		tab_right_animation = NULL;

	g_object_unref (animation->adj);
	g_free (animation);

	return G_SOURCE_REMOVE;
}

static void
tab_scroll_animation_cancel (tab_scroll_animation **animation)
{
	if (*animation == NULL)
		return;

	if ((*animation)->source_id != 0)
		g_source_remove ((*animation)->source_id);

	*(*animation)->moving_flag = 0;
	g_object_unref ((*animation)->adj);
	g_free (*animation);
	*animation = NULL;
}

static void
tab_scroll_animation_start (tab_scroll_animation **slot, GtkAdjustment *adj,
							gdouble current_value, gdouble target_value,
							gint direction, int *moving_flag,
							gboolean is_left)
{
	tab_scroll_animation *animation;
	gdouble distance;
	gdouble frames;

	distance = target_value - current_value;
	if (distance < 0.0)
		distance = -distance;

	if (distance <= 0.0)
	{
		gtk_adjustment_set_value (adj, target_value);
		*moving_flag = 0;
		return;
	}

	animation = g_new0 (tab_scroll_animation, 1);
	animation->adj = g_object_ref (adj);
	animation->current_value = current_value;
	animation->target_value = target_value;
	animation->direction = direction;
	frames = 12.0;
	animation->step_size = distance / MAX (1.0, frames);
	animation->moving_flag = moving_flag;
	animation->is_left = is_left;

	*moving_flag = 1;
	animation->source_id = g_timeout_add (16, tab_scroll_animation_tick, animation);
	*slot = animation;
}

static void
tab_scroll_left_up_clicked (GtkWidget *widget, chanview *cv)
{
	GtkAdjustment *adj;
	gint viewport_size;
	gfloat new_value;
	GtkWidget *inner;
	GdkWindow *parent_win;

	inner = ((tabview *)cv)->inner;
	parent_win = gtk_widget_get_window (gtk_widget_get_parent (inner));

	if (cv->vertical)
	{
		adj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (gtk_widget_get_parent(inner)));
	} else
	{
		adj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (gtk_widget_get_parent(inner)));
	}

	viewport_size = cv_tabs_get_viewport_size (parent_win, cv->vertical);

	new_value = tab_search_offset (inner, gtk_adjustment_get_value (adj), 0, cv->vertical);

	if (new_value + viewport_size > gtk_adjustment_get_upper (adj))
		new_value = gtk_adjustment_get_upper (adj) - viewport_size;

	if (tab_left_is_moving)
	{
		tab_scroll_animation_cancel (&tab_left_animation);
		return;
	}

	tab_scroll_animation_start (&tab_left_animation, adj,
							gtk_adjustment_get_value (adj), new_value,
							-1, &tab_left_is_moving, TRUE);
}

static void
tab_scroll_right_down_clicked (GtkWidget *widget, chanview *cv)
{
	GtkAdjustment *adj;
	gint viewport_size;
	gfloat new_value;
	GtkWidget *inner;
	GdkWindow *parent_win;

	inner = ((tabview *)cv)->inner;
	parent_win = gtk_widget_get_window (gtk_widget_get_parent (inner));

	if (cv->vertical)
	{
		adj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (gtk_widget_get_parent(inner)));
	} else
	{
		adj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (gtk_widget_get_parent(inner)));
	}

	viewport_size = cv_tabs_get_viewport_size (parent_win, cv->vertical);

	new_value = tab_search_offset (inner, gtk_adjustment_get_value (adj), 1, cv->vertical);

	if (new_value == 0 || new_value + viewport_size > gtk_adjustment_get_upper (adj))
		new_value = gtk_adjustment_get_upper (adj) - viewport_size;

	if (tab_right_is_moving)
	{
		tab_scroll_animation_cancel (&tab_right_animation);
		return;
	}

	tab_scroll_animation_start (&tab_right_animation, adj,
							gtk_adjustment_get_value (adj), new_value,
							1, &tab_right_is_moving, FALSE);
}

static gboolean
tab_scroll_cb (GtkWidget *widget, GdkEventScroll *event, gpointer cv)
{
	if (prefs.hex_gui_tab_scrollchans)
	{
		if (event->direction == GDK_SCROLL_DOWN)
			mg_switch_page (1, 1);
		else if (event->direction == GDK_SCROLL_UP)
			mg_switch_page (1, -1);
	}
	else
	{
		/* mouse wheel scrolling */
		if (event->direction == GDK_SCROLL_UP)
			tab_scroll_left_up_clicked (widget, cv);
		else if (event->direction == GDK_SCROLL_DOWN)
			tab_scroll_right_down_clicked (widget, cv);
	}

	return FALSE;
}

/* make a Scroll (arrow) button */

static GtkWidget *
make_sbutton (GtkArrowType type, void *click_cb, void *userdata)
{
	GtkWidget *button, *arrow;
	const char *icon_name = "pan-end-symbolic";

	button = gtk_button_new ();
	switch (type)
	{
	case GTK_ARROW_UP:
		icon_name = "pan-up-symbolic";
		break;
	case GTK_ARROW_DOWN:
		icon_name = "pan-down-symbolic";
		break;
	case GTK_ARROW_LEFT:
		icon_name = "pan-start-symbolic";
		break;
	case GTK_ARROW_RIGHT:
	default:
		icon_name = "pan-end-symbolic";
		break;
	}

	arrow = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (button), arrow);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	g_signal_connect (G_OBJECT (button), "clicked",
							G_CALLBACK (click_cb), userdata);
	g_signal_connect (G_OBJECT (button), "scroll-event",
							G_CALLBACK (tab_scroll_cb), userdata);
	gtk_widget_show (arrow);

	return button;
}

static void
cv_tabs_init (chanview *cv)
{
	GtkWidget *box, *hbox = NULL;
	GtkWidget *viewport;
	GtkWidget *outer;

	if (cv->vertical)
	{
		outer = gtkutil_box_new (GTK_ORIENTATION_VERTICAL, FALSE, 0);
	}
	else
	{
		outer = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 0);
	}
	((tabview *)cv)->outer = outer;
	g_signal_connect (G_OBJECT (outer), "size-allocate",
							G_CALLBACK (cv_tabs_sizealloc), cv);
	gtk_widget_show (outer);

	viewport = gtk_viewport_new (0, 0);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
	if (cv->vertical)
		gtk_widget_set_size_request (viewport, -1, 1);
	else
		gtk_widget_set_size_request (viewport, 1, -1);
	g_signal_connect (G_OBJECT (viewport), "scroll-event",
							G_CALLBACK (tab_scroll_cb), cv);
	gtk_box_pack_start (GTK_BOX (outer), viewport, 1, 1, 0);
	gtk_widget_show (viewport);

	if (cv->vertical)
	{
		box = gtkutil_box_new (GTK_ORIENTATION_VERTICAL, FALSE, 0);
	}
	else
	{
		box = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 0);
	}
	((tabview *)cv)->inner = box;
	gtk_container_add (GTK_CONTAINER (viewport), box);
	gtk_widget_show (box);

	/* if vertical, the buttons can be side by side */
	if (cv->vertical)
	{
		hbox = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (outer), hbox, 0, 0, 0);
		gtk_widget_show (hbox);
	}

	/* make the Scroll buttons */
	((tabview *)cv)->b2 = make_sbutton (cv->vertical ?
													GTK_ARROW_UP : GTK_ARROW_LEFT,
													tab_scroll_left_up_clicked,
													cv);

	((tabview *)cv)->b1 = make_sbutton (cv->vertical ?
													GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
													tab_scroll_right_down_clicked,
													cv);

	if (hbox)
	{
		gtk_container_add (GTK_CONTAINER (hbox), ((tabview *)cv)->b2);
		gtk_container_add (GTK_CONTAINER (hbox), ((tabview *)cv)->b1);
	} else
	{
		gtk_box_pack_start (GTK_BOX (outer), ((tabview *)cv)->b2, 0, 0, 0);
		gtk_box_pack_start (GTK_BOX (outer), ((tabview *)cv)->b1, 0, 0, 0);
	}

	gtk_container_add (GTK_CONTAINER (cv->box), outer);
}

static void
cv_tabs_postinit (chanview *cv)
{
}

static void
tab_add_sorted (chanview *cv, GtkWidget *box, GtkWidget *tab, chan *ch)
{
	GList *list;
	GtkWidget *child;
	int i = 0;
	void *b;

	if (!cv->sorted)
	{
		gtk_box_pack_start (GTK_BOX (box), tab, 0, 0, 0);
		gtk_widget_show (tab);
		return;
	}

	/* sorting TODO:
    *   - move tab if renamed (dialogs) */

	/* userdata, passed to mg_tabs_compare() */
	b = ch->userdata;

	list = gtk_container_get_children (GTK_CONTAINER (box));
	while (list)
	{
		child = list->data;
		if (!GTK_IS_SEPARATOR (child))
		{
			void *a = g_object_get_data (G_OBJECT (child), "u");

			if (ch->tag == 0 && cv->cb_compare (a, b) > 0)
			{
				gtk_box_pack_start (GTK_BOX (box), tab, 0, 0, 0);
				gtk_box_reorder_child (GTK_BOX (box), tab, ++i);
				gtk_widget_show (tab);
				return;
			}
		}
		i++;
		list = list->next;
	}

	/* append */
	gtk_box_pack_start (GTK_BOX (box), tab, 0, 0, 0);
	gtk_box_reorder_child (GTK_BOX (box), tab, i);
	gtk_widget_show (tab);
}

/* remove empty boxes and separators */

static void
cv_tabs_prune (chanview *cv)
{
	GList *boxes, *children;
	GtkWidget *box, *inner;
	GtkWidget *child;
	int empty;

	inner = ((tabview *)cv)->inner;
	boxes = gtk_container_get_children (GTK_CONTAINER (inner));
	while (boxes)
	{
		child = boxes->data;
		box = child;
		boxes = boxes->next;

		/* check if the box is empty (except a vseperator) */
		empty = TRUE;
		children = gtk_container_get_children (GTK_CONTAINER (box));
		while (children)
		{
			if (!GTK_IS_SEPARATOR ((GtkWidget *)children->data))
			{
				empty = FALSE;
				break;
			}
			children = children->next;
		}

		if (empty)
			gtk_widget_destroy (box);
	}
}

static void
tab_add_real (chanview *cv, GtkWidget *tab, chan *ch)
{
	GList *boxes, *children;
	GtkWidget *sep, *box, *inner;
	GtkWidget *child;
	int empty;

	inner = ((tabview *)cv)->inner;
	/* see if a family for this tab already exists */
	boxes = gtk_container_get_children (GTK_CONTAINER (inner));
	while (boxes)
	{
		child = boxes->data;
		box = child;

		if (g_object_get_data (G_OBJECT (box), "f") == ch->family)
		{
			tab_add_sorted (cv, box, tab, ch);
			gtk_widget_queue_resize (gtk_widget_get_parent(inner));
			return;
		}

		boxes = boxes->next;

		/* check if the box is empty (except a vseperator) */
		empty = TRUE;
		children = gtk_container_get_children (GTK_CONTAINER (box));
		while (children)
		{
			if (!GTK_IS_SEPARATOR ((GtkWidget *)children->data))
			{
				empty = FALSE;
				break;
			}
			children = children->next;
		}

		if (empty)
			gtk_widget_destroy (box);
	}

	/* create a new family box */
	if (cv->vertical)
	{
		/* vertical */
		box = gtkutil_box_new (GTK_ORIENTATION_VERTICAL, FALSE, 0);
		sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	} else
	{
		/* horiz */
		box = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 0);
		sep = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
	}

	gtk_box_pack_end (GTK_BOX (box), sep, 0, 0, 4);
	gtk_widget_show (sep);
	gtk_box_pack_start (GTK_BOX (inner), box, 0, 0, 0);
	g_object_set_data (G_OBJECT (box), "f", ch->family);
	gtk_box_pack_start (GTK_BOX (box), tab, 0, 0, 0);
	gtk_widget_show (tab);
	gtk_widget_show (box);
	gtk_widget_queue_resize (gtk_widget_get_parent(inner));
}

static gboolean
tab_ignore_cb (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data)
{
	return TRUE;
}

/* called when a tab is clicked (button down) */

static void
tab_pressed_cb (GtkToggleButton *tab, chan *ch)
{
	chan *old_tab;
	int is_switching = TRUE;
	chanview *cv = ch->cv;

	ignore_toggle = TRUE;
	/* de-activate the old tab */
	old_tab = cv->focused;
	if (old_tab && old_tab->impl)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (old_tab->impl), FALSE);
		if (old_tab == ch)
			is_switching = FALSE;
	}
	gtk_toggle_button_set_active (tab, TRUE);
	ignore_toggle = FALSE;
	cv->focused = ch;

	if (is_switching)
		/* call the focus callback */
		cv->cb_focus (cv, ch, ch->tag, ch->userdata);
}

/* called for keyboard tab toggles only */
static void
tab_toggled_cb (GtkToggleButton *tab, chan *ch)
{
	if (ignore_toggle)
		return;

	/* activated a tab via keyboard */
	tab_pressed_cb (tab, ch);
}

static gboolean
tab_click_cb (GtkWidget *wid, GdkEventButton *event, chan *ch)
{
	GtkWidget *close_button;
	gint close_x;
	gint close_y;
	GtkAllocation close_alloc;

	if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
	{
		close_button = g_object_get_data (G_OBJECT (wid), "tab-close-button");
		if (close_button &&
			gtk_widget_translate_coordinates (close_button, wid, 0, 0, &close_x, &close_y))
		{
			gtk_widget_get_allocation (close_button, &close_alloc);
			if (event->x >= close_x && event->x < close_x + close_alloc.width &&
				event->y >= close_y && event->y < close_y + close_alloc.height)
			{
				ch->cv->cb_xbutton (ch->cv, ch, ch->tag, ch->userdata);
				return TRUE;
			}
		}
	}

	return ch->cv->cb_contextmenu (ch->cv, ch, ch->tag, ch->userdata, event);
}

static gboolean
tab_close_motion_cb (GtkWidget *wid, GdkEventMotion *event, chan *ch)
{
	GtkWidget *close_button;
	gint close_x;
	gint close_y;
	GtkAllocation close_alloc;
	gboolean hover = FALSE;

	close_button = g_object_get_data (G_OBJECT (wid), "tab-close-button");
	if (close_button &&
		gtk_widget_translate_coordinates (close_button, wid, 0, 0, &close_x, &close_y))
	{
		gtk_widget_get_allocation (close_button, &close_alloc);
		hover = event->x >= close_x && event->x < close_x + close_alloc.width &&
			event->y >= close_y && event->y < close_y + close_alloc.height;
	}

	if (hover)
	{
		GdkCursor *cursor;
		gtk_widget_set_state_flags (close_button, GTK_STATE_FLAG_PRELIGHT, TRUE);
		if (gtk_widget_get_window (wid))
		{
			cursor = gdk_cursor_new_for_display (gtk_widget_get_display (wid), GDK_HAND2);
			gdk_window_set_cursor (gtk_widget_get_window (wid), cursor);
			g_object_unref (cursor);
		}
	}
	else
	{
		gtk_widget_unset_state_flags (close_button, GTK_STATE_FLAG_PRELIGHT);
		if (gtk_widget_get_window (wid))
			gdk_window_set_cursor (gtk_widget_get_window (wid), NULL);
	}

	return FALSE;
}

static gboolean
tab_close_leave_cb (GtkWidget *wid, GdkEventCrossing *event, chan *ch)
{
	GtkWidget *close_button;

	close_button = g_object_get_data (G_OBJECT (wid), "tab-close-button");
	if (close_button)
		gtk_widget_unset_state_flags (close_button, GTK_STATE_FLAG_PRELIGHT);
	if (gtk_widget_get_window (wid))
		gdk_window_set_cursor (gtk_widget_get_window (wid), NULL);
	return FALSE;
}

static GtkWidget *
tab_get_label (GtkWidget *tab)
{
	GtkWidget *label;

	label = g_object_get_data (G_OBJECT (tab), "tab-label");
	if (label)
		return label;

	return gtk_bin_get_child (GTK_BIN (tab));
}

static void *
cv_tabs_add (chanview *cv, chan *ch, char *name, GtkTreeIter *parent)
{
	GtkWidget *but;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *close_button;
	GtkWidget *close_icon;

	but = gtk_toggle_button_new ();
	gtk_widget_set_name (but, "zoitechat-tab");
	gtk_widget_add_events (but, GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	label = gtk_label_new (name);
	close_button = gtk_button_new ();
	close_icon = gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
	gtk_button_set_always_show_image (GTK_BUTTON (close_button), TRUE);
	gtk_widget_set_can_focus (close_button, FALSE);
	gtk_container_add (GTK_CONTAINER (close_button), close_icon);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (but), hbox);
	g_object_set_data (G_OBJECT (but), "tab-label", label);
	g_object_set_data (G_OBJECT (but), "tab-close-button", close_button);
	g_object_set_data (G_OBJECT (but), "c", ch);
	/* used to trap right-clicks */
	g_signal_connect (G_OBJECT (but), "button-press-event",
						 	G_CALLBACK (tab_click_cb), ch);
	g_signal_connect (G_OBJECT (but), "motion-notify-event",
						 	G_CALLBACK (tab_close_motion_cb), ch);
	g_signal_connect (G_OBJECT (but), "leave-notify-event",
						 	G_CALLBACK (tab_close_leave_cb), ch);
	/* avoid prelights */
	g_signal_connect (G_OBJECT (but), "enter-notify-event",
						 	G_CALLBACK (tab_ignore_cb), NULL);
	g_signal_connect (G_OBJECT (but), "leave-notify-event",
						 	G_CALLBACK (tab_ignore_cb), NULL);
	g_signal_connect (G_OBJECT (but), "pressed",
							G_CALLBACK (tab_pressed_cb), ch);
	/* for keyboard */
	g_signal_connect (G_OBJECT (but), "toggled",
						 	G_CALLBACK (tab_toggled_cb), ch);
	g_object_set_data (G_OBJECT (but), "u", ch->userdata);
	gtk_widget_show_all (hbox);

	tab_add_real (cv, but, ch);

	return but;
}

/* traverse all the family boxes of tabs 
 *
 * A "group" is basically:
 * GtkV/HBox
 * `-GtkViewPort
 *   `-GtkV/HBox (inner box)
 *     `- GtkBox (family box)
 *        `- GtkToggleButton
 *        `- GtkToggleButton
 *        `- ...
 *     `- GtkBox
 *        `- GtkToggleButton
 *        `- GtkToggleButton
 *        `- ...
 *     `- ...
 *
 * */

static int
tab_group_for_each_tab (chanview *cv,
								int (*callback) (GtkWidget *tab, int num, int usernum),
								int usernum)
{
	GList *tabs;
	GList *boxes;
	GtkWidget *child;
	GtkBox *innerbox;
	int i;

	innerbox = (GtkBox *) ((tabview *)cv)->inner;
	boxes = gtk_container_get_children (GTK_CONTAINER (innerbox));
	i = 0;
	while (boxes)
	{
		child = boxes->data;
		tabs = gtk_container_get_children (GTK_CONTAINER (child));

		while (tabs)
		{
			child = tabs->data;

			if (!GTK_IS_SEPARATOR (child))
			{
				if (callback (child, i, usernum) != -1)
					return i;
				i++;
			}
			tabs = tabs->next;
		}

		boxes = boxes->next;
	}

	return i;
}

static int
tab_check_focus_cb (GtkWidget *tab, int num, int unused)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tab)))
		return num;

	return -1;
}

/* returns the currently focused tab number */

static int
tab_group_get_cur_page (chanview *cv)
{
	return tab_group_for_each_tab (cv, tab_check_focus_cb, 0);
}

static void
cv_tabs_focus (chan *ch)
{
	if (ch->impl)
	/* focus the new one (tab_pressed_cb defocuses the old one) */
		tab_pressed_cb (GTK_TOGGLE_BUTTON (ch->impl), ch);
}

static int
tab_focus_num_cb (GtkWidget *tab, int num, int want)
{
	if (num == want)
	{
		cv_tabs_focus (g_object_get_data (G_OBJECT (tab), "c"));
		return 1;
	}

	return -1;
}

static void
cv_tabs_change_orientation (chanview *cv)
{
	/* cleanup the old one */
	if (cv->func_cleanup)
		cv->func_cleanup (cv);

	/* now rebuild a new tabbar or tree */
	cv->func_init (cv);
	chanview_populate (cv);
}

/* switch to the tab number specified */

static void
cv_tabs_move_focus (chanview *cv, gboolean relative, int num)
{
	int i, max;

	if (relative)
	{
		max = cv->size;
		i = tab_group_get_cur_page (cv) + num;
		/* make it wrap around at both ends */
		if (i < 0)
			i = max - 1;
		if (i >= max)
			i = 0;
		tab_group_for_each_tab (cv, tab_focus_num_cb, i);
		return;
	}

	tab_group_for_each_tab (cv, tab_focus_num_cb, num);
}

static void
cv_tabs_remove (chan *ch)
{
	gtk_widget_destroy (ch->impl);
	ch->impl = NULL;

	cv_tabs_prune (ch->cv);
}

static void
cv_tabs_move (chan *ch, int delta)
{
	int i = 0;
	int pos = 0;
	GList *list;
	GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET (ch->impl));

	for (list = gtk_container_get_children (GTK_CONTAINER (parent)); list; list = list->next)
	{
		GtkWidget *child_entry;

		child_entry = list->data;
		if (child_entry == ch->impl)
			pos = i;

		/* keep separator at end to not throw off our count */
		if (GTK_IS_SEPARATOR (child_entry))
			gtk_box_reorder_child (GTK_BOX (parent), child_entry, -1);
		else
			i++;
	}

	pos = (pos - delta) % i;
	gtk_box_reorder_child (GTK_BOX (parent), ch->impl, pos);
}

static void
cv_tabs_move_family (chan *ch, int delta)
{
	int i, pos = 0;
	GList *list;
	GtkWidget *box = NULL;

	/* find position of tab's family */
	i = 0;
	for (list = gtk_container_get_children (GTK_CONTAINER (((tabview *)ch->cv)->inner)); list; list = list->next)
	{
		GtkWidget *child_entry;
		void *fam;

		child_entry = list->data;
		fam = g_object_get_data (G_OBJECT (child_entry), "f");
		if (fam == ch->family)
		{
			box = child_entry;
			pos = i;
		}
		i++;
	}

	pos = (pos - delta) % i;
	gtk_box_reorder_child (GTK_BOX (gtk_widget_get_parent(box)), box, pos);
}

static void
cv_tabs_cleanup (chanview *cv)
{
	if (cv->box)
		gtk_widget_destroy (((tabview *)cv)->outer);
}

static void
cv_tabs_set_color (chan *ch, PangoAttrList *list)
{
	gtk_label_set_attributes (GTK_LABEL (tab_get_label (ch->impl)), list);
}

static void
cv_tabs_rename (chan *ch, char *name)
{
	PangoAttrList *attr;
	GtkWidget *tab = ch->impl;

	attr = gtk_label_get_attributes (GTK_LABEL (tab_get_label (tab)));
	if (attr)
		pango_attr_list_ref (attr);

	gtk_label_set_text (GTK_LABEL (tab_get_label (tab)), name);
	gtk_widget_queue_resize (gtk_widget_get_parent(gtk_widget_get_parent(gtk_widget_get_parent(tab))));

	if (attr)
	{
		gtk_label_set_attributes (GTK_LABEL (tab_get_label (tab)), attr);
		pango_attr_list_unref (attr);
	}
}

static gboolean
cv_tabs_is_collapsed (chan *ch)
{
	return FALSE;
}

static chan *
cv_tabs_get_parent (chan *ch)
{
	return NULL;
}
