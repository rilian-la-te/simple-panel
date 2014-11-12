/*
 * Copyright (c) 2014 LxDE Developers, see the file AUTHORS for details.
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

#ifndef __CONF_GSETTINGS_H__
#define __CONF_GSETTINGS_H__ 1

#define DEFAULT_PLUGIN_SETTINGS_ID "org.simple.panel.toplevel.plugin"
#define DEFAULT_TOPLEVEL_SETTINGS_ID "org.simple.panel.toplevel"
#define DEFAULT_TOPLEVEL_PATH "/org/simple/panel/toplevel/"
#define DEFAULT_ROOT_NAME "toplevel-settings"

#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <glib-object.h>

G_BEGIN_DECLS

struct _PanelGSettings
{
    GObject parent;
    GSettingsBackend* config_file_backend;
    gchar* config_file_name;
    GSettings* toplevel_settings;
    GSList* all_settings;
    gchar* root_path;
    gchar* root_name;
};

struct _PluginGSettings
{
    GObject parent;
    GSettings* config_settings;
    GSettings* default_settings;
    gchar* config_path_appender;
    gint plugin_number;
};



typedef struct _PanelGSettings PanelGSettings;
typedef struct _PluginGSettings PluginGSettings;

void panel_gsettings_add_plugin_settings(PanelGSettings* settings, const gchar* plugin_name, gint plugin_number);
void panel_gsettings_remove_plugin_settings(PanelGSettings* settings, gint plugin_number);
PanelGSettings*  panel_gsettings_create(const gchar* filename);
void panel_gsettings_free(PanelGSettings* settings);
void panel_gsettings_remove_config_file (PanelGSettings* settings);


G_END_DECLS

#endif /* __CONF_GSETTINGS_H__ */
