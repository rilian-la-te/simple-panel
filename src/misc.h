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

void get_button_spacing(GtkRequisition *req, GtkContainer *parent, gchar *name);
guint32 gcolor2rgb24(GdkRGBA *color);
GtkWidget *lxpanel_button_new_for_icon(SimplePanel *panel, const gchar *name, GdkRGBA *color, const gchar *label);
GtkWidget *lxpanel_button_new_for_fm_icon(SimplePanel *panel, FmIcon *icon, GdkRGBA *color, const gchar *label);
void lxpanel_button_set_icon(GtkWidget* btn, const gchar *name, gint size);
void lxpanel_button_update_icon(GtkWidget* btn, FmIcon *icon, gint size);

G_END_DECLS

#endif
