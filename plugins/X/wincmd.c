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

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#ifndef WNCK_I_KNOW_THIS_IS_UNSTABLE
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#endif
#include <libwnck/libwnck.h>

#include "plugin.h"

#define WINCMD_KEY_LEFT "left-button-command"
#define WINCMD_KEY_MIDDLE "middle-button-command"
#define WINCMD_KEY_TOGGLE "toggle-iconify-and-shade"
#define WINCMD_KEY_IMAGE "image"

/* Commands that can be issued. */
typedef enum {
    WC_NONE,
    WC_ICONIFY,
    WC_SHADE
} WcCommand;

/* Private context for window command plugin. */
typedef struct {
    SimplePanel* panel;
    GSettings* settings;
    char * image;				/* Main icon */
    WcCommand button_1_command;		/* Command for mouse button 1 */
    WcCommand button_2_command;		/* Command for mouse button 2 */
    gboolean toggle_preference;			/* User preference: toggle iconify/shade and map */
    gboolean toggle_state;			/* State of toggle */
} WinCmdPlugin;

static void wincmd_destructor(gpointer user_data);

/* Adjust the toggle state after a window command. */
static void wincmd_adjust_toggle_state(WinCmdPlugin * wc)
{
    /* Ensure that if the user changes the preference from "unconditional" to "toggle", we do a raise on the next click. */
    if (wc->toggle_preference)
        wc->toggle_state = ! wc->toggle_state;
        else wc->toggle_state = TRUE;
}

/* Execute a window command. */
static void wincmd_execute(WinCmdPlugin * wc, WcCommand command)
{
    /* Get the list of all windows. */
    WnckScreen* scr = wnck_screen_get_default();
    GList* windows = wnck_screen_get_windows(scr);
    if (windows)
    {
        WnckWorkspace* desk = wnck_screen_get_active_workspace(scr);
        GList* l;
        for (l = windows; l!= NULL; l=l->next)
        {
            if (wnck_window_is_visible_on_workspace(WNCK_WINDOW(l->data),desk))
            {
                switch (command)
                {
                    case WC_NONE:
                        break;

                    case WC_ICONIFY:
                        if ((( ! wc->toggle_preference) || ( ! wc->toggle_state)))
                            wnck_window_minimize(WNCK_WINDOW(l->data));
                        else
                            wnck_window_unminimize(WNCK_WINDOW(l->data),0);
                        break;
                    case WC_SHADE:
                        if ((( ! wc->toggle_preference) || ( ! wc->toggle_state)))
                            wnck_window_shade(WNCK_WINDOW(l->data));
                        else
                            wnck_window_unshade(WNCK_WINDOW(l->data));
                        break;
                }
            }
        }
        wincmd_adjust_toggle_state(wc);
    }
}

/* Handler for "clicked" signal on main widget. */
static gboolean wincmd_button_clicked(GtkWidget * widget, GdkEventButton * event, SimplePanel * panel)
{
    WinCmdPlugin * wc = lxpanel_plugin_get_data(widget);
    WnckScreen* scr = wnck_screen_get_default();

    /* Left-click to iconify. */
    if (event->button == 1)
    {
        GdkScreen* screen = gtk_widget_get_screen(widget);
        static GdkAtom atom = 0;
        if( G_UNLIKELY(0 == atom) )
            atom = gdk_atom_intern("_NET_SHOWING_DESKTOP", FALSE);

        /* If window manager supports _NET_SHOWING_DESKTOP, use it.
         * Otherwise, fall back to iconifying windows individually. */
        if (gdk_x11_screen_supports_net_wm_hint(screen, atom))
        {
            int showing_desktop = ((( ! wc->toggle_preference) || ( ! wc->toggle_state)) ? TRUE :FALSE);
            wnck_screen_toggle_showing_desktop(scr,showing_desktop);
            wincmd_adjust_toggle_state(wc);
        }
        else
            wincmd_execute(wc, WC_ICONIFY);
    }

    /* Middle-click to shade. */
    else if (event->button == 2)
        wincmd_execute(wc, WC_SHADE);

    return TRUE;
}

/* Plugin constructor. */
static GtkWidget *wincmd_constructor(SimplePanel *panel, GSettings *settings)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    WinCmdPlugin * wc = g_new0(WinCmdPlugin, 1);
    GtkWidget * p;

    wc->button_1_command = g_settings_get_enum(settings,WINCMD_KEY_LEFT);
    wc->button_2_command = g_settings_get_enum(settings,WINCMD_KEY_MIDDLE);
    wc->toggle_preference = g_settings_get_boolean(settings,WINCMD_KEY_TOGGLE);
    wc->image = g_settings_get_string(settings, WINCMD_KEY_IMAGE);

    /* Save construction pointers */
    wc->panel = panel;
    wc->settings = settings;

    /* Allocate top level widget and set into Plugin widget pointer. */
    p = gtk_button_new();
    GtkWidget* img = simple_panel_image_new_for_icon(panel,wc->image,-1);
    vala_panel_setup_button(p,img,NULL);
    lxpanel_plugin_set_data(p, wc, wincmd_destructor);
    gtk_widget_set_tooltip_text(p, _("Left click to iconify all windows.  Middle click to shade them."));

    /* Show the widget and return. */
    return p;
}

/* Plugin destructor. */
static void wincmd_destructor(gpointer user_data)
{
    WinCmdPlugin * wc = user_data;
    g_free(wc->image);
    g_free(wc);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static gboolean wincmd_apply_configuration(gpointer user_data)
{
    GtkWidget * p = user_data;
    WinCmdPlugin * wc = lxpanel_plugin_get_data(p);

    /* Just save settings */
    g_settings_set_enum(wc->settings,WINCMD_KEY_LEFT,wc->button_1_command);
    g_settings_set_enum(wc->settings,WINCMD_KEY_MIDDLE,wc->button_2_command);
    g_settings_set_string(wc->settings,WINCMD_KEY_IMAGE,wc->image);
    g_settings_set_boolean(wc->settings,WINCMD_KEY_TOGGLE,wc->toggle_preference);
    simple_panel_image_change_icon(p,wc->image, wc->panel);
    return FALSE;
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *wincmd_configure(SimplePanel *panel, GtkWidget *p)
{
    WinCmdPlugin * wc = lxpanel_plugin_get_data(p);
    return lxpanel_generic_config_dlg(_("Minimize All Windows"),
        panel, wincmd_apply_configuration, p,
        _("Alternately iconify/shade and raise"), &wc->toggle_preference, CONF_TYPE_BOOL,
        /* FIXME: configure buttons 1 and 2 */
        NULL);
}

FM_DEFINE_MODULE(lxpanel_gtk, wincmd)

/* Plugin descriptor. */
SimplePanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Minimize All Windows"),
    .description = N_("Sends commands to all desktop windows.\nSupported commands are 1) iconify and 2) shade"),

    .new_instance = wincmd_constructor,
    .config = wincmd_configure,
    .has_config = TRUE,
    .button_press_event = wincmd_button_clicked
};
