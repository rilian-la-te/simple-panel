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

#define DESKTOP_ENTRY "Desktop Entry"
#define DESKTOP_FILES_DIR "applications"
#define CATEGORIES "Categories"
#define MENU_CAT "x-simplepanel-menu-category"

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

static void
indent_string (GString *string,
               gint     indent)
{
    while (indent--)
        g_string_append_c (string, ' ');
}

static GString *
g_menu_print_string (GString    *string,
                            GMenuModel *model,
                            gint        indent,
                            gint        tabstop)
{
    gboolean need_nl = FALSE;
    gint i, n;

    if G_UNLIKELY (string == NULL)
            string = g_string_new (NULL);

    n = g_menu_model_get_n_items (model);

    for (i = 0; i < n; i++)
    {
        GMenuAttributeIter *attr_iter;
        GMenuLinkIter *link_iter;
        GString *contents;
        GString *attrs;

        attr_iter = g_menu_model_iterate_item_attributes (model, i);
        link_iter = g_menu_model_iterate_item_links (model, i);
        contents = g_string_new (NULL);
        attrs = g_string_new (NULL);

        while (g_menu_attribute_iter_next (attr_iter))
        {
            const char *name = g_menu_attribute_iter_get_name (attr_iter);
            GVariant *value = g_menu_attribute_iter_get_value (attr_iter);

            if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
            {
                gchar *str;
                str = g_markup_printf_escaped (" %s=\"%s\"", name, g_variant_get_string (value, NULL));
                g_string_append (attrs, str);
                g_free (str);
            }

            else
            {
                gchar *printed;
                gchar *str;
                const gchar *type;

                printed = g_variant_print (value, TRUE);
                type = g_variant_type_peek_string (g_variant_get_type (value));
                str = g_markup_printf_escaped ("<attribute name=\"%s\" type=\"%s\">%s</attribute>\n", name, type, printed);
                indent_string (contents, indent + tabstop);
                g_string_append (contents, str);
                g_free (printed);
                g_free (str);
            }

            g_variant_unref (value);
        }
        g_object_unref (attr_iter);

        while (g_menu_link_iter_next (link_iter))
        {
            const gchar *name = g_menu_link_iter_get_name (link_iter);
            GMenuModel *menu = g_menu_link_iter_get_value (link_iter);
            gchar *str;

            if (contents->str[0])
                g_string_append_c (contents, '\n');

            str = g_markup_printf_escaped ("<link name=\"%s\">\n", name);
            indent_string (contents, indent + tabstop);
            g_string_append (contents, str);
            g_free (str);

            g_menu_print_string (contents, menu, indent + 2 * tabstop, tabstop);

            indent_string (contents, indent + tabstop);
            g_string_append (contents, "</link>\n");
            g_object_unref (menu);
        }
        g_object_unref (link_iter);

        if (contents->str[0])
        {
            indent_string (string, indent);
            g_string_append_printf (string, "<item%s>\n", attrs->str);
            g_string_append (string, contents->str);
            indent_string (string, indent);
            g_string_append (string, "</item>\n");
            need_nl = TRUE;
        }

        else
        {
            if (need_nl)
                g_string_append_c (string, '\n');

            indent_string (string, indent);
            g_string_append_printf (string, "<item%s/>\n", attrs->str);
            need_nl = FALSE;
        }

        g_string_free (contents, TRUE);
        g_string_free (attrs, TRUE);
    }

    return string;
}

gchar* g_menu_make_xml (GMenuModel *model)
{
    GString *string;
    string = g_string_new ("<interface>\n<menu>\n");
    g_menu_print_string (string, model, 2, 2);
    g_string_append(string,"</menu>\n</interface>\n");
    return g_string_free (string, FALSE);
}

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
        action = g_strdup_printf(LAUNCH_ID_ACTION,g_app_info_get_id(G_APP_INFO(info)));
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
                g_object_unref(menu_link);
                break;
            }
        }
        if (!found)
        {
            menu_link = G_MENU(g_menu_model_get_item_link(G_MENU_MODEL(menu),other_num,G_MENU_LINK_SUBMENU));
            g_menu_append_item(menu_link,item);
            g_object_unref(menu_link);
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

GMenuModel* do_applications_menumodel(gboolean for_settings)
{
    GMenu* menu, *submenu;
    GMenuItem* item;
    GList* app_infos_list;
    char* tmp;
    gint i,j;
    app_infos_list = g_app_info_get_all();
    menu = g_menu_new();
    for (i = 0; i < G_N_ELEMENTS(main_cats); i++)
    {
        submenu = g_menu_new();
        GIcon* icon;
        GVariant* val;
        item = g_menu_item_new_submenu(main_cats[i].local_name ?
                                           main_cats[i].local_name : main_cats[i].name,G_MENU_MODEL(submenu));
        icon = g_icon_new_for_string(main_cats[i].icon,NULL);
        g_menu_item_set_icon(item,icon);
        g_menu_item_set_attribute(item,MENU_CAT,"s",main_cats[i].name);
        g_menu_append_item(menu,item);
        g_object_unref(item);
        g_object_unref(icon);
    }
    g_list_foreach(app_infos_list,(add_app_info),menu);
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
        g_menu_model_get_item_attribute(G_MENU_MODEL(menu),j,MENU_CAT,"s",&tmp);
        if ((g_strcmp0(tmp,"Settings") != 0) == for_settings) {
            g_menu_remove(menu,j);
            i--;
        }
        g_free(tmp);
    }
    g_menu_freeze(menu);
    return G_MENU_MODEL(menu);
}

GMenuModel* do_places_menumodel()
{
    GMenu* menu = g_menu_new();
    GMenu* section = NULL;
    GMenuItem* item = NULL;
    char *path, *dir;
    GIcon* icon;
    gint i;
    GDesktopAppInfo* app_info;
    section = g_menu_new();
    path = g_filename_to_uri(g_get_home_dir(),NULL,NULL);
    dir = g_strdup_printf(LAUNCH_URI_ACTION,path);
    g_free(path);
    item = g_menu_item_new(_("Home"),dir);
    icon = G_ICON(g_themed_icon_new_with_default_fallbacks("user-home"));
    g_menu_item_set_icon(item,icon);
    g_menu_append_item(section,item);
    g_free(dir);
    g_object_unref(icon);
    g_object_unref(item);
    path = g_filename_to_uri(g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP),NULL,NULL);
    dir = g_strdup_printf(LAUNCH_URI_ACTION,path);
    g_free(path);
    item = g_menu_item_new(_("Desktop"),dir);
    icon = G_ICON(g_themed_icon_new_with_default_fallbacks("user-desktop"));
    g_menu_item_set_icon(item,icon);
    g_menu_append_item(section,item);
    g_free(dir);
    g_object_unref(icon);
    g_object_unref(item);
    g_menu_append_section(menu,NULL,G_MENU_MODEL(section));
    g_object_unref(section);
    section = g_menu_new();
    dir = g_strdup_printf(LAUNCH_URI_ACTION,"computer:///");
    item = g_menu_item_new(_("Computer"),dir);
    icon = G_ICON(g_themed_icon_new_with_default_fallbacks("computer"));
    g_menu_item_set_icon(item,icon);
    g_menu_append_item(section,item);
    g_free(dir);
    g_object_unref(icon);
    g_object_unref(item);
    dir = g_strdup_printf(LAUNCH_URI_ACTION,"trash:///");
    item = g_menu_item_new(_("Trash"),dir);
    icon = G_ICON(g_themed_icon_new_with_default_fallbacks("user-trash"));
    g_menu_item_set_icon(item,icon);
    g_menu_append_item(section,item);
    g_free(dir);
    g_object_unref(icon);
    g_object_unref(item);
    g_menu_append_section(menu,NULL,G_MENU_MODEL(section));
    g_object_unref(section);
    section = g_menu_new();
    dir = g_strdup_printf(LAUNCH_URI_ACTION,"network:///");
    item = g_menu_item_new(_("Network"),dir);
    icon = G_ICON(g_themed_icon_new_with_default_fallbacks("network-workgroup"));
    g_menu_item_set_icon(item,icon);
    g_menu_append_item(section,item);
    g_free(dir);
    g_object_unref(icon);
    g_object_unref(item);
    dir = g_strdup_printf(LAUNCH_ID_ACTION,"nautilus-connect-server.desktop");
    item = g_menu_item_new(_("Connect to server..."),dir);
    icon = G_ICON(g_themed_icon_new_with_default_fallbacks("network-server"));
    g_menu_item_set_icon(item,icon);
    g_menu_append_item(section,item);
    g_free(dir);
    g_object_unref(icon);
    g_object_unref(item);
    g_menu_append_section(menu,NULL,G_MENU_MODEL(section));
    g_object_unref(section);
    section = g_menu_new();
    app_info = g_desktop_app_info_new("gnome-search-tool.desktop");
    if (!app_info) app_info = g_desktop_app_info_new("mate-search-tool.desktop");
    if (app_info)
    {
        dir = g_strdup_printf(LAUNCH_ID_ACTION,g_app_info_get_id(G_APP_INFO(app_info)));
        item = g_menu_item_new(_("Search..."),dir);
        icon = G_ICON(g_themed_icon_new_with_default_fallbacks("system-search"));
        g_menu_item_set_icon(item,icon);
        g_menu_append_item(section,item);
        g_free(dir);
        g_object_unref(icon);
        g_object_unref(item);
        g_object_unref(app_info);
    }
    g_menu_append_section(menu,NULL,G_MENU_MODEL(section));
    g_object_unref(section);
    g_menu_freeze(menu);
    return G_MENU_MODEL(menu);
}

void menu_load_applications(GSimpleAction* action, GVariant* param, gpointer data)
{

}

void menu_load_places(GSimpleAction* action, GVariant* param, gpointer data)
{

}

void menu_load_system(GSimpleAction* action, GVariant* param, gpointer data)
{

}

GMenuModel* do_system_menumodel()
{
    GMenu* menu,*section;
    GMenuItem* item;
    GDesktopAppInfo* app_info;
    GIcon* icon;
    gchar* dir;
    menu = g_menu_new();
    section = G_MENU(do_applications_menumodel(TRUE));
    g_menu_append_section(menu,NULL,G_MENU_MODEL(section));
    g_object_unref(section);
    section = g_menu_new();
    app_info = g_desktop_app_info_new("gnome-control-center.desktop");
    if (!app_info) app_info = g_desktop_app_info_new("mate-control-center.desktop");
    if (!app_info) app_info = g_desktop_app_info_new("cinnamon-settings.desktop");
    if (!app_info) app_info = g_desktop_app_info_new("xfce4-settings-manager.desktop");
    if (!app_info) app_info = g_desktop_app_info_new("kdesystemsettings.desktop");
    if (app_info)
    {
        dir = g_strdup_printf(LAUNCH_ID_ACTION,g_app_info_get_id(G_APP_INFO(app_info)));
        item = g_menu_item_new(_("Control center"),dir);
        icon = G_ICON(g_themed_icon_new_with_default_fallbacks("preferences-system"));
        g_menu_item_set_icon(item,icon);
        g_menu_append_item(section,item);
        g_free(dir);
        g_object_unref(icon);
        g_object_unref(item);
        g_object_unref(app_info);
    }
    g_menu_append_section(menu,NULL,G_MENU_MODEL(section));
    g_object_unref(section);
    section = g_menu_new();
    item = g_menu_item_new(_("Run..."),"app.run");
    icon = G_ICON(g_themed_icon_new_with_default_fallbacks("system-run"));
    g_menu_item_set_icon(item,icon);
    g_menu_append_item(section,item);
    g_object_unref(icon);
    g_object_unref(item);
    g_menu_append_section(menu,NULL,G_MENU_MODEL(section));
    g_object_unref(section);
    section = g_menu_new();
    item = g_menu_item_new(_("Log Out..."),"app.logout");
    icon = G_ICON(g_themed_icon_new_with_default_fallbacks("system-log-out"));
    g_menu_item_set_icon(item,icon);
    g_menu_append_item(section,item);
    g_object_unref(icon);
    g_object_unref(item);
    item = g_menu_item_new(_("Shutdown..."),"app.shutdown");
    icon = G_ICON(g_themed_icon_new_with_default_fallbacks("system-shutdown"));
    g_menu_item_set_icon(item,icon);
    g_menu_append_item(section,item);
    g_object_unref(icon);
    g_object_unref(item);
    g_menu_append_section(menu,NULL,G_MENU_MODEL(section));
    g_menu_freeze(menu);
    return G_MENU_MODEL(menu);
}

GMenuModel* create_default_menumodel(gboolean as_submenus, const gchar* icon_str)
{
    GMenu* menu = g_menu_new();
    GMenu* section;
    if (as_submenus)
    {
        GMenuItem* item;
        GIcon* icon;
        icon = g_icon_new_for_string(icon_str,NULL);
        item = g_menu_item_new_submenu(_("Applications"),do_applications_menumodel(FALSE));
        g_menu_item_set_icon(item,icon);
        g_object_unref(icon);
        g_menu_append_item(menu,item);
        g_menu_append_submenu(menu, _("Places"), do_places_menumodel());
        g_menu_append_submenu(menu, _("System"), do_system_menumodel());
    }
    else
    {
        g_menu_append(menu,_("Simple Panel v"VERSION),NULL);
        g_menu_append_section(menu, NULL, do_applications_menumodel(FALSE));
        section = g_menu_new();
        g_menu_append_submenu(section, _("Places"), do_places_menumodel());
        g_menu_append_section(menu,NULL,G_MENU_MODEL(section));
        g_object_unref(section);
        g_menu_append_section(menu, NULL, do_system_menumodel());
    }
    g_menu_freeze(menu);
    return G_MENU_MODEL(menu);
}

#ifdef STANDALONE_MAKER
int main(int argc, char** argv, char** envp)
{
    setlocale(LC_CTYPE, "");
    gchar* menu;
    menu = g_menu_make_xml(create_default_menumodel(TRUE,"start-here-symbolic"));
    g_print("%s\n",menu);
    g_free(menu);
    return 0;
}
#endif
