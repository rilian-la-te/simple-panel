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

void css_apply_with_class (GtkWidget* widget, gchar* css, gchar* klass ,gboolean remove)
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

gchar* css_apply_from_file (GtkWidget* widget, gchar* file)
{
    GtkStyleContext* context;
    GtkCssProvider  *provider;
    GError* error = NULL;

    context = gtk_widget_get_style_context (widget);
    gtk_widget_reset_style(widget);
    provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_path (provider, file, &error);
    if (error)
    {
        gchar* returnie=g_strdup(error->message);
        g_error_free(error);
        return returnie;
    }
    gtk_style_context_add_provider (context,
                                    GTK_STYLE_PROVIDER (provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    return NULL;
}

inline gchar* css_generate_panel_icon_button(GdkRGBA color){
    gchar* returnie;
    color.alpha = 0.2;
    returnie = g_strdup_printf(".-panel-icon-button {\n"
                               "padding: 1px;\n"
                               " -GtkWidget-focus-line-width: 0px;\n"
                               " -GtkWidget-focus-padding: 0px;\n"
                               " background-color: transparent;\n"
                               " background-image: none;\n"
                               "}\n"
                               ".-panel-icon-button:hover,"
                               ".-panel-icon-button.highlight,"
                               ".-panel-icon-button:hover:active,"
                               ".-panel-icon-button:active:hover {\n"
                               "color: %s;"
                               "}\n",gdk_rgba_to_string(&color));
    return returnie;
}


inline gchar* css_generate_background(const char *filename, GdkRGBA color,gboolean no_image)
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

inline gchar* css_generate_font_color(GdkRGBA color){
    return g_strdup_printf(".-lxpanel-font-color{\n"
                    " color: %s;\n"
                    "}",gdk_rgba_to_string(&color));
}
inline gchar* css_generate_font_size(gint size){
    return g_strdup_printf(".-lxpanel-font-size{\n"
                    " font-size: %dpx;\n"
                    "}",size);
}
inline gchar* css_generate_font_weight(gboolean is_bold){
    return g_strdup_printf(".-lxpanel-font-weight{\n"
                    " font-weight: %s;\n"
                    "}",is_bold ? "bold" : "normal");
}
