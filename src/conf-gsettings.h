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

#define DEFAULT_PLUGIN_NAME_KEY "plugin-type"
#define DEFAULT_PLUGIN_SCHEMA_KEY "has-schema"
#define DEFAULT_PLUGIN_KEY_EXPAND "is-expanded"
#define DEFAULT_PLUGIN_KEY_CAN_EXPAND "can-expand"
#define DEFAULT_PLUGIN_KEY_PADDING "padding"
#define DEFAULT_PLUGIN_KEY_BORDER "border"
#define DEFAULT_PLUGIN_KEY_POSITION "position"


#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <glib-object.h>

G_BEGIN_DECLS

struct _PanelGSettings
{
    GSettingsBackend* config_file_backend;
    char* config_file_name;
    GSettings* toplevel_settings;
    GSList* all_settings;
    char* root_path;
    char* root_name;
};

struct _PluginGSettings
{
    GSettings* config_settings;
    GSettings* default_settings;
    char* config_path_appender;
    gint64 plugin_number;
};



typedef struct _PanelGSettings PanelGSettings;
typedef struct _PluginGSettings PluginGSettings;

PluginGSettings *panel_gsettings_add_plugin_settings(PanelGSettings* settings,
                                         const char* plugin_name,
                                         gint64 plugin_number);
void panel_gsettings_remove_plugin_settings(PanelGSettings* settings, gint64 plugin_number);
PanelGSettings*  panel_gsettings_create(const gchar* filename);
void panel_gsettings_free(PanelGSettings* settings, gboolean remove);
gboolean panel_gsettings_init_plugin_list(PanelGSettings* settings);
gint64 panel_gsettings_find_free_num (PanelGSettings* settings);
void plugin_gsettings_config_init(PanelGSettings* panel, PluginGSettings* settings, gboolean has_schema);

G_END_DECLS

#endif /* __CONF_GSETTINGS_H__ */
