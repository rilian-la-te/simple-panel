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

#include <gtk/gtk.h>
#include <stdlib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>

#include "plugin.h"

#include "netstatus-icon.h"
#include "netstatus-dialog.h"

#define NETSTATUS_KEY_INTERFACE "interface"
#define NETSTATUS_KEY_CONFTOOL "config-tool"

typedef struct {
    GSettings *settings;
    char *iface;
    char *config_tool;
    GtkWidget *dlg;
} netstatus;


static void on_response( GtkDialog* dlg, gint response, netstatus *ns );

static void
netstatus_destructor(gpointer user_data)
{
    netstatus *ns = (netstatus *)user_data;

    /* The widget is destroyed in plugin_stop().
    gtk_widget_destroy(ns->mainw);
    */
    g_free( ns->iface );
    g_free( ns->config_tool );
    if (ns->dlg)
    {
        g_signal_handlers_disconnect_by_func(ns->dlg, on_response, ns);
        gtk_widget_destroy(ns->dlg);
    }
    g_free(ns);
	return;
}

static void on_response( GtkDialog* dlg, gint response, netstatus *ns )
{
    const char* iface;
    switch( response )
    {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_NONE:
        iface = netstatus_dialog_get_iface_name((GtkWidget*)dlg);
        if( iface )
        {
            g_free(ns->iface);
            ns->iface = g_strdup(iface);
            gtk_widget_destroy( GTK_WIDGET(dlg) );
            ns->dlg = NULL;
        }
    }
}

static gboolean on_button_press( GtkWidget* widget, GdkEventButton* evt, SimplePanel *p )
{
    NetstatusIface* iface;
    netstatus *ns = lxpanel_plugin_get_data(widget);

    if( evt->button == 1 ) /*  Left click*/
    {
        if( ! ns->dlg )
        {
            iface = netstatus_icon_get_iface( NETSTATUS_ICON(widget) );
            ns->dlg = netstatus_dialog_new(iface);
#if !GTK_CHECK_VERSION (3,0,0)
            /* fix background */
            gtk_widget_set_style(ns->dlg, panel_get_defstyle(p));
#endif
            netstatus_dialog_set_configuration_tool( ns->dlg, ns->config_tool );
            g_signal_connect( ns->dlg, "response", G_CALLBACK(on_response), ns );
        }
        gtk_window_present( GTK_WINDOW(ns->dlg) );
    }
    return TRUE;
}

static GtkWidget *
netstatus_constructor(SimplePanel *panel, GSettings *settings)
{
    netstatus *ns;
    NetstatusIface* iface;
    GtkWidget *p;

    ns = g_new0(netstatus, 1);
    ns->settings = settings;
    g_return_val_if_fail(ns != NULL, NULL);

    ns->iface = g_settings_get_string(settings,NETSTATUS_KEY_INTERFACE);
    ns->config_tool = g_settings_get_string(settings,NETSTATUS_KEY_CONFTOOL);

    iface = netstatus_iface_new(ns->iface);
    p = netstatus_icon_new( iface );
    lxpanel_plugin_set_data(p, ns, netstatus_destructor);
    netstatus_icon_set_show_signal((NetstatusIcon *)p, TRUE);
    gtk_widget_add_events( p, GDK_BUTTON_PRESS_MASK );
    g_object_unref( iface );

	return p;
}

static gboolean apply_config(gpointer user_data)
{
    GtkWidget *p = user_data;
    netstatus *ns = lxpanel_plugin_get_data(p);
    NetstatusIface* iface;

    iface = netstatus_iface_new(ns->iface);
    netstatus_icon_set_iface((NetstatusIcon *)p, iface);
    g_object_unref(iface);
    g_settings_set_string(ns->settings, NETSTATUS_KEY_INTERFACE, ns->iface);
    g_settings_set_string(ns->settings, NETSTATUS_KEY_CONFTOOL, ns->config_tool);
    return FALSE;
}

static GtkWidget *netstatus_config(SimplePanel *panel, GtkWidget *p)
{
    GtkWidget* dlg;
    netstatus *ns = lxpanel_plugin_get_data(p);
    dlg = lxpanel_generic_config_dlg(_("Network Status Monitor"),
                panel, apply_config, p,
                _("Interface to monitor"), &ns->iface, CONF_TYPE_STR,
                _("Config tool"), &ns->config_tool, CONF_TYPE_STR,
                NULL );
    return dlg;
}


FM_DEFINE_MODULE(lxpanel_gtk, netstatus)

SimplePanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Network Status Monitor"),
    .description = N_("Monitor network status"),

    .new_instance = netstatus_constructor,
    .config = netstatus_config,
    .button_press_event = on_button_press,
    .has_config = TRUE
};
