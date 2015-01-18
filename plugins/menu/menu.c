/**
 * Copyright (c) 2006-2014 LxDE Developers, see the file AUTHORS for details.
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
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gdesktopappinfo.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "panel-image-menu-item.h"
#include "misc.h"
#include "plugin.h"


#define MENU_KEY_ICON "icon"

typedef struct {
    gchar *name;
    gchar *icon;
    gchar *local_name;
} cat_info;

static cat_info main_cats[] = {
    { "AudioVideo", "applications-multimedia", N_("Audio & Video") },
    { "Education",  "applications-science", N_("Education") },
    { "Game",       "applications-games", N_("Game") },
    { "Graphics",   "applications-graphics", N_("Graphics") },
    { "Network",    "applications-internet", N_("Network") },
    { "Office",     "applications-office",N_("Office") },
    { "Settings",   "preferences-system", N_("Settings") },
    { "System",     "applications-system", N_("System") },
    { "Utility",    "applications-utilities", N_("Utility") },
    { "Development","applications-development", N_("Development") },
    { "Other","applications-other", N_("Other") },
};

typedef struct {
    GtkWidget *box, *img, *label;
    GtkWidget* apps,*places,*system;
    char *fname;
    gboolean has_system_menu, need_reload;
    guint show_system_menu_idle;
    SimplePanel *panel;
    GSettings *settings;
    GAppInfoMonitor* mon;
} menup;

static guint idle_loader = 0;

GQuark SYS_MENU_ITEM_ID = 0;

static void
menu_destructor(gpointer user_data)
{
    menup *m = (menup *)user_data;

    if( G_UNLIKELY( idle_loader ) )
    {
        g_source_remove( idle_loader );
        idle_loader = 0;
    }

    if (m->show_system_menu_idle)
        g_source_remove(m->show_system_menu_idle);

    if (m->mon)
    {
        g_object_unref(m->mon);
    }
    g_free(m->fname);
    g_free(m);
    return;
}

static void
menu_pos(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, GtkWidget *widget)
{
    menup *m = lxpanel_plugin_get_data(widget);
    lxpanel_plugin_popup_set_position_helper(m->panel, widget, GTK_WIDGET(menu), x, y);
    *push_in = TRUE;
}

/* load icon when mapping the menu item to speed up */
static void on_menu_item_map(GtkWidget *mi, menup *m)
{
  GtkImage* img = GTK_IMAGE(panel_image_menu_item_get_image(PANEL_IMAGE_MENU_ITEM(mi)));
    if( img )
    {
        if (m->need_reload)
        {
            gchar* data_str = g_object_get_qdata(G_OBJECT(mi), SYS_MENU_ITEM_ID);
            if (!data_str)
            {
                GVariant* id = gtk_actionable_get_action_target_value(GTK_ACTIONABLE(mi));
                const gchar* id_str = g_variant_get_string(id,NULL);
                if (!id_str)
                    return;
                GAppInfo* info = G_APP_INFO(g_desktop_app_info_new(id_str));
                data_str = g_icon_to_string(g_app_info_get_icon(info));
                g_object_unref(info);
                g_variant_unref(id);
                m->need_reload =  FALSE;
            }
            simple_panel_image_change_icon(GTK_WIDGET(img),data_str);
            g_free(data_str);
        }
    }
}

static void on_menu_item_style_updated(GtkWidget* mi, menup *m)
{
    m->need_reload =  TRUE;
    /* reload icon */
    on_menu_item_map(mi, m);
}

/* FIXME: this is very dirty, especially if desktop is set not to ~/Desktop
   therefore it should be removed and drag & drop gestures used instead */
static void on_add_menu_item_to_desktop(GtkMenuItem* item, GtkWidget* mi)
{
    FmFileInfo *fi = g_object_get_qdata(G_OBJECT(mi), SYS_MENU_ITEM_ID);
    FmPathList *files = fm_path_list_new();

    fm_path_list_push_tail(files, fm_file_info_get_path(fi));
    fm_link_files(NULL, files, fm_path_get_desktop());
    fm_path_list_unref(files);
}

/* TODO: add menu item to panel */
#if 0
static void on_add_menu_item_to_panel(GtkMenuItem* item, MenuCacheApp* app)
{
    /* Find a penel containing lTB_Launchbar applet.
     * The lTB_Launchbar with most buttons will be choosen if
     * there are several lTB_Launchbar applets loaded.
     */
    GSList* l;
    Plugin* lb = NULL;
    int n_btns = -1;

    for(l = all_panels; !lb && l; l = l->next)
    {
        Panel* panel = (Panel*)l->data;
        GList* pl;
        for(pl=panel->plugins; pl; pl = pl->next)
        {
            Plugin* plugin = (Plugin*)pl;
            if( strcmp(plugin->class->type, "launchbar") == 0 )
            {
                /* FIXME: should we let the users choose which launcherbar to add the btn? */
                break;
#if 0
                int n = launchbar_get_n_btns(plugin);
                if( n > n_btns )
                {
                    lb = plugin;
                    n_btns = n;
                }
#endif
            }
        }
    }

    if( ! lb ) /* lTB_Launchbar is not currently in use */
    {
        /* FIXME: add a lTB_Launchbar plugin to the panel which has a menu, too. */
    }

    if( lb )
    {

    }
}
#endif

#if 0
/* This following function restore_grabs is taken from menu.c of
 * gnome-panel.
 */
/*most of this function stolen from the real gtk_menu_popup*/
static void restore_grabs(GtkWidget *w, gpointer data)
{
    GtkWidget *menu_item = data;
    GtkMenu *menu = GTK_MENU(gtk_widget_get_parent(menu_item));
    GtkWidget *xgrab_shell;
    GtkWidget *parent;

    /* Find the last viewable ancestor, and make an X grab on it
    */
    parent = GTK_WIDGET (menu);
    xgrab_shell = NULL;
    while (parent)
    {
        gboolean viewable = TRUE;
        GtkWidget *tmp = parent;

        while (tmp)
        {
#if GTK_CHECK_VERSION(2, 24, 0)
            if (!gtk_widget_get_mapped(tmp))
#else
            if (!GTK_WIDGET_MAPPED (tmp))
#endif
            {
                viewable = FALSE;
                break;
            }
            tmp = gtk_widget_get_parent(tmp);
        }

        if (viewable)
            xgrab_shell = parent;

        parent = gtk_widget_get_parent(parent);
    }

    /*only grab if this HAD a grab before*/
    if (xgrab_shell && (gtk_widget_has_focus(xgrab_shell)))
     {
        if (gdk_pointer_grab (gtk_widget_get_window(xgrab_shell), TRUE,
                    GDK_BUTTON_PRESS_MASK |
                    GDK_BUTTON_RELEASE_MASK |
                    GDK_ENTER_NOTIFY_MASK |
                    GDK_LEAVE_NOTIFY_MASK,
                    NULL, NULL, 0) == 0)
        {
            if (gdk_keyboard_grab (gtk_widget_get_window(xgrab_shell), TRUE,
                    GDK_CURRENT_TIME) == 0)
                gtk_widget_grab_focus (xgrab_shell);
            else
                gdk_pointer_ungrab (GDK_CURRENT_TIME);
        }
    }
    gtk_grab_add (GTK_WIDGET (menu));
}
#endif

static void restore_submenu(GtkMenuItem *mi, GtkWidget *submenu)
{
    g_signal_handlers_disconnect_by_func(mi, restore_submenu, submenu);
    gtk_menu_item_set_submenu(mi, submenu);
    g_object_set_data(G_OBJECT(mi), "PanelMenuItemSubmenu", NULL);
}

static gboolean on_menu_button_press(GtkWidget* mi, GdkEventButton* evt, menup* m)
{
    if( evt->button == 3 )  /* right */
    {
        GtkWidget* item;
        GtkMenu* p;
        /* don't make duplicates */
        if (g_signal_handler_find(mi, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
            restore_submenu, NULL))
        {
            return FALSE;
        }

        p = GTK_MENU(gtk_menu_new());

        item = gtk_menu_item_new_with_label(_("Add to desktop"));
        g_signal_connect(item, "activate", G_CALLBACK(on_add_menu_item_to_desktop), mi);
        gtk_menu_shell_append(GTK_MENU_SHELL(p), item);

        item = gtk_menu_item_get_submenu(GTK_MENU_ITEM(mi)); /* reuse it */
        if (item)
        {
            /* set object data to keep reference on the submenu we preserve */
            g_object_set_data_full(G_OBJECT(mi), "PanelMenuItemSubmenu",
                                   g_object_ref(item), g_object_unref);
            gtk_menu_popdown(GTK_MENU(item));
        }
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), GTK_WIDGET(p));
        g_signal_connect(mi, "deselect", G_CALLBACK(restore_submenu), item);
        gtk_widget_show_all(GTK_WIDGET(p));
    }
    return FALSE;
}

static GtkWidget* menuitem_allocate(menup* m, const gchar* label)
{
    GtkWidget* mi, *img;
    mi = panel_image_menu_item_new_with_mnemonic(label);
    img = simple_panel_image_new_for_icon(m->panel,"image-missing",-1);
    panel_image_menu_item_set_image( PANEL_IMAGE_MENU_ITEM(mi), img );
    g_signal_connect(mi, "map", G_CALLBACK(on_menu_item_map), m);
    g_signal_connect(mi, "style-updated", G_CALLBACK(on_menu_item_style_updated), m);
    g_signal_connect(mi, "button-press-event", G_CALLBACK(on_menu_button_press), m);
    gtk_widget_show( mi );
    return mi;
}

static void menu_item_from_app_info(gpointer info_data, gpointer m_data)
{
    GAppInfo *info = G_APP_INFO(info_data);
    if(g_app_info_shold_show(info))
    {
        menup* m = (menup*)m_data;
        GtkWidget *mi,img, *menu, *cat_menu, *other_menu;
        GList* cats, *l;
        gchar** cat_info, **tmp;
        gboolean found;
        mi = menuitem_allocate(m,g_app_info_get_name(info) );
        gtk_actionable_set_action_name(GTK_ACTIONABLE(mi),"app.launch-id");
        gtk_actionable_set_action_target_value(GTK_ACTIONABLE(mi),g_variant_new_string(g_app_info_get_id(info)));
        cat_info = g_strsplit_set(g_desktop_app_info_get_categories(G_DESKTOP_APP_INFO(info)) != NULL
                ? g_desktop_app_info_get_categories(G_DESKTOP_APP_INFO(info)) : "",";",0);
        menu=m->apps;
        cat_menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu));
        cats = gtk_container_get_children(GTK_CONTAINER(cat_menu));
        for (l = cats; l->data!=NULL; l=l->next)
        {
            const gchar* cat=(const gchar*)g_object_get_qdata(l->data,SYS_MENU_ITEM_ID);
            if(!g_strcmp0(cat,"Other"))
            {
                other_menu = GTK_WIDGET(l->data);
            }
            for (tmp = cat_info; cat_info && *tmp; tmp++)
            {
                if (!g_strcmp0(*tmp,cat))
                {
                    found = TRUE;
                    break;
                }
            }
            if (found)
            {
                gtk_menu_shell_append_item(GTK_MENU_SHELL(l->data),mi);
                break;
            }
        }
        if (!found)
            gtk_menu_shell_append_item(GTK_MENU_SHELL(other_menu),mi);
        if (cat_info)
            g_strfreev(cat_info);
    }
}

static GtkWidget* do_applications_menu_widget(menup* m)
{
    GtkWidget* menu, *submenu;
    GtkWidget* mi;
    GList* app_infos_list;
    gint i;
    GList* l;
    if( G_UNLIKELY( SYS_MENU_ITEM_ID == 0 ) )
        SYS_MENU_ITEM_ID = g_quark_from_static_string( "SysMenuItem" );
    m->need_reload = TRUE;
    app_infos_list = g_app_info_get_all();
    mi = menuitem_allocate(m, _("Applications"));
    g_object_set_qdata(G_OBJECT(mi),SYS_MENU_ITEM_ID,m->fname);
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi),GTK_WIDGET(submenu));
    m->apps = mi;
    for (i = 0; i < G_N_ELEMENTS(main_cats); i++)
    {
        mi = menuitem_allocate(m,main_cats[i].local_name);
        g_object_set_qdata(G_OBJECT(mi),SYS_MENU_ITEM_ID,main_cats[i].name);
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu),mi);
    }
    g_list_foreach(app_infos_list,menu_item_from_app_info,menu);
    g_list_free_full(app_infos_list,(GDestroyNotify)g_object_unref);
    for (l= gtk_container_get_children(GTK_CONTAINER(submenu)); l->data!=NULL; l = l->next)
    {
        GtkWidget* sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(l->data));
        if (!gtk_container_get_children(GTK_CONTAINER(sub)))
            gtk_widget_set_visible(GTK_WIDGET(l->data), FALSE);
    }
    return menu;
}

static GtkWidget* do_places_menu_widget(menup* m)
{
    return NULL;
}

static gboolean sys_menu_item_has_data( GtkMenuItem* item )
{
   return (g_object_get_qdata( G_OBJECT(item), SYS_MENU_ITEM_ID ) != NULL);
}

static void remove_change_handler(gpointer id, GObject* menu)
{
    g_signal_handler_disconnect(gtk_icon_theme_get_default(), GPOINTER_TO_INT(id));
}


static void show_menu( GtkWidget* widget, menup* m, int btn, guint32 time )
{
    GtkWidget* menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(m->apps));
    gtk_menu_popup(GTK_MENU(menu),
                   NULL, NULL,
                   (GtkMenuPositionFunc)menu_pos, widget,
                   btn, time);
}

static gboolean show_system_menu_idle(gpointer user_data)
{
    menup* m = (menup*)user_data;
    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    show_menu( m->box, m, 0, GDK_CURRENT_TIME );
    m->show_system_menu_idle = 0;
    return FALSE;
}

static void show_system_menu(GtkWidget *p)
{
    menup *m = lxpanel_plugin_get_data(p);

    if (m->has_system_menu && m->show_system_menu_idle == 0)
        /* FIXME: I've no idea why this doesn't work without timeout
                              under some WMs, like icewm. */
        m->show_system_menu_idle = g_timeout_add(200, show_system_menu_idle, m);
}

static GtkWidget* make_bar(menup* m)
{
    GtkWidget* bar;
    bar = gtk_menu_bar_new();
    m->apps = do_applications_menu_widget(m);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar),m->apps);
    gtk_widget_show_all(bar);
    m->has_system_menu = TRUE;
    return bar;
}

static void on_reload_menu(GAppInfoMonitor* mon, gpointer menu_pointer)
{
    menup *m = menu_pointer;
    /* g_debug("reload system menu!!"); */
    gtk_widget_destroy(m->apps);
    m->apps = do_applications_menu_widget(m);
    gtk_widget_destroy(m->img);
    m->img = make_bar(m);
    gtk_container_add(GTK_CONTAINER(m->box),m->img);
}

static GtkWidget *
menu_constructor(SimplePanel *panel, GSettings *settings)
{
    menup *m;
    m = g_new0(menup, 1);
    g_return_val_if_fail(m != NULL, 0);


    m->box = gtk_event_box_new();
    gtk_widget_set_has_window(m->box, FALSE);
    lxpanel_plugin_set_data(m->box, m, menu_destructor);

    /* Save construction pointers */
    m->panel = panel;
    m->settings = settings;
    m->img = make_bar(m);
    m->fname = g_settings_get_string(settings,MENU_KEY_ICON);
    m->mon = g_app_info_monitor_get();
    g_signal_connect(m->mon,"changed",G_CALLBACK(on_reload_menu),m);
    if (!m->img) {
        g_warning("menu: plugin init failed");
        gtk_widget_destroy(m->box);
        return NULL;
    }
    gtk_container_add(GTK_CONTAINER(m->box),m->img);
    gtk_widget_show_all(m->box);
    return m->box;
}

static gboolean apply_config(gpointer user_data)
{
    GtkWidget *p = user_data;
    menup* m = lxpanel_plugin_get_data(p);

    if( m->fname ) {
        GtkWidget* img = panel_image_menu_item_get_image(PANEL_IMAGE_MENU_ITEM(m->apps));
        simple_panel_button_set_icon(img, m->fname,-1);
    }
    g_settings_set_string(m->settings, MENU_KEY_ICON, m->fname);
    return FALSE;
}

static GtkWidget *menu_config(SimplePanel *panel, GtkWidget *p)
{
    menup* menu = lxpanel_plugin_get_data(p);
    return lxpanel_generic_config_dlg(_("Menu"), panel, apply_config, p,
                                      _("Icon"), &menu->fname, CONF_TYPE_FILE_ENTRY,
                                      NULL);
}

FM_DEFINE_MODULE(lxpanel_gtk, menu);

SimplePanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Menubar"),
    .description = N_("Application Menu"),

    .new_instance = menu_constructor,
    .config = menu_config,
    .show_system_menu = show_system_menu,
    .has_config = TRUE
};

/* vim: set sw=4 et sts=4 : */
