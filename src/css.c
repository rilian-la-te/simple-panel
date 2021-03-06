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

#include <math.h>

#include "css.h"

void css_apply_with_class (GtkWidget* widget,const gchar* css, gchar* klass ,gboolean remove)
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
        g_object_unref(provider);
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
        g_clear_error(&error);
        return returnie;
    }
    gtk_style_context_add_provider (context,
                                    GTK_STYLE_PROVIDER (provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    return NULL;
}

gchar* css_apply_from_file_to_app (gchar* file)
{
    GtkCssProvider  *provider;
    GError* error = NULL;

    provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_path (provider, file, &error);
    if (error)
    {
        gchar* returnie=g_strdup(error->message);
        g_clear_error(&error);
        return returnie;
    }
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                    GTK_STYLE_PROVIDER (provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    return NULL;
}


inline gchar* css_generate_background(const char *filename, GdkRGBA color,gboolean no_image)
{
	gchar* returnie;
    gchar* str = gdk_rgba_to_string(&color);
	if (no_image) returnie = g_strdup_printf(".-simple-panel-background{\n"
					" background-color: %s;\n"
					" background-image: none;\n"
                    "}",str);
	else returnie = g_strdup_printf(".-simple-panel-background{\n"
                         " background-color: transparent;\n"
                         " background-image: url('%s');\n"
                         "}",filename);
    g_free(str);
	return returnie;
}

inline gchar* css_generate_font_color(GdkRGBA color){
    gchar* color_str = gdk_rgba_to_string(&color);
    gchar* ret;
    ret = g_strdup_printf(".-simple-panel-font-color{\n"
                    "color: %s;\n"
                    "}",color_str);
    g_free(color_str);
    return ret;
}
inline gchar* css_generate_font_size(gint size){
    return g_strdup_printf(".-simple-panel-font-size{\n"
                    " font-size: %dpx;\n"
                    "}",size);
}
inline gchar* css_generate_font_label(gfloat size, gboolean is_bold)
{
    gint size_factor = (gint)round(size*100);
    return g_strdup_printf(".-simple-panel-font-label{\n"
                    " font-size: %d%%;\n"
                    " font-weight: %s;\n"
                    "}",size_factor,is_bold ? "bold" : "normal");
}

inline gchar* css_generate_flat_button(GtkWidget* widget,SimplePanel* panel){
    gchar* returnie;
    GdkRGBA color, active_color;
    gchar* c_str;
    gchar* act_str;
    gtk_style_context_get_color(
                gtk_widget_get_style_context(GTK_WIDGET(panel)),
                gtk_widget_get_state_flags(GTK_WIDGET(panel)),
                &color);
    color.alpha = 0.8;
    active_color.red=color.red;
    active_color.green=color.green;
    active_color.blue=color.blue;
    active_color.alpha =0.5;
    c_str = gdk_rgba_to_string(&color);
    act_str = gdk_rgba_to_string(&active_color);
    const gchar* edge;
    GtkPositionType direction;
    g_object_get(panel,PANEL_PROP_EDGE,&direction,NULL);
    if (direction==GTK_POS_BOTTOM)
        edge="0px 0px 2px 0px";
    if (direction==GTK_POS_TOP)
        edge="2px 0px 0px 0px";
    if (direction==GTK_POS_RIGHT)
        edge="0px 2px 0px 0px";
    if (direction==GTK_POS_LEFT)
        edge="0px 0px 0px 2px";
    returnie = g_strdup_printf(".-panel-flat-button {\n"
                               "padding: 0px;\n"
                               " -GtkWidget-focus-line-width: 0px;\n"
                               " -GtkWidget-focus-padding: 0px;\n"
                               "border-style: solid;"
                               "border-color: transparent;"
                               "border-width: %s;"
                               "}\n"
#if GTK_CHECK_VERSION (3, 14, 0)
                               ".-panel-flat-button:checked,"
#endif
                               ".-panel-flat-button:active {\n"
                               "border-style: solid;"
                               "border-width: %s;"
                               "border-color: %s;"
                               "}\n"
                               ".-panel-flat-button:hover,"
                               ".-panel-flat-button.highlight,"
                               ".-panel-flat-button:active:hover {\n"
                               "border-style: solid;"
                               "border-width: %s;"
                               "border-color: %s;"
                               "}\n",edge,edge,act_str,edge,c_str);
    g_free(act_str);
    g_free(c_str);
    return returnie;
}
