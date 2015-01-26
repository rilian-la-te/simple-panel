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

static void widget_center(GtkWidget* w, gpointer data)
{
    if (GTK_IS_WIDGET(w))
    {
        gtk_widget_set_halign(w, GTK_ALIGN_FILL);
        gtk_widget_set_valign(w, GTK_ALIGN_FILL);
    }
}

static void button_center(GtkWidget* b, GParamSpec* pspec, gpointer data)
{
    GtkWidget* w = gtk_bin_get_child(GTK_BIN(b));
    if (GTK_IS_CONTAINER(w))
    {
        GtkWidget* ch = (GTK_IS_BIN(w)) ? gtk_bin_get_child(GTK_BIN(w)) : w;
        if (GTK_IS_CONTAINER(ch))
            gtk_container_foreach(GTK_CONTAINER(ch),widget_center, data);
        widget_center(ch,b);
    }
}

inline void simple_panel_setup_button(GtkWidget* b, GtkWidget* img,const gchar* label)
{
    const gchar *css = ".-panel-button {\n"
            "font: inherit;"
            "margin: 0px 0px 0px 0px;\n"
            "padding: 0px 0px 0px 0px;\n"
            "}\n";
    g_signal_connect(b,"notify::image",G_CALLBACK(button_center),NULL);
    g_signal_connect(b,"notify::label",G_CALLBACK(button_center),NULL);
    if (img)
    {
        gtk_button_set_image(GTK_BUTTON(b),img);
        gtk_button_set_always_show_image(GTK_BUTTON(b),TRUE);
    }
    if (label)
        gtk_button_set_label(GTK_BUTTON(b),label);
    gtk_button_set_relief(GTK_BUTTON(b),GTK_RELIEF_NONE);
    panel_css_apply_with_class(b,css,"-panel-button",TRUE);
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
    simple_panel_setup_button(event_box,image,label);
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

void activate_menu_launch_id (GSimpleAction* action,GVariant* param, gpointer user_data)
{
    GError* err = NULL;
    const gchar* id = g_variant_get_string(param,NULL);
    GDesktopAppInfo *info = g_desktop_app_info_new(id);
    g_app_info_launch (G_APP_INFO (info), NULL, G_APP_LAUNCH_CONTEXT(gdk_display_get_app_launch_context(gdk_display_get_default())), &err);
    g_object_unref(info);
    if (err)
        g_warning("%s\n",err->message);
    g_clear_error(&err);
}

void activate_menu_launch_uri (GSimpleAction* action,GVariant* param, gpointer user_data)
{
    GError* err = NULL;
    const char* uri = g_variant_get_string(param,NULL);
    g_app_info_launch_default_for_uri(uri,
                                      G_APP_LAUNCH_CONTEXT(gdk_display_get_app_launch_context(gdk_display_get_default())),&err);
    if (err)
        g_warning("%s\n",err->message);
    g_clear_error(&err);
}

void activate_menu_launch_command (GSimpleAction* action,GVariant* param, gpointer user_data)
{
    GError* err = NULL;
    const char* commandline = g_variant_get_string(param,NULL);
    GAppInfo*  info = g_app_info_create_from_commandline(commandline, NULL,G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION,&err);
    if (err)
        g_warning("%s\n",err->message);
    g_clear_error(&err);
    g_app_info_launch(info,NULL,G_APP_LAUNCH_CONTEXT(gdk_display_get_app_launch_context(gdk_display_get_default())),&err);
    if (err)
        g_warning("%s\n",err->message);
    g_clear_error(&err);
}

gint simple_panel_apply_properties_to_menu(GList* widgets, GMenuModel* menu)
{
    GList* l;
    GMenuModel* menu_link;
    GtkWidget* menuw;
    gchar* str = NULL;
    GIcon* icon;
    int len = g_menu_model_get_n_items(menu);
    int i, j ,ret;
    for (i=0, l=widgets; (i<len) && (l!= NULL); i++,l = l->next)
    {
        while (GTK_IS_SEPARATOR_MENU_ITEM(l->data))
            l = l->next;
        menuw = gtk_menu_item_get_submenu(GTK_MENU_ITEM(l->data));
        menu_link = g_menu_model_get_item_link(menu,i,"submenu");
        if (menuw && menu_link)
        {
            simple_panel_apply_properties_to_menu(gtk_container_get_children(GTK_CONTAINER(menuw)),menu_link);
            g_object_unref(menu_link);
        }
        str = NULL;
        menu_link = g_menu_model_get_item_link(menu,i,"section");
        if (menu_link)
        {
            ret = simple_panel_apply_properties_to_menu(l,menu_link);
            for (j = 0; j < ret; j++)
                l=l->next;
            g_object_unref(menu_link);
        }
        g_menu_model_get_item_attribute(menu,i,"icon","s",&str);
        if (str)
        {
            icon = g_icon_new_for_string(str,NULL);
            g_object_set(G_OBJECT(l->data),"icon",icon,NULL);
            g_free(str);
            g_object_unref(icon);
        }
        str = NULL;
        g_menu_model_get_item_attribute(menu,i,"tooltip","s",&str);
        if (str)
        {
            gtk_widget_set_tooltip_text(GTK_WIDGET(l->data),str);
            g_free(str);
        }
    }
    return i-1;
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

void simple_panel_add_prop_as_action(GActionMap* map,const char* prop)
{
    GAction* action;
    action = G_ACTION(g_property_action_new(prop,map,prop));
    g_action_map_add_action(map,action);
    g_object_unref(action);
}

void simple_panel_add_gsettings_as_action(GActionMap* map, GSettings* settings,const char* prop)
{
    GAction* action;
    g_settings_bind(settings,prop,G_OBJECT(map),prop,G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET |G_SETTINGS_BIND_DEFAULT);
    action = G_ACTION(g_settings_create_action(settings,prop));
    g_action_map_add_action(map,action);
    g_object_unref(action);
}

void simple_panel_bind_gsettings(GObject* obj, GSettings* settings, const gchar* prop)
{
    g_settings_bind(settings,prop,obj,prop,G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET | G_SETTINGS_BIND_DEFAULT);
}

/* vim: set sw=4 et sts=4 : */
