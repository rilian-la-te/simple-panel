/*
 * Copyright (c) 2014 LxDE Developers, see the file AUTHORS for details.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>
#include <gtk/gtk.h>
#include "private.h"

#include "conf-gsettings.h"

PanelGSettings*  panel_gsettings_create_custom
(const gchar* filename,const gchar* id, const gchar* path, const gchar* root_name);

void plugin_gsettings_remove (PluginGSettings* settings)
{
    g_free(settings->config_path_appender);
    g_object_unref(settings->config_settings);
    g_object_unref(settings->default_settings);
    g_free(settings);
}

void panel_gsettings_add_plugin_settings(PanelGSettings* settings, const gchar* plugin_name, gint plugin_number)
{
    PluginGSettings* new_settings = g_new0(PluginGSettings,1);
    new_settings->plugin_number = plugin_number;
    new_settings->config_path_appender=g_strdup_printf("%d",plugin_number);
    char* path = g_strdup_printf("%s%d/",settings->root_path,plugin_number);
    char* id = g_strdup_printf("%s.%s",settings->root_path,plugin_name);
    new_settings->config_settings = g_settings_new_with_backend_and_path(
                id,
                settings->config_file_backend,
                path);
    new_settings->default_settings = g_settings_new_with_backend_and_path(
                DEFAULT_PLUGIN_SETTINGS_ID,
                settings->config_file_backend,
                path);
    settings->all_settings = g_slist_append(settings->all_settings,new_settings);
}

void panel_gsettings_remove_plugin_settings(PanelGSettings* settings, gint plugin_number)
{
    GSList* l;
    PluginGSettings* removed_settings;
    gchar* group_name = g_strdup_printf("%d",plugin_number);
    for(l = settings->all_settings; l!=NULL; l=l->next)
    {
        removed_settings = l->data;
        if (plugin_number = removed_settings->plugin_number)
        {
            g_free(removed_settings->config_path_appender);
            g_object_unref(removed_settings->config_settings);
            g_object_unref(removed_settings->default_settings);
            break;
        }
        removed_settings = NULL;
    }
    if (removed_settings)
    {
        GKeyFile* keyfile = g_key_file_new();
        g_key_file_load_from_file(keyfile,settings->config_file_name,G_KEY_FILE_KEEP_COMMENTS,NULL);
        if (g_key_file_has_group(keyfile,group_name))
        {
            g_key_file_remove_group(keyfile,group_name,NULL);
            g_key_file_save_to_file(keyfile,settings->config_file_name,NULL);
        }
        g_key_file_free(keyfile);
        g_free(group_name);
    }
}

PanelGSettings*  panel_gsettings_create(const gchar* filename)
{
    return panel_gsettings_create_custom(filename,
                                         DEFAULT_TOPLEVEL_SETTINGS_ID,
                                         DEFAULT_TOPLEVEL_PATH,
                                         DEFAULT_ROOT_NAME);
}


PanelGSettings*  panel_gsettings_create_custom
(const gchar* filename,const gchar* id, const gchar* path, const gchar* root_name)
{
    PanelGSettings* new_settings = g_new0(PanelGSettings,1);
    new_settings->config_file_name=g_strdup(filename);
    new_settings->root_name = g_strdup(id);
    new_settings->root_path = g_strdup(path);
    new_settings->all_settings = NULL;
    new_settings->config_file_backend=g_keyfile_settings_backend_new(
                filename,new_settings->root_path,
                root_name);
    new_settings->toplevel_settings = g_settings_new_with_backend_and_path(
                new_settings->root_name,
                new_settings->config_file_backend,
                new_settings->root_path);
    return new_settings;
}

void panel_gsettings_free(PanelGSettings* settings)
{
    g_slist_free_full(settings->all_settings,(GDestroyNotify)plugin_gsettings_remove);
    g_object_unref(settings->toplevel_settings);
    g_object_unref(settings->config_file_backend);
    g_free(settings->root_name);
    g_free(settings->config_file_name);
    g_free(settings->root_path);
}

void panel_gsettings_remove_config_file (PanelGSettings* settings)
{
    g_slist_free_full(settings->all_settings,(GDestroyNotify)plugin_gsettings_remove);
    g_object_unref(settings->toplevel_settings);
    g_object_unref(settings->config_file_backend);
    g_free(settings->root_name);
    g_free(settings->root_path);
    GFile* f;
    f= g_file_new_for_path(settings->config_file_name);
    g_file_delete(f,NULL,NULL);
    g_object_unref(f);
    g_free(settings->config_file_name);
}

