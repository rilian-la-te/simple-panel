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
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

static void add_app_info(GDesktopAppInfo* info, GtkBuilder* builder)
{
    GMenu *menu_link = NULL;
    GMenuItem* item = NULL;
    GIcon *icon,*missing;
    char *action, *cname;
    char **cats, **tmp;
    gboolean found = FALSE;
    cname = action = NULL;
    icon = missing = NULL;
    cats = tmp = NULL;
    if (g_app_info_should_show(G_APP_INFO(info)))
    {
        const gchar* tooltip;
        action = g_strdup_printf(LAUNCH_ID_ACTION,g_app_info_get_id(G_APP_INFO(info)));
        item = g_menu_item_new(g_app_info_get_name(G_APP_INFO(info)),(const gchar*)action);
        icon = g_app_info_get_icon(G_APP_INFO(info));
        missing = g_icon_new_for_string("application-x-executable",NULL);
        g_menu_item_set_icon(item, icon ? icon : missing );
        tooltip = g_app_info_get_description(G_APP_INFO(info));
        if (tooltip)
            g_menu_item_set_attribute(item,"tooltip","s",tooltip);
        cats = g_strsplit_set(g_desktop_app_info_get_categories(G_DESKTOP_APP_INFO(info)) != NULL
                ? g_desktop_app_info_get_categories(G_DESKTOP_APP_INFO(info)) : "",";",0);
        for (tmp = cats; cats && *tmp; tmp++)
        {
            cname = g_ascii_strdown(*tmp,-1);
            menu_link = G_MENU(gtk_builder_get_object(builder,cname));
            if (menu_link)
            {
                found = TRUE;
                break;
            }
            g_free(cname);
        }
        if (!found)
            menu_link = G_MENU(gtk_builder_get_object(builder,"other"));
        g_menu_append_item(menu_link,item);
        g_object_unref(item);
        menu_link = NULL;
        g_object_unref(missing);
        g_free(action);
        if (cats)
            g_strfreev(cats);
    }
}

GMenuModel* do_applications_menumodel(gboolean for_settings)
{
    GMenu* menu, *submenu;
    GtkBuilder* builder;
    GList* app_infos_list, *l;
    char* tmp;
    gint i,j;
    app_infos_list = g_app_info_get_all();
    builder = gtk_builder_new_from_resource("/org/simple/panel/lib/system-menus.ui");
    menu = G_MENU(gtk_builder_get_object(builder,"categories"));
    for (l = app_infos_list; l != NULL; l = l->next)
        add_app_info(l->data,builder);
    g_list_free_full(app_infos_list,(GDestroyNotify)g_object_unref);
    for (i = 0; i < g_menu_model_get_n_items(G_MENU_MODEL(menu));i++)
    {
        i = (i < 0) ? 0 : i;
        submenu = G_MENU(g_menu_model_get_item_link(G_MENU_MODEL(menu),i,G_MENU_LINK_SUBMENU));
        if (g_menu_model_get_n_items(G_MENU_MODEL(submenu)) <= 0)
        {
            g_menu_remove(menu,i);
            i--;
        }
        j = (i < 0) ? 0 : i;
        g_menu_model_get_item_attribute(G_MENU_MODEL(menu),j,"x-cat","s",&tmp);
        if ((g_strcmp0(tmp,"settings") != 0) == for_settings) {
            g_menu_remove(menu,j);
            i--;
        }
        g_free(tmp);
    }
    g_object_ref(menu);
    g_object_unref(builder);
    return G_MENU_MODEL(menu);
}

GMenuModel* do_places_menumodel()
{
    GMenu* menu, *section;
    GMenuItem* item = NULL;
    GtkBuilder* builder;
    char *path;
    GDesktopAppInfo* app_info;
    builder = gtk_builder_new_from_resource("/org/simple/panel/lib/system-menus.ui");
    menu = G_MENU(gtk_builder_get_object(builder,"places-menu"));
    section = G_MENU(gtk_builder_get_object(builder,"folders-section"));

    item = g_menu_item_new(_("Home"),NULL);
    path = g_filename_to_uri(g_get_home_dir(),NULL,NULL);
    g_menu_item_set_attribute(item,"icon","s","user-home");
    g_menu_item_set_action_and_target_value(item,"app.launch-uri",g_variant_new_string(path));
    g_menu_append_item(section,item);
    g_object_unref(item);
    g_free(path);
    item = g_menu_item_new(_("Desktop"),NULL);
    path = g_filename_to_uri(g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP),NULL,NULL);
    g_menu_item_set_attribute(item,"icon","s","user-desktop");
    g_menu_item_set_action_and_target_value(item,"app.launch-uri",g_variant_new_string(path));
    g_menu_append_item(section,item);
    g_object_unref(item);
    g_free(path);
    section = G_MENU(gtk_builder_get_object(builder,"recent-section"));
    app_info = g_desktop_app_info_new("gnome-search-tool.desktop");
    if (!app_info) app_info = g_desktop_app_info_new("mate-search-tool.desktop");
    if (app_info)
    {
        item = g_menu_item_new(_("Search..."),NULL);
        g_menu_item_set_attribute(item,"icon","s","system-search");
        g_menu_item_set_action_and_target_value(item,"app.launch-id",g_variant_new_string(g_app_info_get_id(G_APP_INFO(app_info))));
        g_menu_prepend_item(section,item);
        g_object_unref(item);
        g_object_unref(app_info);
    }
    section = G_MENU(gtk_builder_get_object(builder,"recent-section"));
    g_menu_remove(section,1);
    g_object_ref(menu);
    g_object_unref(builder);
    return G_MENU_MODEL(menu);
}

GMenuModel* do_system_menumodel()
{
    GtkBuilder* builder;
    GMenu* menu,*section;
    GMenuItem* item;
    GDesktopAppInfo* app_info;
    GIcon* icon;
    gchar* dir;
    builder = gtk_builder_new_from_resource("/org/simple/panel/lib/system-menus.ui");
    menu = G_MENU(gtk_builder_get_object(builder,"settings-section"));
    section = G_MENU(do_applications_menumodel(TRUE));
    g_menu_append_section(menu,NULL,G_MENU_MODEL(section));
    g_object_unref(section);
    app_info = g_desktop_app_info_new("gnome-control-center.desktop");
    if (!app_info) app_info = g_desktop_app_info_new("matecc.desktop");
    if (!app_info) app_info = g_desktop_app_info_new("cinnamon-settings.desktop");
    if (!app_info) app_info = g_desktop_app_info_new("xfce4-settings-manager.desktop");
    if (!app_info) app_info = g_desktop_app_info_new("kdesystemsettings.desktop");
    if (app_info)
    {
        item = g_menu_item_new(_("Control center"),NULL);
        g_menu_item_set_attribute(item,"icon","s","preferences-system");
        g_menu_item_set_action_and_target_value(item,"app.launch-id",g_variant_new_string(g_app_info_get_id(G_APP_INFO(app_info))));
        g_menu_append_item(menu,item);
        g_object_unref(item);
        g_object_unref(app_info);
    }
    menu = G_MENU(gtk_builder_get_object(builder,"system-menu"));
    g_object_ref(menu);
    g_object_unref(builder);
    return G_MENU_MODEL(menu);
}

void g_menu_append_all_sections(GMenu *menu, GMenuModel* append)
{
    gint i;
    gint num = g_menu_model_get_n_items(append);
    gchar* label = NULL;
    GMenuModel* link;
    for(i = 0; i< num; i++)
    {
        link = g_menu_model_get_item_link(G_MENU_MODEL(append),i,"section");
        g_menu_model_get_item_attribute(append,i,"label","s",&label);
        if (link)
        {
            g_menu_append_section(menu,label,link);
            g_object_unref(link);
        }
        if (label)
            g_free(label);
    }
}

GMenuModel* create_default_menumodel(gboolean as_submenus, const gchar* icon_str)
{
    GMenu* menu = g_menu_new();
    GMenu* section;
    GMenuModel* apps, *places, *system;
    apps = do_applications_menumodel(FALSE);
    places = do_places_menumodel();
    system = do_system_menumodel();
    if (as_submenus)
    {
        GMenuItem* item;
        item = g_menu_item_new_submenu(_("Applications"),apps);
        g_menu_item_set_attribute(item,"icon","s",icon_str);
        g_menu_append_item(menu,item);
        g_object_unref(item);
        g_menu_append_submenu(menu, _("Places"), places);
        g_menu_append_submenu(menu, _("System"), system);
    }
    else
    {
        g_menu_append(menu,_("Simple Panel v"VERSION),NULL);
        g_menu_append_section(menu, NULL,apps);
        section = g_menu_new();
        g_menu_append_submenu(section, _("Places"), places);
        g_menu_append_section(menu,NULL,G_MENU_MODEL(section));
        g_object_unref(section);
        g_menu_append_all_sections(menu, system);
    }
    g_object_unref(apps);
    g_object_unref(places);
    g_object_unref(system);
    return G_MENU_MODEL(menu);
}
