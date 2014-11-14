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
    if (settings->config_settings)
        g_object_unref(settings->config_settings);
    g_object_unref(settings->default_settings);
    g_free(settings);
}

inline static gint64 compare_int64(const gint64* a, const gint64* b)
{
    return *a-*b;
}

gint64 panel_gsettings_find_free_num (PanelGSettings* settings)
{
    GKeyFile* kf = g_key_file_new();
    gsize i,len;
    char** panel_config_groups;
    g_key_file_load_from_file(kf,settings->config_file_name,G_KEY_FILE_KEEP_COMMENTS,NULL);
    panel_config_groups = g_key_file_get_groups(kf,&len);
    GSList*l, *tmp = NULL;
    for (i = 0; i < len; i++)
    {
        if(g_strcmp0(DEFAULT_ROOT_NAME,panel_config_groups[i]))
            continue;
        gint64 plugin_number = g_ascii_strtoll(panel_config_groups[i],NULL,10);
        l = g_slist_append(l,&plugin_number);
    }
    if (l=NULL)
        return 0;
    l = g_slist_sort(l,(GCompareFunc)compare_int64);
    for (i = 0,tmp=l; i < len-1; i++)
    {
        if ((gint64)&tmp->data>i)
            return i;
        tmp=tmp->next;
    }
    g_slist_free_full(l,NULL);
    g_strfreev(panel_config_groups);
    g_key_file_free(kf);
    return len;
}

gboolean panel_gsettings_init_plugin_list(PanelGSettings* settings)
{
    if (settings->all_settings)
        return FALSE;
    GKeyFile* kf = g_key_file_new();
    char** panel_config_groups;
    gsize len,i;
    GError* error;
    g_key_file_load_from_file(kf,settings->config_file_name,G_KEY_FILE_KEEP_COMMENTS,NULL);
    panel_config_groups = g_key_file_get_groups(kf,&len);
    for (i = 0; i < len; i++)
    {
        if(g_strcmp0(DEFAULT_ROOT_NAME,panel_config_groups[i]))
            continue;
        gchar* plugin_name = g_key_file_get_string(kf,panel_config_groups[i],DEFAULT_PLUGIN_NAME_KEY,&error);
        if (error)
        {
            g_key_file_remove_group(kf,panel_config_groups[i],NULL);
            if (plugin_name)
                g_free(plugin_name);
            g_error_free(error);
            continue;
        }
        gboolean has_schema = g_key_file_get_boolean(kf,panel_config_groups[i],DEFAULT_PLUGIN_SCHEMA_KEY,&error);
        if (error)
        {
            g_key_file_remove_group(kf,panel_config_groups[i],NULL);
            if (plugin_name)
                g_free(plugin_name);
            g_error_free(error);
            continue;
        }
        gint64 plugin_number = g_ascii_strtoll(panel_config_groups[i],NULL,10);
        PluginGSettings* s = panel_gsettings_add_plugin_settings(settings,plugin_name,plugin_number);
        plugin_gsettings_config_init(settings,s,has_schema);
        g_free(plugin_name);
    }
    g_strfreev(panel_config_groups);
    g_key_file_free(kf);
    return TRUE;
}

PluginGSettings* panel_gsettings_add_plugin_settings(PanelGSettings* settings,
                                         const gchar* plugin_name,
                                         gint64 plugin_number)
{
    PluginGSettings* new_settings = g_new0(PluginGSettings,1);
    new_settings->plugin_number = plugin_number;
    new_settings->config_path_appender=g_strdup_printf("%s",plugin_name);
    char* path = g_strdup_printf("%s%li/",settings->root_path,plugin_number);
    new_settings->default_settings = g_settings_new_with_backend_and_path(
                DEFAULT_PLUGIN_SETTINGS_ID,
                settings->config_file_backend,
                path);
    settings->all_settings = g_slist_append(settings->all_settings,new_settings);
    g_free(path);
    return new_settings;
}

void plugin_gsettings_config_init(PanelGSettings* panel, PluginGSettings* settings, gboolean has_schema)
{
    g_settings_set_boolean(settings->default_settings,DEFAULT_PLUGIN_SCHEMA_KEY,has_schema);
    if (has_schema)
    {
        char* id = g_strdup_printf("%s.%s",panel->root_path,settings->config_path_appender);
        char* path = g_strdup_printf("%s%li/",panel->root_path,settings->plugin_number);
        settings->config_settings = g_settings_new_with_backend_and_path(
                    id,
                    panel->config_file_backend,
                    path);
        g_free(path);
        g_free(id);
    }
}

void panel_gsettings_remove_plugin_settings(PanelGSettings* settings, gint64 plugin_number)
{
    GSList* l;
    PluginGSettings* removed_settings;
    gchar* group_name = g_strdup_printf("%li",plugin_number);
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

