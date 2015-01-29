/**
 * Copyright (c) 2006-2014 LxDE Developers, see the file AUTHORS for details.
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __MISC_H__
#define __MISC_H__ 1

#include <gtk/gtk.h>

#include "panel.h"

G_BEGIN_DECLS

gchar *expand_tilda(const gchar *file);

GtkWidget *simple_panel_button_new_for_icon(SimplePanel *panel, const gchar *name, GdkRGBA *color, const gchar *label);
void simple_panel_button_set_icon(GtkWidget* btn, const gchar *name, SimplePanel* p, gint size);
GtkWidget* simple_panel_image_new_for_icon(SimplePanel * p,const gchar *name, gint height);
GtkWidget* simple_panel_image_new_for_gicon(SimplePanel * p,GIcon *icon, gint height);
void simple_panel_image_change_icon(GtkWidget* img, const gchar* name, SimplePanel* p);
void simple_panel_image_change_gicon(GtkWidget* img, GIcon* icon, SimplePanel* p);
void simple_panel_scale_button_set_range (GtkScaleButton* b, gint lower, gint upper);
void simple_panel_scale_button_set_value_labeled (GtkScaleButton* b, gint value);
void start_panels_from_dir(GtkApplication* app,const char *panel_dir);
void simple_panel_bind_gsettings(GObject* obj, GSettings* settings, const gchar* prop);
void activate_menu(GSimpleAction* simple, GVariant* param, gpointer data);
void activate_panel_preferences(GSimpleAction* simple, GVariant* param, gpointer data);
G_END_DECLS

#endif
