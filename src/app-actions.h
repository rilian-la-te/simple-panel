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


#ifndef APPACTIONS_H
#define APPACTIONS_H

#include <gtk/gtk.h>
#include "app.h"

G_BEGIN_DECLS
void activate_about(GSimpleAction* simple, GVariant* param, gpointer data);
void activate_run(GSimpleAction* simple, GVariant* param, gpointer data);
void activate_exit(GSimpleAction* simple, GVariant* param, gpointer data);
void activate_logout(GSimpleAction* simple, GVariant* param, gpointer data);
void activate_shutdown(GSimpleAction* simple, GVariant* param, gpointer data);
void activate_terminal(GSimpleAction* simple, GVariant* param, gpointer data);
GSettings *load_global_config_gsettings(PanelApp* app, GSettingsBackend **config);
void apply_styling(PanelApp* app);

G_END_DECLS

#endif // APPACTIONS_H
