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

static void on_theme_changed(GtkWidget* img, GObject* object);
static void _gtk_image_set_panel(GtkWidget* img, GIcon *icon, SimplePanel* panel, gint size);

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

void calculate_position(SimplePanel *np)
{
    GdkRectangle rect;
    rect.width = np->priv->aw;
    rect.height = np->priv->ah;
    _calculate_position(np, &rect);
    np->priv->aw = rect.width;
    np->priv->ah = rect.height;
    np->priv->ax = rect.x;
    np->priv->ay = rect.y;
}

gchar *
expand_tilda(const gchar *file)
{
    return ((file[0] == '~') ?
        g_strdup_printf("%s%s", getenv("HOME"), file+1)
        : g_strdup(file));
}

static void on_theme_changed(GtkWidget *img, GObject *object)
{
    int size = gtk_image_get_pixel_size(GTK_IMAGE(img));
    GIcon* icon;
    gtk_image_get_gicon(GTK_IMAGE(img),&icon,NULL);
    _gtk_image_set_panel(img,icon,NULL,size);
}

static void _gtk_image_set_panel(GtkWidget* img, GIcon *icon, SimplePanel* panel, gint size)
{
    if(icon)
        gtk_image_set_from_gicon(GTK_IMAGE(img),icon,GTK_ICON_SIZE_INVALID);
    else
        return;
    if(panel)
        g_object_bind_property(panel,PANEL_PROP_ICON_SIZE,img,"pixel-size",G_BINDING_SYNC_CREATE | G_BINDING_DEFAULT);
    else
        gtk_image_set_pixel_size(GTK_IMAGE(img),size);
    g_signal_connect_swapped(gtk_icon_theme_get_default(),"changed",G_CALLBACK(on_theme_changed),img);
}

GtkWidget* simple_panel_image_new_for_gicon(SimplePanel *p, GIcon *icon, gint size)
{
    GtkWidget* img = gtk_image_new();
    _gtk_image_set_panel(img,icon,p,size);
    return img;
}

GtkWidget* simple_panel_image_new_for_icon(SimplePanel * p,const gchar *name, gint height)
{
    GIcon* icon = g_icon_new_for_string(name,NULL);
    GtkWidget* ret = simple_panel_image_new_for_gicon(p,icon,height);
    g_object_unref(icon);
    return ret;
}

void simple_panel_image_change_gicon(GtkWidget *img, GIcon *icon, SimplePanel* p)
{
    _gtk_image_set_panel(img,icon,p,-1);
}

void simple_panel_image_change_icon(GtkWidget *img, const gchar *name,SimplePanel* p)
{
    GIcon* icon = g_icon_new_for_string(name,NULL);
    simple_panel_image_change_gicon(img,icon,p);
    g_object_unref(icon);
    return;
}

static GtkWidget *_simple_panel_button_new_for_icon(SimplePanel* panel,GIcon *icon,
                                               gint size, const GdkRGBA* color,
                                               const gchar *label)
{
    const gchar* css = ".-panel-icon-button {\n"
                       "  padding: 0px;\n"
                       "  margin: 0px;\n"
                       " -GtkWidget-focus-line-width: 0px;\n"
                       " -GtkWidget-focus-padding: 0px;\n"
                       "}\n"
                       ".-panel-icon-button:hover,"
                       ".-panel-icon-button:prelight,"
                       ".-panel-icon-button.highlight,"
                       ".-panel-icon-button:active:hover {\n"
                       " -gtk-image-effect: highlight;"
                       "}\n";
    GtkWidget * event_box = gtk_button_new();
    GtkWidget* image = NULL;
    if (icon)
        image = simple_panel_image_new_for_gicon(panel,icon, size);
	vala_panel_setup_button(event_box,image,label);
    panel_css_apply_with_class(event_box,NULL,GTK_STYLE_CLASS_BUTTON,FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(event_box), 0);
    gtk_widget_set_can_focus(event_box, FALSE);
    gtk_widget_set_has_window(event_box,FALSE);
    panel_css_apply_with_class(event_box,css,"-panel-icon-button",TRUE);
    return event_box;
}

/* consumes reference on icon */
static void _simple_panel_button_set_icon(GtkWidget* btn, GIcon* icon,SimplePanel* p, gint size)
{
    /* Locate the image within the button. */
    GtkWidget * img = gtk_button_get_image(GTK_BUTTON(btn));
    _gtk_image_set_panel(img,icon,p,size);
}

void simple_panel_button_set_icon(GtkWidget* btn, const gchar *name, SimplePanel* p, gint size)
{
    GIcon* icon = g_icon_new_for_string(name,NULL);
    _simple_panel_button_set_icon(btn, icon, p, size);
    g_object_unref(icon);
}

GtkWidget *simple_panel_button_new_for_icon(SimplePanel *panel, const gchar *name, GdkRGBA *color, const gchar *label)
{
    GdkRGBA fallback = {1,1,1,0.15};
    GIcon* icon = g_icon_new_for_string(name,NULL);
    GtkWidget* ret = _simple_panel_button_new_for_icon(panel,icon,
                                        -1, (color != NULL) ? color : &fallback, label);
    g_object_unref(icon);
    return ret;
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

void start_panels_from_dir(GtkApplication* app,const char *panel_dir)
{
    GDir* dir = g_dir_open( panel_dir, 0, NULL );
    const gchar* name;

    if( ! dir )
    {
        return;
    }

    while((name = g_dir_read_name(dir)) != NULL)
    {
        char* panel_config = g_build_filename( panel_dir, name, NULL );
        if ((strchr(panel_config, '~') == NULL) && (name[0] != '.'))    /* Skip editor backup files in case user has hand edited in this directory */
        {
            SimplePanel* panel = panel_load(app,panel_config, name );
            if( panel )
                gtk_application_add_window(app,GTK_WINDOW(panel));
        }
        g_free( panel_config );
    }
    g_dir_close( dir );
}

void simple_panel_scale_button_set_range (GtkScaleButton* b, gint lower, gint upper)
{
    gtk_adjustment_set_lower(gtk_scale_button_get_adjustment(b),lower);
    gtk_adjustment_set_upper(gtk_scale_button_get_adjustment(b),upper);
    gtk_adjustment_set_step_increment(gtk_scale_button_get_adjustment(b),1);
    gtk_adjustment_set_page_increment(gtk_scale_button_get_adjustment(b),1);
}

void simple_panel_scale_button_set_value_labeled (GtkScaleButton* b, gint value)
{
    gtk_scale_button_set_value(b,value);
    gchar* str = g_strdup_printf("%d",value);
    gtk_button_set_label(GTK_BUTTON(b),str);
    g_free(str);
}

void simple_panel_bind_gsettings(GObject* obj, GSettings* settings, const gchar* prop)
{
    g_settings_bind(settings,prop,obj,prop,G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET | G_SETTINGS_BIND_DEFAULT);
}

/* vim: set sw=4 et sts=4 : */
