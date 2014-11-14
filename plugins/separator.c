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

#include "plugin.h"

#include <glib/gi18n.h>

/* Plugin constructor. */
static GtkWidget *separator_constructor(SimplePanel *panel, GSettings *settings)
{
    GtkWidget *instance, *sep;

    /* Allocate top level widget and set into Plugin widget pointer. */
    instance = gtk_event_box_new();
    gtk_widget_set_has_window(instance, FALSE);
    gtk_widget_add_events(instance, GDK_BUTTON_PRESS_MASK);
    gtk_container_set_border_width(GTK_CONTAINER(instance), 1);

    /* Allocate separator as a child of top level. */
    sep = panel_separator_new(panel);
    gtk_container_add(GTK_CONTAINER(instance), sep);
    gtk_widget_show(sep);

    /* Show the widget and return. */
    return instance;
}

/* Callback when panel configuration changes. */
static void separator_reconfigure(SimplePanel *panel, GtkWidget *instance)
{
    /* Determine if the orientation changed in a way that requires action. */
    GtkWidget * sep = gtk_bin_get_child(GTK_BIN(instance));
    gtk_orientable_set_orientation(GTK_ORIENTABLE(sep),panel_get_orientation(panel));
}

/* Plugin descriptor. */
SimplePanelPluginInit lxpanel_static_plugin_separator = {
    .name = N_("Separator"),
    .description = N_("Add a separator to the panel"),

    .new_instance = separator_constructor,
    .reconfigure = separator_reconfigure
};
