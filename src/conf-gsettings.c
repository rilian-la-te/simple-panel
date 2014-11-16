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
#include <string.h>
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

inline static guint compare_uint(gconstpointer a, gconstpointer b)
{
    guint inta = GPOINTER_TO_UINT(a);
    guint intb = GPOINTER_TO_UINT(b);
    return inta - intb;
}

gint64 panel_gsettings_find_free_num (PanelGSettings* settings)
{
    GKeyFile* kf = g_key_file_new();
    gsize i,len;
    char** panel_config_groups;
    g_key_file_load_from_file(kf,settings->config_file_name,G_KEY_FILE_KEEP_COMMENTS,NULL);
    panel_config_groups = g_key_file_get_groups(kf,&len);
    GSList *l = NULL;
    GSList *tmp = NULL;
    for (i = 0; i < len; i++)
    {
        if(!g_strcmp0(DEFAULT_ROOT_NAME,panel_config_groups[i]))
            continue;
        guint plugin_number = g_ascii_strtoll(panel_config_groups[i],NULL,10);
        l = g_slist_insert_sorted(l,GINT_TO_POINTER(plugin_number),(GCompareFunc)compare_uint);
    }
    if (l == NULL)
        return 0;
    for (i = 0,tmp=l; i < len-1; i++)
    {
        if (GPOINTER_TO_INT(tmp->data) > i)
        {
            g_slist_free(l);
            g_strfreev(panel_config_groups);
            g_key_file_free(kf);
            return i;
        }
        tmp=tmp->next;
    }
    g_slist_free(l);
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
    GError* error = NULL;
    g_key_file_load_from_file(kf,settings->config_file_name,G_KEY_FILE_KEEP_COMMENTS,NULL);
    panel_config_groups = g_key_file_get_groups(kf,&len);
    for (i = 0; i < len; i++)
    {
        if(!g_strcmp0(DEFAULT_ROOT_NAME,panel_config_groups[i]))
            continue;
        gchar* plugin_name = g_key_file_get_string(kf,panel_config_groups[i],DEFAULT_PLUGIN_NAME_KEY,&error);
        plugin_name = g_strstrip(g_strdelimit(plugin_name,"'",' '));
        if (error)
        {
            g_key_file_remove_group(kf,panel_config_groups[i],NULL);
            if (plugin_name)
                g_free(plugin_name);
            g_clear_error(&error);
            continue;
        }
        gboolean has_schema = g_key_file_get_boolean(kf,panel_config_groups[i],DEFAULT_PLUGIN_SCHEMA_KEY,&error);
        if (error)
        {
            g_key_file_remove_group(kf,panel_config_groups[i],NULL);
            if (plugin_name)
                g_free(plugin_name);
            g_clear_error(&error);
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
                                         const char* plugin_name,
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
        char* id = g_strdup_printf("%s.%s",panel->root_name,settings->config_path_appender);
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
    char* group_name = g_strdup_printf("%li",plugin_number);
    for(l = settings->all_settings; l!=NULL; l=l->next)
    {
        removed_settings = l->data;
        if (plugin_number == removed_settings->plugin_number)
        {
            settings->all_settings = g_slist_remove(l,removed_settings);
            g_free(removed_settings->config_path_appender);
            if (removed_settings->config_settings)
                g_object_unref(removed_settings->config_settings);
            g_object_unref(removed_settings->default_settings);
            GKeyFile* keyfile = g_key_file_new();
            g_key_file_load_from_file(keyfile,settings->config_file_name,G_KEY_FILE_KEEP_COMMENTS,NULL);
            if (g_key_file_has_group(keyfile,group_name))
            {
                g_key_file_remove_group(keyfile,group_name,NULL);
                g_key_file_save_to_file(keyfile,settings->config_file_name,NULL);
            }
            g_key_file_free(keyfile);
            g_free(group_name);
            return;
        }
        removed_settings = NULL;
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

void panel_gsettings_free(PanelGSettings* settings, gboolean remove)
{
    g_slist_free_full(settings->all_settings,(GDestroyNotify)plugin_gsettings_remove);
    if (settings->toplevel_settings)
    {
        g_object_unref(settings->toplevel_settings);
        settings->toplevel_settings = NULL;
        g_object_unref(settings->config_file_backend);
        settings->config_file_backend = NULL;
    }
    if (settings->root_name)
    {
        g_free(settings->root_name);
        settings->root_name = NULL;
    }
    if (remove)
    {
        GFile* f;
        f= g_file_new_for_path(settings->config_file_name);
        g_file_delete(f,NULL,NULL);
        g_object_unref(f);
    }
    if (settings->root_path)
    {
        g_free(settings->root_path);
        settings->root_path = NULL;
    }
    if (settings->config_file_name)
    {
        g_free(settings->config_file_name);
        settings->config_file_name = NULL;
        g_free(settings);
    }
    settings = NULL;
}
