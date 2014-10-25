/*
 * fb-background-monitor.c:
 *
 * Copyright (C) 2001, 2002 Ian McKellar <yakk@yakk.net>
 *                     2002 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Authors:
 *      Ian McKellar <yakk@yakk.net>
 *      Mark McLoughlin <mark@skynet.ie>
 */

#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>

#include "css.h"

//#define DEBUG
#include "dbg.h"

void fb_bg_apply_css (GtkWidget* widget, gchar* css, gchar* klass ,gboolean remove)
{
    GtkStyleContext* context;
    GtkCssProvider  *provider;

    context = gtk_widget_get_style_context (widget);
    gtk_widget_reset_style(widget);

    if (remove) {
        gtk_style_context_remove_class (context, klass);
    }
    else
    {
        provider = gtk_css_provider_new ();
        gtk_css_provider_load_from_data (provider, css,
                                        -1, NULL);
        gtk_style_context_add_class (context, klass);
        gtk_style_context_add_provider (context,
                                        GTK_STYLE_PROVIDER (provider),
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
}

gchar* fb_bg_generate_string(const char *filename, GdkRGBA color,gboolean no_image)
{
	gchar* returnie;
	if (no_image) returnie = g_strdup_printf(".-lxpanel-background{\n"
					" background-color: %s;\n"
					" background-image: none;\n"
					"}",gdk_rgba_to_string(&color));
	else returnie = g_strdup_printf(".-lxpanel-background{\n"
						 " background-color: %s;\n"
                         " background-image: url('%s');\n"
						 "}",gdk_rgba_to_string(&color),filename);
	return returnie;
}
