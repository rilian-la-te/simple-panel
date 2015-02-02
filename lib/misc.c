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
#include <math.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "misc.h"
#include "private.h"
#include "vala.h"

static void
calculate_width(int scrw, int wtype, int align, int margin,
      int *panw, int *x)
{
    if (wtype == SIZE_PERCENT) {
        /* sanity check */
        if (*panw > 100)
            *panw = 100;
        else if (*panw < 0)
            *panw = 1;
        *panw = ((gfloat) scrw * (gfloat) *panw) / 100.0;
    }
    if (align != ALIGN_CENTER) {
        if (margin > scrw) {
            g_warning( "margin is bigger then edge size %d > %d. Ignoring margin",
                  margin, scrw);
            margin = 0;
        }
	*panw = MIN(scrw - margin, *panw);
    }
    if (align == ALIGN_LEFT)
        *x += margin;
    else if (align == ALIGN_RIGHT) {
        *x += scrw - *panw - margin;
        if (*x < 0)
            *x = 0;
    } else if (align == ALIGN_CENTER)
        *x += (scrw - *panw) / 2;
}


void _calculate_position(SimplePanel *panel, GdkRectangle* rect)
{
    Panel *np = panel->priv;
    GdkScreen *screen;
    GdkRectangle marea;

    screen = gdk_screen_get_default();
    if (np->monitor < 0) /* all monitors */
    {
        marea.x = 0;
        marea.y = 0;
        marea.width = gdk_screen_get_width(screen);
        marea.height = gdk_screen_get_height(screen);
    }
    else if (np->monitor < gdk_screen_get_n_monitors(screen))
        gdk_screen_get_monitor_geometry(screen,np->monitor,&marea);
    else
    {
        marea.x = 0;
        marea.y = 0;
        marea.width = 0;
        marea.height = 0;
    }
    if (np->edge == GTK_POS_TOP || np->edge == GTK_POS_BOTTOM) {
        rect->width = np->width;
        rect->x = marea.x;
        calculate_width(marea.width, np->widthtype, np->align, np->margin,
              &rect->width, &rect->x);
        rect->height = ((( ! np->autohide) || (np->visible)) ? np->height : np->height_when_hidden);
        rect->y = marea.y + ((np->edge == GTK_POS_TOP) ? 0 : (marea.height - rect->height));

    } else {
        rect->height = np->width;
        rect->y = marea.y;
        calculate_width(marea.height, np->widthtype, np->align, np->margin,
              &rect->height, &rect->y);
        rect->width = ((( ! np->autohide) || (np->visible)) ? np->height : np->height_when_hidden);
        rect->x = marea.x + ((np->edge == GTK_POS_LEFT) ? 0 : (marea.width - rect->width));
    }
    return;
}

GtkWidget* simple_panel_image_new_for_gicon(SimplePanel *p, GIcon *icon, gint size)
{
    GtkWidget* img = gtk_image_new();
	vala_panel_setup_icon(GTK_IMAGE(img),icon,p,size);
    return img;
}

GtkWidget* simple_panel_image_new_for_icon(SimplePanel * p,const gchar *name, gint height)
{
    GIcon* icon = g_icon_new_for_string(name,NULL);
    GtkWidget* ret = simple_panel_image_new_for_gicon(p,icon,height);
    g_object_unref(icon);
    return ret;
}

GtkWidget *simple_panel_button_new_for_icon(SimplePanel *panel, const gchar *name, GdkRGBA *color, const gchar *label)
{
    GIcon* icon = g_icon_new_for_string(name,NULL);
	GtkWidget * event_box = gtk_button_new();
	vala_panel_setup_icon_button(GTK_BUTTON(event_box),icon,label,panel);
    g_object_unref(icon);
	return event_box;
}

void activate_panel_preferences(GSimpleAction* simple, GVariant* param, gpointer data)
{
	GtkApplication* app = (GtkApplication*)data;
	const gchar* par = g_variant_get_string(param, NULL);
	GList* all_panels = gtk_application_get_windows(app);
	GList* l;
	for( l = all_panels; l; l = l->next )
	{
		gchar* name;
		SimplePanel* p = (SimplePanel*)l->data;
		g_object_get(p,"name",&name,NULL);
		if (!g_strcmp0(par,name))
			panel_configure(p, "geometry");
		else
			g_warning("No panel with this name found.\n");
		g_free(name);
	}
}

void activate_menu(GSimpleAction* simple, GVariant* param, gpointer data)
{
	GtkApplication* app = GTK_APPLICATION(data);
	GList* l;
	GList* all_panels = gtk_application_get_windows(app);
	for( l = all_panels; l; l = l->next )
	{
		SimplePanel* p = (SimplePanel*)l->data;
		GList *plugins, *pl;

		plugins = gtk_container_get_children(GTK_CONTAINER(p->priv->box));
		for (pl = plugins; pl; pl = pl->next)
		{
			const SimplePanelPluginInit *init = PLUGIN_CLASS(pl->data);
			if (init->show_system_menu)
			/* queue to show system menu */
				init->show_system_menu(pl->data);
		}
		g_list_free(plugins);
	}
}

/* vim: set sw=4 et sts=4 : */
