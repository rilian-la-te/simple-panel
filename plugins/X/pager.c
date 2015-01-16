/* pager.c -- pager module of lxpanel project
 *
 * Copyright (C) 2009 Dongxu Li <song6song@sourceforge.net>
 *               2012 Julien Lavergne <gilir@ubuntu.com>
 *
 * This file is part of lxpanel.
 *
 * lxpanel is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * lxpanel is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with sawfish; see the file COPYING.   If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <libfm/fm-gtk.h>

#include <glib/gi18n.h>
#ifndef WNCK_I_KNOW_THIS_IS_UNSTABLE
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#endif
#include <libwnck/libwnck.h>

#include "plugin.h"

typedef struct
{
    WnckPager *pager;
    gint border;
} PagerData;

/* command to configure desktop, it will be set by .config callback */
static const char *configure_command = NULL;

static void on_params_change_callback(SimplePanel* panel, GParamSpec* param, PagerData* d)
{
    int rows, r,h;
    g_object_get(panel,"height",&h,NULL);
    h -= 2 * d->border;
    /* set geometry */
    wnck_pager_set_orientation(d->pager, panel_get_orientation(panel));
    if (panel_get_orientation(panel) == GTK_ORIENTATION_VERTICAL)
        h *= ((gfloat) gdk_screen_height() / (gfloat) gdk_screen_width());
    rows = h / (panel_get_icon_size(panel) * 2) + 1; /* min */
    r = (h - 2) / panel_get_icon_size(panel); /* max */
    rows = MAX(rows, r);
    wnck_pager_set_n_rows(d->pager, rows);
}

static GtkWidget *pager_constructor(SimplePanel *panel, GSettings *settings)
{
    GtkWidget *p, *w;
    PagerData *d;
    int border = 1; /* NOTE: old 'pager' used 2, WnckPager has 1, need 1 more */

    /* FIXME: use some global setting for border */
    w = wnck_pager_new();

    g_return_val_if_fail(w != NULL, NULL);
    d = g_new(PagerData, 1);
    d->pager = WNCK_PAGER(w);
    d->border = border;
    p = gtk_event_box_new();
    gtk_widget_set_has_window(p, FALSE);
    lxpanel_plugin_set_data(p, d, g_free);

    /* we cannot configure pager until it added into widgets hierarchy */
    g_signal_connect(panel,"notify::edge",G_CALLBACK(on_params_change_callback),d);
    g_signal_connect(panel,"notify::width",G_CALLBACK(on_params_change_callback),d);
    g_signal_connect(panel,"notify::height",G_CALLBACK(on_params_change_callback),d);
    wnck_pager_set_display_mode(d->pager, WNCK_PAGER_DISPLAY_CONTENT);

    gtk_widget_show(w);

    /* add an additional container to setup a border */
    w = gtk_event_box_new();
    gtk_widget_set_has_window(w, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(w), border);
    gtk_container_add(GTK_CONTAINER(w), (GtkWidget*)d->pager);
    on_params_change_callback(panel,NULL,d);
    gtk_widget_show(w);
    gtk_container_add(GTK_CONTAINER(p), w);

    return p;
}

/* this is a modified version of patch from Lubuntu */
static GtkWidget *pager_configure(SimplePanel *panel, GtkWidget *instance)
{
    if (configure_command)
        fm_launch_command_simple(NULL, NULL, G_APP_INFO_CREATE_NONE,
                                 configure_command, NULL);
    else
        fm_show_error(NULL, NULL,
                      _("Sorry, there was no window manager configuration program found."));
    return NULL; /* no configuration dialog of lxpanel available */
}

static void pager_menu_callback(GtkWidget *widget, gpointer data)
{
    gtk_widget_set_sensitive(widget, FALSE);
}

static gboolean pager_update_context_menu(GtkWidget *plugin, GtkMenu *menu)
{
    GdkScreen *screen = gdk_screen_get_default();
    const char *wm_name = gdk_x11_screen_get_window_manager_name(screen);
    char *path = NULL;

    /* update configure_command */
    configure_command = NULL;
    if (g_strcmp0(wm_name, "Openbox") == 0)
    {
        if ((path = g_find_program_in_path("obconf")))
        {
            configure_command = "obconf --tab 6";
        }
    }
    else if (g_strcmp0(wm_name, "compiz") == 0)
    {
         if ((path = g_find_program_in_path("ccsm")))
         {
              configure_command = "ccsm";
         }
         else if ((path = g_find_program_in_path("simple-ccsm")))
         {
              configure_command = "simple-ccsm";
         }
    }
    /* FIXME: support other WMs */
    if (configure_command == NULL)
    {
        /* disable 'Settings' menu item */
        gtk_container_foreach(GTK_CONTAINER(menu), pager_menu_callback, NULL);
    }
    /* FIXME: else replace 'Settings' item label with "Launch Workspaces Configurator (%s)" */
    g_free(path);
    return FALSE;
}

FM_DEFINE_MODULE(lxpanel_gtk, pager)

SimplePanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Desktop Pager"),
    .description = N_("Simple pager plugin"),

    .new_instance = pager_constructor,
    .config = pager_configure,
//    .update_context_menu = pager_update_context_menu,
};
