/**
 *
 * Copyright (c) 2006-2014 LxDE Developers, see the file AUTHORS for details.
 * Copyright (c) Konstantin Pugin.
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
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <libfm/fm-gtk.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "misc.h"
#include "plugin.h"
#include "menu-maker.h"

#define MENUMODEL_KEY_ICON "icon-name"
#define MENUMODEL_KEY_CAPTION "menu-name"
#define MENUMODEL_KEY_IS_MENU_BAR "is-menu-bar"
#define MENUMODEL_KEY_IS_SYSTEM_MENU "is-system-menu"
#define MENUMODEL_KEY_IS_INTERNAL_MENU "is-internal-menu"
#define MENUMODEL_KEY_MODEL_FILE "model-file"
#define MENUMODEL_KEY_MODEL_NAME "model-name"


typedef struct {
    GtkWidget *box, *button;
    char* caption;
    SimplePanel *panel;
    GSettings *settings;
    GMenuModel* menu;
    gchar* icon_str, *model_file, *model_name;
    GAppInfoMonitor* app_monitor;
    GFileMonitor* file_monitor;
    gboolean system,internal,bar;
    guint show_system_menu_idle;
    guint monitor_update_idle;
} MenuModelPlugin;

static GMenuModel* return_menumodel(MenuModelPlugin* m);
static GMenuModel* read_menumodel(MenuModelPlugin *m);
static GtkWidget* create_menubutton(MenuModelPlugin* m);
static GtkWidget* create_menubar(MenuModelPlugin* m);
static void monitor_update(MenuModelPlugin* m);
static GtkWidget* menumodel_widget_create(MenuModelPlugin* m);
static void panel_edge_changed(SimplePanel* panel, GParamSpec* param, MenuModelPlugin* menu);

static void menumodel_widget_destroy(MenuModelPlugin* m)
{
    if (m->show_system_menu_idle)
        g_source_remove(m->show_system_menu_idle);
    if (m->monitor_update_idle)
        g_source_remove(m->monitor_update_idle);
    if (m->app_monitor)
    {
        g_signal_handlers_disconnect_matched(m->app_monitor, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         monitor_update, NULL);
        g_object_unref(m->app_monitor);
    }
    if (m->file_monitor)
    {
        g_signal_handlers_disconnect_matched(m->file_monitor, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         monitor_update, NULL);
        g_object_unref(m->file_monitor);
    }
    if (m->bar)
        g_signal_handlers_disconnect_matched(m->panel, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         panel_edge_changed, NULL);
    if (m->button)
        gtk_container_remove(GTK_CONTAINER(m->box),m->button);
    if (m->menu)
        g_object_unref(m->menu);
}

static void
menumodel_destructor(gpointer user_data)
{
    MenuModelPlugin *m = (MenuModelPlugin *)user_data;
    menumodel_widget_destroy(m);
    if (m->icon_str)
        g_free(m->icon_str);
    if (m->model_file)
        g_free(m->model_file);
    if (m->model_name)
        g_free(m->model_name);
    if (m->caption)
        g_free(m->caption);
    g_free(m);
    return;
}

static void panel_edge_changed(SimplePanel* panel, GParamSpec* param, MenuModelPlugin* menu)
{
    int edge;
    g_object_get(panel,PANEL_PROP_EDGE,&edge,NULL);
    int orient;
    orient = (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM) ? GTK_PACK_DIRECTION_LTR : GTK_PACK_DIRECTION_TTB;
    if (menu->bar)
    {
        gtk_menu_bar_set_pack_direction(GTK_MENU_BAR(menu->button),orient);
    }
}

static void
menu_pos(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, GtkWidget *widget)
{
    MenuModelPlugin *m = lxpanel_plugin_get_data(widget);
    lxpanel_plugin_popup_set_position_helper(m->panel, widget, GTK_WIDGET(menu), x, y);
    *push_in = TRUE;
}

static void show_menu( GtkWidget* widget, MenuModelPlugin* m, int btn, guint32 time )
{
    GtkMenu* menu = GTK_MENU(gtk_menu_new_from_model(G_MENU_MODEL(m->menu)));
    gtk_menu_attach_to_widget(menu,widget,NULL);
    gtk_menu_popup(GTK_MENU(menu),
                   NULL, NULL,
                   (GtkMenuPositionFunc)menu_pos, widget,
                   btn, time);
    g_signal_connect(GTK_WIDGET(menu),"focus-out-event",G_CALLBACK(gtk_widget_destroy),NULL);
}

static gboolean show_system_menu_idle(gpointer user_data)
{
    MenuModelPlugin* m = (MenuModelPlugin*)user_data;
    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    show_menu( m->box, m, 0, GDK_CURRENT_TIME );
    m->show_system_menu_idle = 0;
    return FALSE;
}

static gboolean monitor_update_idle(gpointer user_data)
{
    MenuModelPlugin* m = (MenuModelPlugin*)user_data;
    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    menumodel_widget_destroy(m);
    m->button = menumodel_widget_create(m);
    m->monitor_update_idle = 0;
    return FALSE;
}


static void show_system_menu(GtkWidget *p)
{
    MenuModelPlugin *m = lxpanel_plugin_get_data(p);

    if (m && m->system && m->show_system_menu_idle == 0)
        /* FIXME: I've no idea why this doesn't work without timeout
                              under some WMs, like icewm. */
        m->show_system_menu_idle = g_timeout_add(200, show_system_menu_idle, m);
}

static void monitor_update(MenuModelPlugin* m)
{
    if (m && m->monitor_update_idle == 0)
        m->monitor_update_idle = g_timeout_add(200, monitor_update_idle, m);
}

static GMenuModel* read_menumodel(MenuModelPlugin* m)
{
    GtkBuilder* builder;
    builder = gtk_builder_new();
    gtk_builder_add_from_file(builder,m->model_file,NULL);
    GMenuModel* menu = G_MENU_MODEL(gtk_builder_get_object(builder,m->model_name));
    return menu;
}


static GMenuModel* return_menumodel(MenuModelPlugin* m)
{
    GMenuModel* ret;
    if (m->internal)
    {
        ret = create_default_menumodel(m->bar,m->icon_str);
        m->app_monitor = g_app_info_monitor_get();
        g_signal_connect_swapped(m->app_monitor,"changed",G_CALLBACK(monitor_update),m);
        m->file_monitor = NULL;
    }
    else
    {
        GFile* f;
        if (!m->model_file)
        {
            return NULL;
        }
        f = g_file_new_for_path(m->model_file);
        ret = read_menumodel(m);
        m->app_monitor = NULL;
        m->file_monitor = g_file_monitor_file(f,G_FILE_MONITOR_SEND_MOVED|G_FILE_MONITOR_WATCH_HARD_LINKS,NULL,NULL);
        g_object_unref(f);
        g_signal_connect_swapped(m->file_monitor,"changed",G_CALLBACK(monitor_update),m);
    }
    return ret;
}

static GtkWidget* menumodel_widget_create(MenuModelPlugin* m)
{
    m->menu = return_menumodel(m);
    if (m->bar)
        return create_menubar(m);
    else
        return create_menubutton(m);
}

static GtkWidget* create_menubutton(MenuModelPlugin* m)
{
    GtkWidget* img;
    m->button = gtk_menu_button_new();
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(m->button),G_MENU_MODEL(m->menu));
    gtk_menu_button_set_use_popover(GTK_MENU_BUTTON(m->button),FALSE);
    if(m->icon_str)
    {
        img = simple_panel_image_new_for_icon(m->panel,m->icon_str,-1);
        gtk_widget_show(img);
    }
    simple_panel_setup_button(m->button,img,m->caption ? m->caption : NULL);
    gtk_container_add(GTK_CONTAINER(m->box),m->button);
    gtk_widget_show(m->button);
    return m->button;
}

static GtkWidget* create_menubar(MenuModelPlugin* m)
{
    m->button = gtk_menu_bar_new_from_model(G_MENU_MODEL(m->menu));
    gtk_container_add(GTK_CONTAINER(m->box),m->button);
    gtk_widget_show(m->button);
    g_signal_connect(m->panel,"notify::edge",G_CALLBACK(panel_edge_changed),m);
    return m->button;
}

static GtkWidget* menumodel_constructor(SimplePanel *panel, GSettings *settings)
{
    MenuModelPlugin* plugin;
    gboolean system,internal;
    plugin = g_new0(MenuModelPlugin,1);
    g_return_val_if_fail(m != NULL, 0);
    plugin->panel = panel;
    plugin->settings = settings;
    system = plugin->system = g_settings_get_boolean(settings,MENUMODEL_KEY_IS_SYSTEM_MENU);
    internal = plugin->internal = g_settings_get_boolean(settings,MENUMODEL_KEY_IS_INTERNAL_MENU);
    plugin->bar = g_settings_get_boolean(settings,MENUMODEL_KEY_IS_MENU_BAR);
    plugin->model_name = g_settings_get_string(settings,MENUMODEL_KEY_MODEL_NAME);
    g_settings_get(plugin->settings,MENUMODEL_KEY_MODEL_FILE,"ms",&plugin->model_file);
    g_settings_get(plugin->settings,MENUMODEL_KEY_CAPTION,"ms",&plugin->caption);
    g_settings_get(plugin->settings,MENUMODEL_KEY_ICON,"ms",&plugin->icon_str);
    plugin->box = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(plugin->box), FALSE);
    plugin->button = menumodel_widget_create(plugin);
    lxpanel_plugin_set_data(plugin->box, plugin, menumodel_destructor);
    gtk_widget_show(plugin->box);
    return plugin->box;
}

static gboolean apply_config(gpointer user_data)
{
    GtkWidget *p = user_data;
    MenuModelPlugin* m = lxpanel_plugin_get_data(p);
    g_settings_set(m->settings, MENUMODEL_KEY_ICON,"ms", m->icon_str);
    g_settings_set(m->settings, MENUMODEL_KEY_CAPTION,"ms", m->caption);
    g_settings_set_string(m->settings, MENUMODEL_KEY_MODEL_NAME, m->model_name);
    g_settings_set(m->settings, MENUMODEL_KEY_MODEL_FILE,"ms", m->model_file);
    g_settings_set_boolean(m->settings,MENUMODEL_KEY_IS_INTERNAL_MENU,m->internal);
    g_settings_set_boolean(m->settings,MENUMODEL_KEY_IS_SYSTEM_MENU,m->system);
    g_settings_set_boolean(m->settings,MENUMODEL_KEY_IS_MENU_BAR,m->bar);
    menumodel_widget_destroy(m);
    menumodel_widget_create(m);
    return FALSE;
}

static GtkWidget *menumodel_config(SimplePanel *panel, GtkWidget *p)
{
    MenuModelPlugin* menu = lxpanel_plugin_get_data(p);

    return lxpanel_generic_config_dlg(_("Custom Menu"), panel, apply_config, p,
                                      _("If internal menu is enabled, menu file will not be used, predefeined menu will be used instead."),NULL, CONF_TYPE_TRIM,
                                      _("Is internal menu"), &menu->internal, CONF_TYPE_BOOL,
                                      _("Is system menu (can be keybound)"), &menu->system, CONF_TYPE_BOOL,
                                      _("Is Menubar"), &menu->bar, CONF_TYPE_BOOL,
                                      _("Icon (for button only)"), &menu->icon_str, CONF_TYPE_FILE_ENTRY,
                                      _("Caption (for button only)"), &menu->caption, CONF_TYPE_STR,
                                      _("Menu file name"), &menu->model_file, CONF_TYPE_FILE_ENTRY,
                                      _("Menu name in file"), &menu->model_name, CONF_TYPE_STR,
                                      NULL);
}

SimplePanelPluginInit lxpanel_static_plugin_menumodel = {
    .name = N_("Custom Menu"),
    .description = N_("Any custom menu in GMenuModel XML format. Also supports setting it as application menu."),
    .new_instance = menumodel_constructor,
    .config = menumodel_config,
    .show_system_menu = show_system_menu,
    .has_config = TRUE
};

/* vim: set sw=4 et sts=4 : */
