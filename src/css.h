/*
 * fb-background-monitor.h:
 *
 * Copyright (C) 2001, 2002 Ian McKellar <yakk@yakk.net>
 *                     2002 Sun Microsystems, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Authors:
 *      Ian McKellar <yakk@yakk.net>
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __FB_BG_H__
#define __FB_BG_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include "panel.h"

inline void css_apply_with_class (GtkWidget* widget,const gchar* css, gchar* klass ,gboolean remove);
inline gchar* css_generate_background(const char *filename, GdkRGBA color,gboolean no_image);
inline gchar* css_generate_panel_icon_button(GdkRGBA color);
inline gchar* css_generate_font_color(GdkRGBA color);
inline gchar* css_generate_font_size(gint size);
inline gchar* css_generate_font_label(gfloat size, gboolean is_bold);
inline gchar* css_apply_from_file (GtkWidget* widget, gchar* file);
inline gchar* css_apply_from_file_to_app (gchar* file);
inline gchar* css_generate_flat_button(GtkWidget* widget,SimplePanel* panel);

#endif /* __FB_BG_H__ */
