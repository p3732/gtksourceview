/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/* gtksourcespacedrawer.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2008, 2011, 2016 - Paolo Borelli <pborelli@gnome.org>
 * Copyright (C) 2008, 2010 - Ignacio Casal Quinteiro <icq@gnome.org>
 * Copyright (C) 2010 - Garret Regier
 * Copyright (C) 2013 - Arpad Borsos <arpad.borsos@googlemail.com>
 * Copyright (C) 2015, 2016 - Sébastien Wilmet <swilmet@gnome.org>
 * Copyright (C) 2016 - Christian Hergert <christian@hergert.me>
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "gtksourcespacedrawer.h"
#include "gtksourcebuffer.h"
#include "gtksourceiter.h"
#include "gtksourcestylescheme.h"
#include "gtksourcetag.h"

/*
#define ENABLE_PROFILE
*/
#undef ENABLE_PROFILE

struct _GtkSourceSpaceDrawerPrivate
{
	GtkSourceDrawSpacesFlags flags;
	GdkRGBA *color;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceSpaceDrawer, _gtk_source_space_drawer, G_TYPE_OBJECT)

static void
_gtk_source_space_drawer_finalize (GObject *object)
{
	GtkSourceSpaceDrawer *drawer = GTK_SOURCE_SPACE_DRAWER (object);

	if (drawer->priv->color != NULL)
	{
		gdk_rgba_free (drawer->priv->color);
	}

	G_OBJECT_CLASS (_gtk_source_space_drawer_parent_class)->finalize (object);
}

static void
_gtk_source_space_drawer_class_init (GtkSourceSpaceDrawerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = _gtk_source_space_drawer_finalize;
}

static void
_gtk_source_space_drawer_init (GtkSourceSpaceDrawer *drawer)
{
	drawer->priv = _gtk_source_space_drawer_get_instance_private (drawer);
}

GtkSourceSpaceDrawer *
_gtk_source_space_drawer_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_SPACE_DRAWER, NULL);
}

GtkSourceDrawSpacesFlags
_gtk_source_space_drawer_get_flags (GtkSourceSpaceDrawer *drawer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer), 0);

	return drawer->priv->flags;
}

gboolean
_gtk_source_space_drawer_set_flags (GtkSourceSpaceDrawer     *drawer,
				    GtkSourceDrawSpacesFlags  flags)
{
	gboolean changed = FALSE;

	g_return_val_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer), changed);

	if (drawer->priv->flags != flags)
	{
		drawer->priv->flags = flags;
		changed = TRUE;
	}

	return changed;
}

void
_gtk_source_space_drawer_update_color (GtkSourceSpaceDrawer *drawer,
				       GtkSourceView        *view)
{
	GtkSourceBuffer *buffer;
	GtkSourceStyleScheme *style_scheme;

	g_return_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer));
	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	if (!gtk_widget_get_realized (GTK_WIDGET (view)))
	{
		return;
	}

	if (drawer->priv->color != NULL)
	{
		gdk_rgba_free (drawer->priv->color);
		drawer->priv->color = NULL;
	}

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	style_scheme = gtk_source_buffer_get_style_scheme (buffer);

	if (style_scheme != NULL)
	{
		GtkSourceStyle *style;

		style = _gtk_source_style_scheme_get_draw_spaces_style (style_scheme);

		if (style != NULL)
		{
			gchar *color_str = NULL;
			gboolean color_set;
			GdkRGBA color;

			g_object_get (style,
				      "foreground", &color_str,
				      "foreground-set", &color_set,
				      NULL);

			if (color_set &&
			    color_str != NULL &&
			    gdk_rgba_parse (&color, color_str))
			{
				drawer->priv->color = gdk_rgba_copy (&color);
			}

			g_free (color_str);
		}
	}

	if (drawer->priv->color == NULL)
	{
		GtkStyleContext *context;
		GdkRGBA color;

		context = gtk_widget_get_style_context (GTK_WIDGET (view));
		gtk_style_context_save (context);
		gtk_style_context_set_state (context, GTK_STATE_FLAG_INSENSITIVE);
		gtk_style_context_get_color (context,
					     gtk_style_context_get_state (context),
					     &color);
		gtk_style_context_restore (context);

		drawer->priv->color = gdk_rgba_copy (&color);
	}
}

static inline gboolean
is_tab (gunichar c)
{
	return c == '\t';
}

static inline gboolean
is_nbsp (gunichar c)
{
	return g_unichar_break_type (c) == G_UNICODE_BREAK_NON_BREAKING_GLUE;
}

static inline gboolean
is_space (gunichar c)
{
	return g_unichar_type (c) == G_UNICODE_SPACE_SEPARATOR;
}

static inline gboolean
is_newline (const GtkTextIter *iter)
{
	/* TODO take into account implicit trailing newline. */
	return gtk_text_iter_ends_line (iter) && !gtk_text_iter_is_end (iter);
}

static inline gboolean
is_whitespace (gunichar c)
{
	return (g_unichar_isspace (c) || is_nbsp (c) || is_space (c));
}

static void
draw_space_at_pos (cairo_t      *cr,
		   GdkRectangle  rect)
{
	gint x, y;
	gdouble w;

	x = rect.x;
	y = rect.y + rect.height * 2 / 3;

	w = rect.width;

	cairo_save (cr);
	cairo_move_to (cr, x + w * 0.5, y);
	cairo_arc (cr, x + w * 0.5, y, 0.8, 0, 2 * G_PI);
	cairo_restore (cr);
}

static void
draw_tab_at_pos (cairo_t      *cr,
		 GdkRectangle  rect)
{
	gint x, y;
	gdouble w, h;

	x = rect.x;
	y = rect.y + rect.height * 2 / 3;

	w = rect.width;
	h = rect.height;

	cairo_save (cr);
	cairo_move_to (cr, x + w * 1 / 8, y);
	cairo_rel_line_to (cr, w * 6 / 8, 0);
	cairo_rel_line_to (cr, -h * 1 / 4, -h * 1 / 4);
	cairo_rel_move_to (cr, +h * 1 / 4, +h * 1 / 4);
	cairo_rel_line_to (cr, -h * 1 / 4, +h * 1 / 4);
	cairo_restore (cr);
}

static void
draw_newline_at_pos (cairo_t      *cr,
		     GdkRectangle  rect)
{
	gint x, y;
	gdouble w, h;

	x = rect.x;
	y = rect.y + rect.height / 3;

	w = 2 * rect.width;
	h = rect.height;

	cairo_save (cr);
	if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_LTR)
	{
		cairo_move_to (cr, x + w * 7 / 8, y);
		cairo_rel_line_to (cr, 0, h * 1 / 3);
		cairo_rel_line_to (cr, -w * 6 / 8, 0);
		cairo_rel_line_to (cr, +h * 1 / 4, -h * 1 / 4);
		cairo_rel_move_to (cr, -h * 1 / 4, +h * 1 / 4);
		cairo_rel_line_to (cr, +h * 1 / 4, +h * 1 / 4);
	}
	else
	{
		cairo_move_to (cr, x + w * 1 / 8, y);
		cairo_rel_line_to (cr, 0, h * 1 / 3);
		cairo_rel_line_to (cr, w * 6 / 8, 0);
		cairo_rel_line_to (cr, -h * 1 / 4, -h * 1 / 4);
		cairo_rel_move_to (cr, +h * 1 / 4, +h * 1 / 4);
		cairo_rel_line_to (cr, -h * 1 / 4, -h * 1 / 4);
	}

	cairo_restore (cr);
}

static void
draw_nbsp_at_pos (cairo_t      *cr,
		  GdkRectangle  rect,
		  gboolean      narrowed)
{
	gint x, y;
	gdouble w, h;

	x = rect.x;
	y = rect.y + rect.height / 2;

	w = rect.width;
	h = rect.height;

	cairo_save (cr);
	cairo_move_to (cr, x + w * 1 / 6, y);
	cairo_rel_line_to (cr, w * 4 / 6, 0);
	cairo_rel_line_to (cr, -w * 2 / 6, +h * 1 / 4);
	cairo_rel_line_to (cr, -w * 2 / 6, -h * 1 / 4);

	if (narrowed)
	{
		cairo_fill (cr);
	}
	else
	{
		cairo_stroke (cr);
	}

	cairo_restore (cr);
}

static void
draw_whitespace_at_iter (GtkTextView *text_view,
			 GtkTextIter *iter,
			 cairo_t     *cr)
{
	gunichar c;
	GdkRectangle rect;

	gtk_text_view_get_iter_location (text_view, iter, &rect);

	/* If the space is at a line-wrap position, or if the character is a
	 * newline, we get 0 width so we fallback to the height.
	 */
	if (rect.width == 0)
	{
		rect.width = rect.height;
	}

	c = gtk_text_iter_get_char (iter);

	if (is_tab (c))
	{
		draw_tab_at_pos (cr, rect);
	}
	else if (is_nbsp (c))
	{
		/* We also need to check if we want to draw a narrowed space */
		draw_nbsp_at_pos (cr, rect, c == 0x202F);
	}
	else if (is_space (c))
	{
		draw_space_at_pos (cr, rect);
	}
	else if (is_newline (iter))
	{
		draw_newline_at_pos (cr, rect);
	}
}

static void
draw_spaces_tag_foreach (GtkTextTag *tag,
			 gboolean   *found)
{
	if (*found)
	{
		return;
	}

	if (GTK_SOURCE_IS_TAG (tag))
	{
		gboolean draw_spaces_set;

		g_object_get (tag,
			      "draw-spaces-set", &draw_spaces_set,
			      NULL);

		if (draw_spaces_set)
		{
			*found = TRUE;
		}
	}
}

static gboolean
buffer_has_draw_spaces_tag (GtkTextBuffer *buffer)
{
	GtkTextTagTable *table;
	gboolean found = FALSE;

	table = gtk_text_buffer_get_tag_table (buffer);
	gtk_text_tag_table_foreach (table,
				    (GtkTextTagTableForeach) draw_spaces_tag_foreach,
				    &found);

	return found;
}

static void
space_needs_drawing_according_to_tag (const GtkTextIter *iter,
				      gboolean          *has_tag,
				      gboolean          *needs_drawing)
{
	GSList *tags;
	GSList *l;

	*has_tag = FALSE;
	*needs_drawing = FALSE;

	tags = gtk_text_iter_get_tags (iter);
	tags = g_slist_reverse (tags);

	for (l = tags; l != NULL; l = l->next)
	{
		GtkTextTag *tag = l->data;

		if (GTK_SOURCE_IS_TAG (tag))
		{
			gboolean draw_spaces_set;
			gboolean draw_spaces;

			g_object_get (tag,
				      "draw-spaces-set", &draw_spaces_set,
				      "draw-spaces", &draw_spaces,
				      NULL);

			if (draw_spaces_set)
			{
				*has_tag = TRUE;
				*needs_drawing = draw_spaces;
				break;
			}
		}
	}

	g_slist_free (tags);
}

static gboolean
space_needs_drawing_according_to_location (GtkSourceSpaceDrawer *drawer,
					   const GtkTextIter    *iter,
					   const GtkTextIter    *leading_end,
					   const GtkTextIter    *trailing_start)
{
	gint iter_locations = 0;
	gint allowed_locations = drawer->priv->flags & (GTK_SOURCE_DRAW_SPACES_LEADING |
							GTK_SOURCE_DRAW_SPACES_TEXT |
							GTK_SOURCE_DRAW_SPACES_TRAILING);

	/* Draw all by default */
	if (allowed_locations == 0)
	{
		return TRUE;
	}

	if (gtk_text_iter_compare (iter, leading_end) < 0)
	{
		iter_locations |= GTK_SOURCE_DRAW_SPACES_LEADING;
	}

	if (gtk_text_iter_compare (trailing_start, iter) <= 0)
	{
		iter_locations |= GTK_SOURCE_DRAW_SPACES_TRAILING;
	}

	/* Neither leading nor trailing, must be in text */
	if (iter_locations == 0)
	{
		iter_locations = GTK_SOURCE_DRAW_SPACES_TEXT;
	}

	return (iter_locations & allowed_locations) != 0;
}

static gboolean
space_needs_drawing_according_to_whitespace_type (GtkSourceSpaceDrawer *drawer,
						  const GtkTextIter    *iter)
{
	gunichar c;

	c = gtk_text_iter_get_char (iter);

	if (is_tab (c))
	{
		return drawer->priv->flags & GTK_SOURCE_DRAW_SPACES_TAB;
	}
	else if (is_nbsp (c))
	{
		return drawer->priv->flags & GTK_SOURCE_DRAW_SPACES_NBSP;
	}
	else if (is_space (c))
	{
		return drawer->priv->flags & GTK_SOURCE_DRAW_SPACES_SPACE;
	}
	else if (is_newline (iter))
	{
		return drawer->priv->flags & GTK_SOURCE_DRAW_SPACES_NEWLINE;
	}

	return FALSE;
}

static gboolean
space_needs_drawing (GtkSourceSpaceDrawer *drawer,
		     const GtkTextIter    *iter,
		     const GtkTextIter    *leading_end,
		     const GtkTextIter    *trailing_start)
{
	gboolean has_tag;
	gboolean needs_drawing;

	/* Check the GtkSourceTag:draw-spaces property (higher priority) */
	space_needs_drawing_according_to_tag (iter, &has_tag, &needs_drawing);
	if (has_tag)
	{
		return needs_drawing;
	}

	/* Check the flags */
	return (space_needs_drawing_according_to_location (drawer, iter, leading_end, trailing_start) &&
		space_needs_drawing_according_to_whitespace_type (drawer, iter));
}

static void
get_end_iter (GtkTextView *text_view,
	      GtkTextIter *start_iter,
	      GtkTextIter *end_iter,
	      gint         x,
	      gint         y,
	      gboolean     is_wrapping)
{
	gint min, max, i;
	GdkRectangle rect;

	*end_iter = *start_iter;
	if (!gtk_text_iter_ends_line (end_iter))
	{
		gtk_text_iter_forward_to_line_end (end_iter);
	}

	/* check if end_iter is inside the bounding box anyway */
	gtk_text_view_get_iter_location (text_view, end_iter, &rect);
	if (( is_wrapping && rect.y < y) ||
	    (!is_wrapping && rect.x < x))
	{
		return;
	}

	min = gtk_text_iter_get_line_offset (start_iter);
	max = gtk_text_iter_get_line_offset (end_iter);

	while (max >= min)
	{
		i = (min + max) >> 1;
		gtk_text_iter_set_line_offset (end_iter, i);
		gtk_text_view_get_iter_location (text_view, end_iter, &rect);

		if (( is_wrapping && rect.y < y) ||
		    (!is_wrapping && rect.x < x))
		{
			min = i + 1;
		}
		else if (( is_wrapping && rect.y > y) ||
			 (!is_wrapping && rect.x > x))
		{
			max = i - 1;
		}
		else
		{
			break;
		}
	}
}

void
_gtk_source_space_drawer_draw (GtkSourceSpaceDrawer *drawer,
			       GtkSourceView        *view,
			       cairo_t              *cr)
{
	GtkTextView *text_view;
	GtkTextBuffer *buffer;
	GdkRectangle clip;
	gint x1, y1, x2, y2;
	GtkTextIter start;
	GtkTextIter end;
	GtkTextIter leading_end;
	GtkTextIter trailing_start;
	GtkTextIter line_end;
	gboolean is_wrapping;

#ifdef ENABLE_PROFILE
	static GTimer *timer = NULL;
	if (timer == NULL)
	{
		timer = g_timer_new ();
	}

	g_timer_start (timer);
#endif

	g_return_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer));
	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));
	g_return_if_fail (cr != NULL);

	if (drawer->priv->color == NULL)
	{
		g_warning ("GtkSourceSpaceDrawer: color not set.");
		return;
	}

	text_view = GTK_TEXT_VIEW (view);
	buffer = gtk_text_view_get_buffer (text_view);

	if (drawer->priv->flags == 0 &&
	    !buffer_has_draw_spaces_tag (buffer))
	{
		return;
	}

	if (!gdk_cairo_get_clip_rectangle (cr, &clip))
	{
		return;
	}

	is_wrapping = gtk_text_view_get_wrap_mode (text_view) != GTK_WRAP_NONE;

	x1 = clip.x;
	y1 = clip.y;
	x2 = x1 + clip.width;
	y2 = y1 + clip.height;

	gtk_text_view_get_iter_at_location (text_view, &start, x1, y1);
	gtk_text_view_get_iter_at_location (text_view, &end, x2, y2);

	_gtk_source_iter_get_leading_spaces_end_boundary (&start, &leading_end);
	_gtk_source_iter_get_trailing_spaces_start_boundary (&start, &trailing_start);
	get_end_iter (text_view, &start, &line_end, x2, y2, is_wrapping);

	gdk_cairo_set_source_rgba (cr, drawer->priv->color);
	cairo_set_line_width (cr, 0.8);
	cairo_translate (cr, -0.5, -0.5);

	while (TRUE)
	{
		gunichar c = gtk_text_iter_get_char (&start);
		gint ly;

		if (is_whitespace (c) &&
		    space_needs_drawing (drawer, &start, &leading_end, &trailing_start))
		{
			draw_whitespace_at_iter (text_view, &start, cr);
		}

		if (!gtk_text_iter_forward_char (&start))
		{
			break;
		}

		if (gtk_text_iter_compare (&start, &line_end) > 0)
		{
			if (gtk_text_iter_compare (&start, &end) > 0)
			{
				break;
			}

			/* Move to the first iter in the exposed area of the
			 * next line.
			 */
			if (!gtk_text_iter_starts_line (&start) &&
			    !gtk_text_iter_forward_line (&start))
			{
				break;
			}

			gtk_text_view_get_line_yrange (text_view, &start, &ly, NULL);
			gtk_text_view_get_iter_at_location (text_view, &start, x1, ly);

			/* Move back one char otherwise tabs may not be redrawn. */
			if (!gtk_text_iter_starts_line (&start))
			{
				gtk_text_iter_backward_char (&start);
			}

			_gtk_source_iter_get_leading_spaces_end_boundary (&start, &leading_end);
			_gtk_source_iter_get_trailing_spaces_start_boundary (&start, &trailing_start);
			get_end_iter (text_view, &start, &line_end, x2, y2, is_wrapping);
		}
	};

	cairo_stroke (cr);

#ifdef ENABLE_PROFILE
	g_timer_stop (timer);

	/* Same indentation as similar features in gtksourceview.c. */
	g_print ("    %s time: %g (sec * 1000)\n",
		 G_STRFUNC,
		 g_timer_elapsed (timer, NULL) * 1000);
#endif
}