/**
 * Copyright (c) 2006-2014 LxDE Developers, see the file AUTHORS for details.
 * Copyright (c) 2014 Konstantin Pugin.
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

#ifndef GMENUMAKER_H
#define GMENUMAKER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

GMenuModel* do_applications_menumodel();
GMenuModel* do_places_menumodel();
GMenuModel* do_system_menumodel();
GMenuModel* create_default_menumodel(gboolean as_submenus, const gchar *icon_str);
gchar* g_menu_make_xml (GMenuModel *model);

G_END_DECLS

#endif // GMENUMAKER_H

