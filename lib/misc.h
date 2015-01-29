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

#define simple_panel_image_change_gicon(img, icon, p) \
	gtk_image_set_from_gicon ( GTK_IMAGE(img) , (icon), GTK_ICON_SIZE_INVALID)

#define simple_panel_image_change_icon(img, name, p) {\
	GIcon* __icon__ = g_icon_new_for_string(name,NULL); 	\
	gtk_image_set_from_gicon ( GTK_IMAGE(img) , __icon__, GTK_ICON_SIZE_INVALID); \
	g_object_unref(__icon__);}

#define simple_panel_button_set_icon(btn,name,p,size) {\
	GIcon* __icon__ = g_icon_new_for_string(name,NULL); 	\
	GtkImage* __img__ = GTK_IMAGE(gtk_button_get_image(GTK_BUTTON(btn))); \
	gtk_image_set_from_gicon ( GTK_IMAGE(__img__) , (__icon__), GTK_ICON_SIZE_INVALID); \
	g_object_unref(__icon__);}

#define expand_tilda(file)\
	( (file) [0] == '~') ? \
			g_strdup_printf("%s%s", getenv("HOME"), (file)+1) \
			: g_strdup(file)

G_BEGIN_DECLS

GtkWidget *simple_panel_button_new_for_icon(SimplePanel *panel, const gchar *name, GdkRGBA *color, const gchar *label);
GtkWidget* simple_panel_image_new_for_icon(SimplePanel * p,const gchar *name, gint height);
GtkWidget* simple_panel_image_new_for_gicon(SimplePanel * p,GIcon *icon, gint height);
void activate_menu(GSimpleAction* simple, GVariant* param, gpointer data);
void activate_panel_preferences(GSimpleAction* simple, GVariant* param, gpointer data);

G_END_DECLS

#endif
