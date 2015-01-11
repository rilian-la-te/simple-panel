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

#include "menu-maker.h"
#include <glib.h>
#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define DESKTOP_ENTRY "Desktop Entry"
#define DESKTOP_FILES_DIR "applications"
#define CATEGORIES "Categories"
#define LAUNCH_ACTION "app.launch-id('%s')"
#define MENU_CAT "x-menu-category"

typedef struct {
    gchar *name;
    gchar *icon;
    gchar *local_name;
} cat_info;

static cat_info main_cats[] = {
    { "AudioVideo", "applications-multimedia", N_("Audio & Video") },
    { "Education",  "applications-science", N_("Education")},
    { "Game",       "applications-games", N_("Game")},
    { "Graphics",   "applications-graphics", N_("Graphics")},
    { "Network",    "applications-internet", N_("Network")},
    { "Office",     "applications-office", N_("Office")},
    { "Settings",   "preferences-system", N_("Settings")},
    { "System",     "applications-system", N_("System")},
    { "Utility",    "applications-utilities", N_("Utility")},
    { "Development","applications-development", N_("Development")},
    { "Other","applications-other", N_("Other")},

};

static void add_app_info(gpointer data_info, gpointer data_menu)
{
    GMenu* menu, *menu_link;
    GDesktopAppInfo* info;
    GMenuItem* item = NULL;
    GIcon *icon,*missing;
    gchar *name, *action;
    char **cats, **tmp;
    gint i;
    gint other_num = -1;
    gboolean found = FALSE;
    info = G_DESKTOP_APP_INFO(data_info);
    menu = G_MENU(data_menu);
    name = action = NULL;
    icon = missing = NULL;
    cats = tmp = NULL;
    if (g_app_info_should_show(G_APP_INFO(info)))
    {
        action = g_strdup_printf(LAUNCH_ACTION,g_app_info_get_id(G_APP_INFO(info)));
        item = g_menu_item_new(g_app_info_get_name(G_APP_INFO(info)),(const gchar*)action);
        icon = g_app_info_get_icon(G_APP_INFO(info));
        missing = g_icon_new_for_string("application-x-executable",NULL);
        g_menu_item_set_icon(item, icon ? icon : missing );
        cats = g_strsplit_set(g_desktop_app_info_get_categories(G_DESKTOP_APP_INFO(info)) != NULL
                ? g_desktop_app_info_get_categories(G_DESKTOP_APP_INFO(info)) : "",";",0);
        for (i = 0; i < g_menu_model_get_n_items(G_MENU_MODEL(menu)); i++)
        {
            g_menu_model_get_item_attribute(G_MENU_MODEL(menu),i,MENU_CAT,"s",&name);
            if (!g_strcmp0(name,"Other"))
                other_num = i;
            for (tmp = cats; cats && *tmp; tmp++)
                if (!g_strcmp0(name,*tmp))
                {
                    found = TRUE;
                    break;
                }
            if (found)
            {
                menu_link = G_MENU(g_menu_model_get_item_link(G_MENU_MODEL(menu),i,G_MENU_LINK_SUBMENU));
                g_menu_append_item(menu_link,item);
                break;
            }
        }
        if (!found)
        {
            menu_link = G_MENU(g_menu_model_get_item_link(G_MENU_MODEL(menu),other_num,G_MENU_LINK_SUBMENU));
            g_menu_append_item(menu_link,item);
        }
    }
out:
    if (item)
        g_object_unref(item);
    if (missing)
        g_object_unref(missing);
    if (name)
        g_free(name);
    g_free(action);
    if (cats)
        g_strfreev(cats);
}

GMenuModel* do_applications_menu()
{
    GMenu* menu, *submenu;
    GMenuItem* item;
    GList* app_infos_list;
    gint i;
    app_infos_list = g_app_info_get_all();
    menu = g_menu_new();
    for (i = 0; i < G_N_ELEMENTS(main_cats); i++)
    {
        submenu = g_menu_new();
        item = g_menu_item_new_submenu(main_cats[i].local_name ?
                                           main_cats[i].local_name : main_cats[i].name,G_MENU_MODEL(submenu));
        g_menu_item_set_icon(item,g_icon_new_for_string(main_cats[i].icon,NULL));
        g_menu_item_set_attribute(item,MENU_CAT,"s",main_cats[i].name);
        g_menu_append_item(menu,item);
        g_object_unref(item);
    }
    g_list_foreach(app_infos_list,(add_app_info),menu);
    g_list_free_full(app_infos_list,(GDestroyNotify)g_object_unref);
    for (i = 0; i <= g_menu_model_get_n_items(G_MENU_MODEL(menu));i++)
    {
        submenu = G_MENU(g_menu_model_get_item_link(G_MENU_MODEL(menu),i,G_MENU_LINK_SUBMENU));
        if (g_menu_model_get_n_items(G_MENU_MODEL(submenu)) <= 0)
            g_menu_remove(menu,i);
    }
    g_menu_freeze(menu);
    return G_MENU_MODEL(menu);
}

static gboolean
dir_changed(const gchar *dir, time_t btime)
{
    GDir *d = NULL;
    gchar *cwd;
    const gchar *name;
    gboolean ret = FALSE;
    struct stat buf;

    if (g_stat(dir, &buf))
        return FALSE;
    g_debug("dir=%s ct=%lu mt=%lu\n", dir, buf.st_ctime, buf.st_mtime);
    if ((ret = buf.st_mtime > btime))
        return TRUE;
    cwd = g_get_current_dir();
    if (g_chdir(dir))
    {
        g_debug("can't chdir to %s\n", dir);
        goto out;
    }
    if (!(d = g_dir_open(".", 0, NULL)))
    {
        g_warning("can't open dir %s\n", dir);
        goto out;
    }
    while (!ret && (name = g_dir_read_name(d)))
    {
        if (g_file_test(name, G_FILE_TEST_IS_DIR))
            ret = dir_changed(name, btime);
        else if (!g_str_has_suffix(name, ".desktop"))
            continue;
        else if (g_stat(name, &buf))
            continue;
        g_debug("name=%s ct=%lu mt=%lu\n", name, buf.st_ctime, buf.st_mtime);
        ret = buf.st_mtime > btime;
    }
out:
    if (d)
        g_dir_close(d);
    g_chdir(cwd);
    g_free(cwd);
    return ret;
}

gboolean
systemmenu_changed(time_t btime)
{
    const gchar * const * dirs;
    gboolean ret = FALSE;
    gchar *cwd = g_get_current_dir();

    for (dirs = g_get_system_data_dirs(); *dirs && !ret; dirs++)
    {
        g_chdir(*dirs);
        ret = dir_changed(DESKTOP_FILES_DIR, btime);
    }

    g_debug("btime=%lu\n", btime);
    if (!ret)
    {
        g_chdir(g_get_user_data_dir());
        ret = dir_changed(DESKTOP_FILES_DIR, btime);
    }
    g_chdir(cwd);
    g_free(cwd);
    return ret;
}
