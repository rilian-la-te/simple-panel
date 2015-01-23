/*
 * Code from libwnck
 */
/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Kim Woelders
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2003, 2005-2007 Vincent Untz
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#include "tasklist.h"
#include "tasklist-dnd.h"

static void
wnck_drag_clean_up (WnckWindow     *window,
		    GdkDragContext *context,
		    gboolean	    clean_up_for_context_destroy,
		    gboolean	    clean_up_for_window_destroy);

static void
wnck_drag_context_destroyed (gpointer  windowp,
                             GObject  *context)
{
  wnck_drag_clean_up (windowp, (GdkDragContext *) context, TRUE, FALSE);
}

static void
wnck_update_drag_icon (WnckWindow     *window,
		       GdkDragContext *context)
{
  gint org_w, org_h, dnd_w, dnd_h;
  WnckWorkspace *workspace;
  GdkRectangle rect;
  cairo_surface_t *surface;
  GtkWidget *widget;
  cairo_t *cr;

  widget = g_object_get_data (G_OBJECT (context), "wnck-drag-source-widget");
  if (!widget)
    return;

  if (!gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (widget),
					  GTK_ICON_SIZE_DND, &dnd_w, &dnd_h))
    dnd_w = dnd_h = 32;
  /* windows are huge, so let's make this huge */
  dnd_w *= 3;

  workspace = wnck_window_get_workspace (window);
  if (workspace == NULL)
    workspace = wnck_screen_get_active_workspace (wnck_window_get_screen (window));
  if (workspace == NULL)
    return;

  wnck_window_get_geometry (window, NULL, NULL, &org_w, &org_h);

  rect.x = rect.y = 0;
  rect.width = 0.5 + ((double) (dnd_w * org_w) / (double) wnck_workspace_get_width (workspace));
  rect.width = MIN (org_w, rect.width);
  rect.height = 0.5 + ((double) (rect.width * org_h) / (double) org_w);

  /* we need at least three pixels to draw the smallest window */
  rect.width = MAX (rect.width, 3);
  rect.height = MAX (rect.height, 3);

  surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               rect.width, rect.height);
  cr = cairo_create (surface);
  draw_window (cr, widget, window, &rect, GTK_STATE_FLAG_NORMAL, FALSE);
  cairo_destroy (cr);
  cairo_surface_set_device_offset (surface, 2, 2);

  gtk_drag_set_icon_surface (context, surface);

  cairo_surface_destroy (surface);
}

static void
wnck_drag_window_destroyed (gpointer  contextp,
                            GObject  *window)
{
  wnck_drag_clean_up ((WnckWindow *) window, GDK_DRAG_CONTEXT (contextp),
                      FALSE, TRUE);
}

static void
wnck_drag_source_destroyed (gpointer  contextp,
                            GObject  *drag_source)
{
  g_object_steal_data (G_OBJECT (contextp), "wnck-drag-source-widget");
}

/* CAREFUL: This function is a bit brittle, because the pointers given may be
 * finalized already */
static void
wnck_drag_clean_up (WnckWindow     *window,
		    GdkDragContext *context,
		    gboolean	    clean_up_for_context_destroy,
		    gboolean	    clean_up_for_window_destroy)
{
  if (clean_up_for_context_destroy)
    {
      GtkWidget *drag_source;

      drag_source = g_object_get_data (G_OBJECT (context),
                                       "wnck-drag-source-widget");
      if (drag_source)
        g_object_weak_unref (G_OBJECT (drag_source),
                             wnck_drag_source_destroyed, context);

      g_object_weak_unref (G_OBJECT (window),
                           wnck_drag_window_destroyed, context);
      if (g_signal_handlers_disconnect_by_func (window,
                                                wnck_update_drag_icon, context) != 2)
	g_assert_not_reached ();
    }

  if (clean_up_for_window_destroy)
    {
      g_object_steal_data (G_OBJECT (context), "wnck-drag-source-widget");
      g_object_weak_unref (G_OBJECT (context),
                           wnck_drag_context_destroyed, window);
    }
}

void
_wnck_window_set_as_drag_icon (WnckWindow     *window,
                               GdkDragContext *context,
                               GtkWidget      *drag_source)
{
  g_return_if_fail (WNCK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));

  g_object_weak_ref (G_OBJECT (window), wnck_drag_window_destroyed, context);
  g_signal_connect (window, "geometry_changed",
                    G_CALLBACK (wnck_update_drag_icon), context);
  g_signal_connect (window, "icon_changed",
                    G_CALLBACK (wnck_update_drag_icon), context);

  g_object_set_data (G_OBJECT (context), "wnck-drag-source-widget", drag_source);
  g_object_weak_ref (G_OBJECT (drag_source), wnck_drag_source_destroyed, context);

  g_object_weak_ref (G_OBJECT (context), wnck_drag_context_destroyed, window);

  wnck_update_drag_icon (window, context);
}

