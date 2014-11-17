/**
 * Copyright (c) 2008 LxDE Developers, see the file AUTHORS for details.
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

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include "netstat.h"
#include "statusicon.h"
#include "misc.h"

struct statusicon *create_statusicon(SimplePanel* panel,GtkWidget *box,const char *tooltips, const char* icon_name)
{
    struct statusicon *newicon;
    newicon = malloc(sizeof(struct statusicon));

    /* main */
    newicon->main = gtk_event_box_new();

    gtk_widget_set_has_window(newicon->main, FALSE);
    gtk_widget_add_events(newicon->main, GDK_BUTTON_PRESS_MASK);
//    gtk_widget_set_size_request(newicon->main, 24, 24);
    gtk_box_pack_start(GTK_BOX(box), newicon->main, TRUE, TRUE, 0);

    /* icon */
    newicon->icon = simple_panel_image_new_for_icon(panel, icon_name, -1);
    gtk_container_add(GTK_CONTAINER(newicon->main), newicon->icon);
    gtk_widget_show_all(newicon->main);

    /* tooltip */
    gtk_widget_set_tooltip_text(newicon->main, tooltips);

    return newicon;
}

void statusicon_destroy(struct statusicon *icon)
{
    if (icon==NULL)
        return;

    gtk_widget_destroy(GTK_WIDGET(icon->icon));
    gtk_widget_destroy(GTK_WIDGET(icon->main));
    g_free(icon);
}


void update_statusicon(struct statusicon *widget, const char *icon_name)
{
    simple_panel_image_change_icon(widget->icon, icon_name);
}

void set_statusicon_tooltips(struct statusicon *widget, const char *tooltips)
{
    gtk_widget_set_tooltip_text(widget->main, tooltips);
}

void set_statusicon_visible(struct statusicon *widget, gboolean b)
{
    if (b)
        gtk_widget_show(widget->main);
    else
        gtk_widget_hide(widget->main);
}
