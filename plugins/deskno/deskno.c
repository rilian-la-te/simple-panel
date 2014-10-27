/**
 * Desktop number plugin to lxpanel
 *
 * Copyright (c) 2008-2014 LxDE Developers, see the file AUTHORS for details.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

// reused dclock.c and variables from pager.c
// 11/23/04 by cmeury

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>

#ifndef WNCK_I_KNOW_THIS_IS_UNSTABLE
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#endif
#include <libwnck/libwnck.h>

#include "plugin.h"
#include "X/x-misc.h"

/* Private context for desktop number plugin. */
typedef struct {
    LXPanel * panel;			/* Back pointer to Panel */
    config_setting_t *settings;
    GtkWidget * label;			/* The label */
    int number_of_desktops;		/* Number of desktops */
    char * * desktop_labels;		/* Vector of desktop labels */
    gboolean bold;			/* User preference: True if bold font */
    gboolean wm_labels;			/* User preference: True to display window manager labels */
} DesknoPlugin;

static void deskno_destructor(gpointer user_data);
//static void deskno_redraw(WnckWorkspace* workspace, gpointer* data);

/* Handler for current_desktop event from window manager. */
static gboolean deskno_name_update(WnckScreen* screen, WnckWorkspace* space,gpointer* data)
{
    DesknoPlugin * dc = (DesknoPlugin*)data;
    WnckWorkspace* workspace = wnck_screen_get_active_workspace(screen);
    const gchar* name;
    if (dc->wm_labels)
        name = wnck_workspace_get_name(workspace);
    else
    {
        name = g_strdup_printf("%d",wnck_workspace_get_number(workspace)+1);
    }
    lxpanel_draw_label_text(dc->panel, dc->label, name, dc->bold, 1, TRUE);
}

/* Handler for button-press-event on top level widget. */
static gboolean deskno_button_press_event(GtkWidget * widget, GdkEventButton * event, LXPanel * p)
{
    /* Right-click goes to next desktop, wrapping around to first. */
    int desknum = gdk_x11_screen_get_current_desktop(gdk_screen_get_default());
    int desks = gdk_x11_screen_get_number_of_desktops(gdk_screen_get_default());
    int newdesk = desknum + 1;
    if (newdesk >= desks)
        newdesk = 0;

    /* Ask the window manager to make the new desktop current. */
    Xclimsg(GDK_ROOT_WINDOW(), a_NET_CURRENT_DESKTOP, newdesk, 0, 0, 0, 0);
    return TRUE;
}

/* Plugin constructor. */
static GtkWidget *deskno_constructor(LXPanel *panel, config_setting_t *settings)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    DesknoPlugin * dc = g_new0(DesknoPlugin, 1);
    GtkWidget *p;
    int tmp_int;

    g_return_val_if_fail(dc != NULL, 0);
    dc->panel = panel;
    dc->settings = settings;

    /* Default parameters. */
    dc->wm_labels = TRUE;

    /* Load parameters from the configuration file. */
    if (config_setting_lookup_int(settings, "BoldFont", &tmp_int))
        dc->bold = tmp_int != 0;
    if (config_setting_lookup_int(settings, "WMLabels", &tmp_int))
        dc->wm_labels = tmp_int != 0;

    /* Allocate top level widget and set into Plugin widget pointer. */
    p = gtk_event_box_new();
    lxpanel_plugin_set_data(p, dc, deskno_destructor);
    gtk_container_set_border_width(GTK_CONTAINER (p), 1);

    /* Allocate label widget and add to top level. */
    dc->label = gtk_label_new(NULL);
    gtk_container_add(GTK_CONTAINER(p), dc->label);

    /* Connect signals.  Note use of window manager event object. */
    WnckScreen* screen = wnck_screen_get_default();
    g_signal_connect(screen, "active-workspace-changed", G_CALLBACK(deskno_name_update), (gpointer*) dc);

    /* Initialize value and show the widget. */
    wnck_screen_force_update(screen);
    deskno_name_update(screen,NULL,(gpointer*) dc);
    gtk_widget_show_all(p);
    return p;
}

/* Plugin destructor. */
static void deskno_destructor(gpointer user_data)
{
    DesknoPlugin * dc = (DesknoPlugin *) user_data;

    /* Disconnect signal from window manager event object. */
    WnckScreen* screen = wnck_screen_get_default();
    g_signal_handlers_disconnect_by_func(screen, deskno_name_update, dc);

    /* Deallocate all memory. */
    if (dc->desktop_labels != NULL)
        g_strfreev(dc->desktop_labels);
    g_free(dc);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static gboolean deskno_apply_configuration(gpointer user_data)
{
    DesknoPlugin * dc = lxpanel_plugin_get_data(user_data);
    deskno_name_update(wnck_screen_get_default(),wnck_screen_get_active_workspace(wnck_screen_get_default()),(gpointer*) dc);
    config_group_set_int(dc->settings, "BoldFont", dc->bold);
    config_group_set_int(dc->settings, "WMLabels", dc->wm_labels);
    return FALSE;
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *deskno_configure(LXPanel *panel, GtkWidget *p)
{
    DesknoPlugin * dc = lxpanel_plugin_get_data(p);
    GtkWidget * dlg = lxpanel_generic_config_dlg(_("Desktop Number / Workspace Name"),
        panel, deskno_apply_configuration, p,
        _("Bold font"), &dc->bold, CONF_TYPE_BOOL,
        _("Display desktop names"), &dc->wm_labels, CONF_TYPE_BOOL,
        NULL);
    gtk_widget_set_size_request(GTK_WIDGET(dlg), 400, -1);	/* Improve geometry */
    return dlg;
}

/* Callback when panel configuration changes. */
static void deskno_panel_configuration_changed(LXPanel *panel, GtkWidget *p)
{
    DesknoPlugin * dc = lxpanel_plugin_get_data(p);
    deskno_name_update(wnck_screen_get_default(),NULL, (gpointer*)dc);
}

FM_DEFINE_MODULE(lxpanel_gtk, deskno)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Desktop Number / Workspace Name"),
    .description = N_("Display workspace number, by cmeury@users.sf.net"),

    .new_instance = deskno_constructor,
    .config = deskno_configure,
    .reconfigure = deskno_panel_configuration_changed,
    .button_press_event = deskno_button_press_event
};
