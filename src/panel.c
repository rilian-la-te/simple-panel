/*
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <libfm/fm-gtk.h>

#define __LXPANEL_INTERNALS__

#include "app.h"
#include "private.h"
#include "misc.h"
#include "css.h"
#include "dbg.h"
#include "panel-enum-types.h"

enum
{
    PROP_0,
    PROP_NAME,
    PROP_EDGE,
    PROP_ORIENTATION,
    PROP_HEIGHT,
    PROP_WIDTH,
    PROP_SIZE_TYPE,
    PROP_ALIGNMENT,
    PROP_MARGIN,
    PROP_MONITOR,
    PROP_AUTOHIDE,
    PROP_AUTOHIDE_SIZE,
    PROP_ENABLEFONTCOLOR,
    PROP_ENABLEFONTSIZE,
    PROP_BACKGROUNDFILE,
    PROP_BACKGROUNDTYPE,
    PROP_TINTCOLOR,
    PROP_FONTCOLOR,
    PROP_FONTSIZE,
    PROP_ICON_SIZE,
    PROP_DOCK,
    PROP_STRUT,
    PROP_NUMBER
};

static GtkApplication* win_grp;

GList* all_panels = NULL;

gboolean is_restarting = FALSE;

gboolean is_in_lxde = FALSE;

gchar* cprofile = NULL;

void update_panel_geometry( SimplePanel* p );
static void panel_start_gui(SimplePanel *p);
static void ah_start(SimplePanel *p);
static void ah_stop(SimplePanel *p);
static void _panel_update_background(SimplePanel * p);
static void _panel_update_fonts(SimplePanel * p);
static void activate_remove_plugin(GSimpleAction* action, GVariant* param, gpointer pl);
static void activate_new_panel(GSimpleAction* action, GVariant* param, gpointer data);
static void activate_remove_panel(GSimpleAction* action, GVariant* param, gpointer data);
static void activate_panel_settings(GSimpleAction* action, GVariant* param, gpointer data);
static gboolean _panel_set_monitor(SimplePanel* panel, int monitor);
static void panel_add_actions( SimplePanel* p);
void simple_panel_config_save( SimplePanel* panel );

G_DEFINE_TYPE(PanelWindow, lxpanel, GTK_TYPE_APPLICATION_WINDOW)

static inline char *_system_config_file_name(const char *dir, const char *file_name)
{
    return g_build_filename(dir, "simple-panel", cprofile, file_name, NULL);
}

static inline char *_user_config_file_name(const char *name1, const char *name2)
{
    return g_build_filename(g_get_user_config_dir(), "simple-panel", cprofile, name1,
                            name2, NULL);
}

static void lxpanel_finalize(GObject *object)
{
    SimplePanel *self = LXPANEL(object);
    Panel *p = self->priv;

    if( p->config_changed )
        simple_panel_config_save( self );
    config_destroy(p->config);

    g_free( p->background_file );

    g_free( p->name );
    g_free(p);

    G_OBJECT_CLASS(lxpanel_parent_class)->finalize(object);
}

static void lxpanel_destroy(GtkWidget *object)
{
    SimplePanel *self = LXPANEL(object);
    Panel *p = self->priv;

    if (p->autohide)
        ah_stop(self);

    if (p->pref_dialog != NULL)
        gtk_widget_destroy(p->pref_dialog);
    p->pref_dialog = NULL;

    if (p->plugin_pref_dialog != NULL)
        /* just close the dialog, it will do all required cleanup */
        gtk_dialog_response(GTK_DIALOG(p->plugin_pref_dialog), GTK_RESPONSE_CLOSE);

    if (p->initialized)
    {
        gtk_application_remove_window(GTK_APPLICATION(win_grp), GTK_WINDOW(self));
        gdk_flush();
        p->initialized = FALSE;
    }

    if (p->background_update_queued)
    {
        g_source_remove(p->background_update_queued);
        p->background_update_queued = 0;
    }
	GTK_WIDGET_CLASS(lxpanel_parent_class)->destroy(object);
}

static gboolean idle_update_background(gpointer p)
{
    SimplePanel *panel = LXPANEL(p);

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    /* Panel could be destroyed while background update scheduled */
    if (gtk_widget_get_realized(p))
    {
        gdk_display_sync( gtk_widget_get_display(p) );
        _panel_update_background(panel);
    }
    panel->priv->background_update_queued = 0;

    return FALSE;
}

void _panel_queue_update_background(SimplePanel *panel)
{
    if (panel->priv->background_update_queued)
        return;
    panel->priv->background_update_queued = g_idle_add_full(G_PRIORITY_HIGH,
                                                            idle_update_background,
                                                            panel, NULL);
}

static void lxpanel_realize(GtkWidget *widget)
{
    GTK_WIDGET_CLASS(lxpanel_parent_class)->realize(widget);

    _panel_queue_update_background(LXPANEL(widget));
}

static void lxpanel_size_allocate(GtkWidget *widget, GtkAllocation *a)
{
    Panel *p = LXPANEL(widget)->priv;

	GTK_WIDGET_CLASS(lxpanel_parent_class)->size_allocate(widget, a);
    gtk_widget_set_allocation(widget,a);
    if (p->widthtype == PANEL_SIZE_DYNAMIC)
        p->width = (p->orientation == GTK_ORIENTATION_HORIZONTAL) ? a->width : a->height;
    if (p->heighttype == PANEL_SIZE_DYNAMIC)
        p->height = (p->orientation == GTK_ORIENTATION_HORIZONTAL) ? a->height : a->width;

    calculate_position(p);
    if (a->width != p->aw || a->height != p->ah || a->x != p->ax || a->y != p->ay)
    {
        gtk_window_move(GTK_WINDOW(widget), p->ax, p->ay);
        _panel_set_wm_strut(LXPANEL(widget));
    }
    else
		if (p->background_update_queued)
    {
        g_source_remove(p->background_update_queued);
        p->background_update_queued = 0;

        if (gtk_widget_get_realized(widget))
            _panel_update_background(LXPANEL(widget));
    }
}

static void
lxpanel_get_preferred_width (GtkWidget *widget,
							   gint      *minimal_width,
							   gint      *natural_width)
{
  Panel *p = LXPANEL(widget)->priv;
  GTK_WIDGET_CLASS(lxpanel_parent_class)->get_preferred_width(widget, minimal_width,natural_width);
  if (p->widthtype == PANEL_SIZE_DYNAMIC && (p->orientation == GTK_ORIENTATION_HORIZONTAL))
  {
      *minimal_width= (*natural_width<=gdk_screen_width()) ? *natural_width : gdk_screen_width();
      p->width = *minimal_width;
  }
  else
      *minimal_width=p->aw;
  *natural_width=*minimal_width;
  if (!p->visible)
      /* When the panel is in invisible state, the content box also got hidden, thus always
       * report 0 size.  Ask the content box instead for its size. */
      gtk_widget_get_preferred_width(p->box, minimal_width, natural_width);
  calculate_position(p);
}

static void
lxpanel_get_preferred_height (GtkWidget *widget,
								gint      *minimal_height,
								gint      *natural_height)
{
    Panel *p = LXPANEL(widget)->priv;
    GTK_WIDGET_CLASS(lxpanel_parent_class)->get_preferred_height(widget, minimal_height,natural_height);
    if (p->widthtype == PANEL_SIZE_DYNAMIC && p->orientation == GTK_ORIENTATION_VERTICAL)
    {
        *minimal_height= (*natural_height<=gdk_screen_height()) ? *natural_height : gdk_screen_height();
        p->height = *minimal_height;
    }
    else
        *minimal_height=p->ah;
    *natural_height=*minimal_height;
    if (!p->visible)
        /* When the panel is in invisible state, the content box also got hidden, thus always
         * report 0 size.  Ask the content box instead for its size. */
        gtk_widget_get_preferred_height(p->box, minimal_height, natural_height);
    calculate_position(p);
}

static gboolean lxpanel_configure_event (GtkWidget *widget, GdkEventConfigure *e)
{
    Panel *p = LXPANEL(widget)->priv;

    if (e->width == p->cw && e->height == p->ch && e->x == p->cx && e->y == p->cy)
        goto ok;
    p->cw = e->width;
    p->ch = e->height;
    p->cx = e->x;
    p->cy = e->y;

ok:
    return GTK_WIDGET_CLASS(lxpanel_parent_class)->configure_event(widget, e);
}

static gboolean lxpanel_map_event(GtkWidget *widget, GdkEventAny *event)
{
    Panel *p = PLUGIN_PANEL(widget)->priv;

    if (p->autohide)
        ah_start(LXPANEL(widget));
    return GTK_WIDGET_CLASS(lxpanel_parent_class)->map_event(widget, event);
}

/* Handler for "button_press_event" signal with Panel as parameter. */
static gboolean lxpanel_button_press(GtkWidget *widget, GdkEventButton *event)
{
    if (event->button == 3) /* right button */
    {
        GtkMenu* popup = (GtkMenu*) lxpanel_get_plugin_menu(LXPANEL(widget), NULL);
        gtk_menu_popup(popup, NULL, NULL, NULL, NULL, event->button, event->time);
        return TRUE;
    }
    return FALSE;
}

static void simple_panel_set_property(GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
    SimplePanel* toplevel;
    g_return_if_fail (LX_IS_PANEL (object));

    toplevel = LXPANEL (object);
    gboolean geometry,background,configuration,fonts;
    switch (prop_id) {
    case PROP_NAME:
        toplevel->priv->name = g_strdup(g_value_get_string(value));
        break;
    case PROP_EDGE:
        toplevel->priv->edge = g_value_get_enum (value);
        configuration = TRUE;
        geometry = TRUE;
        break;
    case PROP_HEIGHT:
        toplevel->priv->height = g_value_get_int(value);
        geometry = TRUE;
        break;
    case PROP_WIDTH:
        toplevel->priv->width = g_value_get_int(value);
        geometry = TRUE;
        break;
    case PROP_SIZE_TYPE:
        toplevel->priv->widthtype = g_value_get_enum(value);
        geometry = TRUE;
        break;
    case PROP_MONITOR:
        _panel_set_monitor(toplevel,g_value_get_int(value));
        geometry = TRUE;
        configuration = TRUE;
        break;
    case PROP_ALIGNMENT:
        toplevel->priv->allign = g_value_get_enum(value);
        geometry = TRUE;
        break;
    case PROP_MARGIN:
        toplevel->priv->margin = g_value_get_int(value);
        geometry = TRUE;
        break;
    case PROP_BACKGROUNDTYPE:
        toplevel->priv->background = g_value_get_enum(value);
        background = TRUE;
        configuration = TRUE;
        break;
    case PROP_AUTOHIDE:
        toplevel->priv->autohide = g_value_get_boolean(value);
        toplevel->priv->visible = toplevel->priv->autohide ? FALSE : TRUE;
        geometry=TRUE;
        break;
    case PROP_AUTOHIDE_SIZE:
        toplevel->priv->height_when_hidden = g_value_get_int(value);
        geometry=TRUE;
        break;
    case PROP_ENABLEFONTCOLOR:
        toplevel->priv->usefontcolor = g_value_get_boolean (value);
        fonts = TRUE;
        configuration = TRUE;
        break;
    case PROP_ENABLEFONTSIZE:
        toplevel->priv->usefontsize = g_value_get_boolean (value);
        fonts = TRUE;
        configuration = TRUE;
        break;
    case PROP_BACKGROUNDFILE:
        if (toplevel->priv->background_file)
            g_free(toplevel->priv->background_file);
        toplevel->priv->background_file = g_strdup(g_value_get_string(value));
        background=TRUE;
        break;
    case PROP_TINTCOLOR:
        gdk_rgba_parse(&toplevel->priv->gtintcolor,g_value_get_string(value));
        background = TRUE;
        break;
    case PROP_FONTCOLOR:
        gdk_rgba_parse(&toplevel->priv->gfontcolor,g_value_get_string(value));
        fonts = TRUE;
        configuration = TRUE;
        break;
    case PROP_FONTSIZE:
        toplevel->priv->fontsize = g_value_get_int(value);
        fonts = TRUE;
        configuration = TRUE;
        break;
    case PROP_ICON_SIZE:
        toplevel->priv->icon_size = g_value_get_int(value);
        configuration = TRUE;
        break;
    case PROP_DOCK:
        toplevel->priv->setdocktype = g_value_get_boolean(value);
        panel_set_dock_type(toplevel);
        geometry = TRUE;
        break;
    case PROP_STRUT:
        toplevel->priv->setstrut = g_value_get_boolean(value);
        geometry = TRUE;
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
    if (gtk_widget_get_window(GTK_WIDGET(toplevel))!= NULL)
    {
        if (configuration)
            _panel_set_panel_configuration_changed(toplevel);
        if (geometry)
            update_panel_geometry(toplevel);
        if (background)
            _panel_update_background(toplevel);
        if (fonts)
            _panel_update_fonts(toplevel);
    }
}

static void simple_panel_get_property(GObject      *object,
                                      guint         prop_id,
                                      GValue *value,
                                      GParamSpec   *pspec)
{
    SimplePanel* toplevel;
    g_return_if_fail (LX_IS_PANEL (object));

    toplevel = LXPANEL (object);

    switch (prop_id) {
    case PROP_NAME:
        g_value_set_string(value,toplevel->priv->name);
        break;
    case PROP_EDGE:
        g_value_set_enum (value,toplevel->priv->edge);
        break;
    case PROP_ORIENTATION:
        g_value_set_enum(value,toplevel->priv->orientation);
    case PROP_HEIGHT:
        g_value_set_int (value, toplevel->priv->height);
        break;
    case PROP_WIDTH:
        g_value_set_int (value,toplevel->priv->width);
        break;
    case PROP_SIZE_TYPE:
        g_value_set_enum(value,toplevel->priv->widthtype);
        break;
    case PROP_MONITOR:
        g_value_set_int(value,toplevel->priv->monitor);
        break;
    case PROP_ALIGNMENT:
        g_value_set_enum(value, toplevel->priv->allign);
        break;
    case PROP_MARGIN:
        g_value_set_int(value,toplevel->priv->margin);
        break;
    case PROP_BACKGROUNDTYPE:
        g_value_set_enum(value,toplevel->priv->background);
        break;
    case PROP_AUTOHIDE:
        g_value_set_boolean(value, toplevel->priv->autohide);
        break;
    case PROP_AUTOHIDE_SIZE:
        g_value_set_int(value,toplevel->priv->height_when_hidden);
        break;
    case PROP_ENABLEFONTCOLOR:
        g_value_set_boolean (value,toplevel->priv->usefontcolor);
        break;
    case PROP_ENABLEFONTSIZE:
        g_value_set_boolean (value,toplevel->priv->usefontsize);
        break;
    case PROP_BACKGROUNDFILE:
        g_value_set_string(value,toplevel->priv->background_file);
        break;
    case PROP_TINTCOLOR:
        g_value_set_string(value,gdk_rgba_to_string(&toplevel->priv->gtintcolor));
        break;
    case PROP_FONTCOLOR:
        g_value_set_string(value,gdk_rgba_to_string(&toplevel->priv->gfontcolor));
        break;
    case PROP_FONTSIZE:
        g_value_set_int(value,toplevel->priv->fontsize);
        break;
    case PROP_ICON_SIZE:
        g_value_set_int(value,toplevel->priv->icon_size);
        break;
    case PROP_DOCK:
        g_value_set_boolean(value,toplevel->priv->setdocktype);
        break;
    case PROP_STRUT:
        g_value_set_boolean(value,toplevel->priv->setstrut);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void lxpanel_class_init(PanelWindowClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

    gobject_class->set_property = simple_panel_set_property;
    gobject_class->get_property = simple_panel_get_property;

    gobject_class->finalize = lxpanel_finalize;
	widget_class->destroy = lxpanel_destroy;
    widget_class->realize = lxpanel_realize;
	widget_class->get_preferred_width = lxpanel_get_preferred_width;
	widget_class->get_preferred_height = lxpanel_get_preferred_height;
    widget_class->size_allocate = lxpanel_size_allocate;
    widget_class->configure_event = lxpanel_configure_event;
    widget_class->map_event = lxpanel_map_event;
    widget_class->button_press_event = lxpanel_button_press;

    g_object_class_install_property (
                gobject_class,
                PROP_NAME,
                g_param_spec_string (
                    "name",
                    "Name",
                    "The name of this panel",
                    NULL,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
                gobject_class,
                PROP_EDGE,
                g_param_spec_enum(
                    "edge",
                    "Edge",
                    "Edge of the screen where panel attached",
                    gtk_position_type_get_type(),
                    GTK_POS_TOP,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
                gobject_class,
                PROP_ORIENTATION,
                g_param_spec_enum(
                    "orientation",
                    "Orientation",
                    "Orientation of the panel",
                    gtk_orientation_get_type(),
                    GTK_ORIENTATION_HORIZONTAL,
                    G_PARAM_READABLE));
    g_object_class_install_property(
                gobject_class,
                PROP_HEIGHT,
                g_param_spec_int(
                    "height",
                    "Height",
                    "Height of this panel",
                    PANEL_HEIGHT_MIN,
                    PANEL_HEIGHT_MAX,
                    PANEL_HEIGHT_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
                gobject_class,
                PROP_WIDTH,
                g_param_spec_int(
                    "width",
                    "Width",
                    "Width of this panel (in given size type)",
                    0,
                    G_MAXINT,
                    100,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
                gobject_class,
                PROP_SIZE_TYPE,
                g_param_spec_enum(
                    "size-type",
                    "Size Type",
                    "Type of panel size counting",
                    PANEL_SIZE_TYPE,
                    PANEL_SIZE_PERCENT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
                gobject_class,
                PROP_ALIGNMENT,
                g_param_spec_enum(
                    "alignment",
                    "Alignment",
                    "Panel alignment side",
                    PANEL_ALLIGN_TYPE,
                    PANEL_ALLIGN_NONE,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_MONITOR,
                g_param_spec_int (
                    "monitor",
                    "Xinerama monitor",
                    "The monitor (in terms of Xinerama) which the panel is on",
                    0,
                    G_MAXINT,
                    0,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_AUTOHIDE,
                g_param_spec_boolean (
                    "auto-hide",
                    "Auto hide",
                    "Automatically hide the panel when the mouse leaves the panel",
                    FALSE,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_AUTOHIDE_SIZE,
                g_param_spec_int (
                    "auto-hide-size",
                    "Auto-hide size",
                    "The number of pixels visible when the panel has been automatically hidden",
                    0,
                    10,
                    PANEL_AUTOHIDE_SIZE,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
                gobject_class,
                PROP_BACKGROUNDTYPE,
                g_param_spec_enum(
                    "background-type",
                    "Background Type",
                    "Type of panel background",
                    PANEL_BACKGROUND_TYPE,
                    PANEL_BACKGROUND_GTK,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_ENABLEFONTCOLOR,
                g_param_spec_boolean(
                    "enable-font-color",
                    "Enable font color",
                    "Enable custom font color",
                    FALSE,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_ENABLEFONTSIZE,
                g_param_spec_boolean(
                    "enable-font-size",
                    "Enable font size",
                    "Enable custom font size",
                    FALSE,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
                gobject_class,
                PROP_ICON_SIZE,
                g_param_spec_int(
                    "icon-size",
                    "Icon size",
                    "Size of panel icons",
                    PANEL_HEIGHT_MIN,
                    PANEL_HEIGHT_MAX,
                    PANEL_HEIGHT_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_BACKGROUNDFILE,
                g_param_spec_string (
                    "background-file",
                    "Background file",
                    "Background file of this panel",
                    NULL,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_TINTCOLOR,
                g_param_spec_string (
                    "background-color",
                    "Background color",
                    "Background color of this panel",
                    "white",
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_FONTCOLOR,
                g_param_spec_string (
                    "font-color",
                    "Font color",
                    "Font color color of this panel",
                    "black",
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
                gobject_class,
                PROP_FONTSIZE,
                g_param_spec_int(
                    "font-size",
                    "Font size",
                    "Size of panel fonts",
                    PANEL_FONT_MIN,
                    PANEL_FONT_MAX,
                    PANEL_FONT_DEFAULT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_STRUT,
                g_param_spec_boolean (
                    "strut",
                    "Set strut",
                    "Set strut to crop it from maximized windows",
                    TRUE,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_DOCK,
                g_param_spec_boolean (
                    "dock",
                    "Dock",
                    "Make window managers treat panel as dock",
                    TRUE,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

}

static void lxpanel_init(PanelWindow *self)
{
    Panel *p = g_new0(Panel, 1);
    self->priv = p;
    p->topgwin = self;
    p->icon_theme = gtk_icon_theme_get_default();
    p->config = config_new();
	GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(self));
	GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
	gtk_widget_set_visual(GTK_WIDGET(self), visual);
}


/* Allocate and initialize new Panel structure. */
static SimplePanel* panel_allocate(GtkApplication* app)
{
    return g_object_new(LX_TYPE_PANEL, NULL);
}

/* Normalize panel configuration after load from file or reconfiguration. */
static void panel_normalize_configuration(Panel* p)
{
    panel_set_panel_configuration_changed( p );
    if (p->width < 0)
        p->width = 100;
    if (p->widthtype == PANEL_SIZE_PERCENT && p->width > 100)
        p->width = 100;
    p->heighttype = PANEL_SIZE_PIXEL;
    if (p->heighttype == PANEL_SIZE_PIXEL) {
        if (p->height < PANEL_HEIGHT_MIN)
            p->height = PANEL_HEIGHT_MIN;
        else if (p->height > PANEL_HEIGHT_MAX)
            p->height = PANEL_HEIGHT_MAX;
    }
    if (p->monitor < 0)
        p->monitor = 0;
}

/****************************************************
 *         panel's handlers for WM events           *
 ****************************************************/

void _panel_set_wm_strut(SimplePanel *panel)
{
    int index;
    GdkAtom atom;
    GdkWindow* xwin = gtk_widget_get_window(GTK_WIDGET(panel));
    Panel *p = panel->priv;
    gulong strut_size;
    gulong strut_lower;
    gulong strut_upper;

    if (!gtk_widget_get_mapped(GTK_WIDGET(panel)))
        return;
    /* most wm's tend to ignore struts of unmapped windows, and that's how
     * lxpanel hides itself. so no reason to set it. */
    if (p->autohide && p->height_when_hidden <= 0)
        return;

    /* Dispatch on edge to set up strut parameters. */
    switch (p->edge)
    {
        case GTK_POS_LEFT:
            index = 0;
            strut_size = p->aw;
            strut_lower = p->ay;
            strut_upper = p->ay + p->ah;
            break;
        case GTK_POS_RIGHT:
            index = 1;
            strut_size = p->aw;
            strut_lower = p->ay;
            strut_upper = p->ay + p->ah;
            break;
        case GTK_POS_TOP:
            index = 2;
            strut_size = p->ah;
            strut_lower = p->ax;
            strut_upper = p->ax + p->aw;
            break;
        case GTK_POS_BOTTOM:
            index = 3;
            strut_size = p->ah;
            strut_lower = p->ax;
            strut_upper = p->ax + p->aw;
            break;
        default:
            return;
    }

    /* Handle autohide case.  EWMH recommends having the strut be the minimized size. */
    if (p->autohide)
        strut_size = p->height_when_hidden;

    /* Set up strut value in property format. */
    gulong desired_strut[12];
    memset(desired_strut, 0, sizeof(desired_strut));
    if (p->setstrut)
    {
        desired_strut[index] = strut_size;
        desired_strut[4 + index * 2] = strut_lower;
        desired_strut[5 + index * 2] = strut_upper;
    }
    else
    {
        strut_size = 0;
        strut_lower = 0;
        strut_upper = 0;
    }

    /* If strut value changed, set the property value on the panel window.
     * This avoids property change traffic when the panel layout is recalculated but strut geometry hasn't changed. */
    if ((p->strut_size != strut_size) || (p->strut_lower != strut_lower) || (p->strut_upper != strut_upper) || (p->strut_edge != p->edge))
    {
        p->strut_size = strut_size;
        p->strut_lower = strut_lower;
        p->strut_upper = strut_upper;
        p->strut_edge = p->edge;

        /* If window manager supports STRUT_PARTIAL, it will ignore STRUT.
         * Set STRUT also for window managers that do not support STRUT_PARTIAL. */
        if (strut_size != 0)
        {
            atom = gdk_atom_intern_static_string("_NET_WM_STRUT_PARTIAL");
            gdk_property_change(xwin,atom,gdk_atom_intern_static_string("CARDINAL"),32,GDK_PROP_MODE_REPLACE,(guchar*)desired_strut,12);
            atom = gdk_atom_intern_static_string("_NET_WM_STRUT");
            gdk_property_change(xwin,atom,gdk_atom_intern_static_string("CARDINAL"),32,GDK_PROP_MODE_REPLACE,(guchar*)desired_strut,4);
        }
        else
        {
            atom = gdk_atom_intern_static_string("_NET_WM_STRUT_PARTIAL");
            gdk_property_delete(xwin,atom);
            atom = gdk_atom_intern_static_string("_NET_WM_STRUT");
            gdk_property_delete(xwin,atom);
        }
    }
}

/****************************************************
 *         panel's handlers for GTK events          *
 ****************************************************/
void _panel_determine_background_css(SimplePanel * panel, GtkWidget * widget);

void panel_determine_background_pixmap(Panel * panel, GtkWidget * widget, GdkWindow * window)
{
    _panel_determine_background_css(panel->topgwin, widget);
}

void _panel_determine_background_css(SimplePanel * panel, GtkWidget * widget)
{
	Panel * p = panel->priv;
    gchar* css = NULL;
    GdkRGBA color;
    gboolean system = FALSE;
    gdk_rgba_parse(&color,"transparent");
    if (p->background == PANEL_BACKGROUND_CUSTOM_IMAGE)
	{
		/* User specified background pixmap. */
        if (p->background_file != NULL)
        {
            system = FALSE;
            css = css_generate_background(p->background_file,color,FALSE);
        }
	}
    else if (p->background == PANEL_BACKGROUND_CUSTOM_COLOR)
	{
		/* User specified background color. */
        system = FALSE;
        css = css_generate_background("none",p->gtintcolor,TRUE);
    }
    else
    {
        system = TRUE;
    }

    if (css)
    {
        css_apply_with_class(widget,css,"-simple-panel-background",system);
        g_free(css);
    }
    else if (system)
    {
        css_apply_with_class(widget,"","-simple-panel-background",system);
    }
    if (p->background == PANEL_BACKGROUND_GNOME)
    {
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(p->topgwin)),"gnome-panel-menu-bar");
    }
    else
    {
        gtk_style_context_remove_class(gtk_widget_get_style_context(GTK_WIDGET(p->topgwin)),"gnome-panel-menu-bar");
    }
}

/* Update the background of the entire panel.
 * This function should only be called after the panel has been realized. */
void panel_update_background(Panel * p)
{
    _panel_update_background(p->topgwin);
}

static void _panel_update_background(SimplePanel * p)
{
    GtkWidget *w = GTK_WIDGET(p);

    _panel_determine_background_css(p, w);
}

void panel_update_fonts(Panel * p)
{
    _panel_update_fonts(p->topgwin);
}

void _panel_update_fonts(SimplePanel * p)
{
    gchar* css;
    if (p->priv->usefontcolor){
        css = css_generate_font_color(p->priv->gfontcolor);
        css_apply_with_class(GTK_WIDGET(p),css,"-simple-panel-font-color",FALSE);
        g_free(css);
    } else {
        css_apply_with_class(GTK_WIDGET(p),css,"-simple-panel-font-color",TRUE);
    }
    if (p->priv->usefontsize){
        css = css_generate_font_size(p->priv->fontsize);
        css_apply_with_class(GTK_WIDGET(p),css,"-simple-panel-font-size",FALSE);
        g_free(css);
    } else {
        css_apply_with_class(GTK_WIDGET(p),css,"-simple-panel-font-size",TRUE);
    }
}

/****************************************************
 *         autohide : borrowed from fbpanel         *
 ****************************************************/

/* Autohide is behaviour when panel hides itself when mouse is "far enough"
 * and pops up again when mouse comes "close enough".
 * Formally, it's a state machine with 3 states that driven by mouse
 * coordinates and timer:
 * 1. VISIBLE - ensures that panel is visible. When/if mouse goes "far enough"
 *      switches to WAITING state
 * 2. WAITING - starts timer. If mouse comes "close enough", stops timer and
 *      switches to VISIBLE.  If timer expires, switches to HIDDEN
 * 3. HIDDEN - hides panel. When mouse comes "close enough" switches to VISIBLE
 *
 * Note 1
 * Mouse coordinates are queried every PERIOD milisec
 *
 * Note 2
 * If mouse is less then GAP pixels to panel it's considered to be close,
 * otherwise it's far
 */

#define GAP 2
#define PERIOD 300

typedef enum
{
    AH_STATE_VISIBLE,
    AH_STATE_WAITING,
    AH_STATE_HIDDEN
} PanelAHState;

static void ah_state_set(SimplePanel *p, PanelAHState ah_state);

static gboolean
mouse_watch(SimplePanel *panel)
{
    Panel *p = panel->priv;
    gint x, y;

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    ENTER;
    GdkDeviceManager *device_manager = gdk_display_get_device_manager (gdk_display_get_default());
    GdkDevice *device = gdk_device_manager_get_client_pointer (device_manager);
    gdk_device_get_position (device,NULL, &x, &y);

/*  Reduce sensitivity area
    p->ah_far = ((x < p->cx - GAP) || (x > p->cx + p->cw + GAP)
        || (y < p->cy - GAP) || (y > p->cy + p->ch + GAP));
*/

    gint cx, cy, cw, ch, gap;

    cx = p->ax;
    cy = p->ay;
    cw = p->cw;
    ch = p->ch;

    if (cw == 1) cw = 0;
    if (ch == 1) ch = 0;
    /* reduce area which will raise panel so it does not interfere with apps */
    if (p->ah_state == AH_STATE_HIDDEN) {
        gap = MAX(p->height_when_hidden, GAP);
        switch (p->edge) {
        case GTK_POS_LEFT:
            cw = gap;
            break;
        case GTK_POS_RIGHT:
            cx = cx + cw - gap;
            cw = gap;
            break;
        case GTK_POS_TOP:
            ch = gap;
            break;
        case GTK_POS_BOTTOM:
            cy = cy + ch - gap;
            ch = gap;
            break;
       }
    }
    p->ah_far = ((x < cx) || (x > cx + cw) || (y < cy) || (y > cy + ch));

    ah_state_set(panel, p->ah_state);
    RET(TRUE);
}

static gboolean ah_state_hide_timeout(gpointer p)
{
    if (!g_source_is_destroyed(g_main_current_source()))
    {
        ah_state_set(p, AH_STATE_HIDDEN);
        ((SimplePanel *)p)->priv->hide_timeout = 0;
    }
    return FALSE;
}

static void ah_state_set(SimplePanel *panel, PanelAHState ah_state)
{
    Panel *p = panel->priv;

    ENTER;
    if (p->ah_state != ah_state) {
        p->ah_state = ah_state;
        switch (ah_state) {
        case AH_STATE_VISIBLE:
            gtk_widget_show(GTK_WIDGET(panel));
            gtk_widget_show(p->box);
            gtk_widget_queue_resize(GTK_WIDGET(panel));
            gtk_window_stick(GTK_WINDOW(panel));
            p->visible = TRUE;
            break;
        case AH_STATE_WAITING:
            if (p->hide_timeout)
                g_source_remove(p->hide_timeout);
            p->hide_timeout = g_timeout_add(2 * PERIOD, ah_state_hide_timeout, panel);
            break;
        case AH_STATE_HIDDEN:
            if (p->height_when_hidden > 0)
            {
                gtk_widget_hide(p->box);
                gtk_widget_queue_resize(GTK_WIDGET(panel));
            }
            else
                gtk_widget_hide(GTK_WIDGET(panel));
            p->visible = FALSE;
        }
    } else if (p->autohide && p->ah_far) {
        switch (ah_state) {
        case AH_STATE_VISIBLE:
            ah_state_set(panel, AH_STATE_WAITING);
            break;
        case AH_STATE_WAITING:
            break;
        case AH_STATE_HIDDEN:
            /* configurator might change height_when_hidden value */
            if (p->height_when_hidden > 0)
            {
                if (gtk_widget_get_visible(p->box))
                {
                    gtk_widget_hide(p->box);
                    gtk_widget_show(GTK_WIDGET(panel));
                }
            }
            else
                if (gtk_widget_get_visible(GTK_WIDGET(panel)))
                {
                    gtk_widget_hide(GTK_WIDGET(panel));
                    gtk_widget_show(p->box);
                }
        }
    } else {
        switch (ah_state) {
        case AH_STATE_VISIBLE:
            break;
        case AH_STATE_WAITING:
            if (p->hide_timeout)
                g_source_remove(p->hide_timeout);
            p->hide_timeout = 0;
            /* continue with setting visible */
        case AH_STATE_HIDDEN:
            ah_state_set(panel, AH_STATE_VISIBLE);
        }
    }
    RET();
}

/* starts autohide behaviour */
static void ah_start(SimplePanel *p)
{
    ENTER;
    if (!p->priv->mouse_timeout)
        p->priv->mouse_timeout = g_timeout_add(PERIOD, (GSourceFunc) mouse_watch, p);
    RET();
}

/* stops autohide */
static void ah_stop(SimplePanel *p)
{
    ENTER;
    if (p->priv->mouse_timeout) {
        g_source_remove(p->priv->mouse_timeout);
        p->priv->mouse_timeout = 0;
    }
    if (p->priv->hide_timeout) {
        g_source_remove(p->priv->hide_timeout);
        p->priv->hide_timeout = 0;
    }
    RET();
}
/* end of the autohide code
 * ------------------------------------------------------------- */

void activate_panel_settings(GSimpleAction *action, GVariant *param, gpointer data)
{
    int page = g_variant_get_int32(param);
    g_variant_unref(param);
    SimplePanel* p = (SimplePanel*)data;
    panel_configure(p,page);

}

static void activate_config_plugin(GSimpleAction *action, GVariant *param, gpointer pl)
{
    GtkWidget* plugin = (GtkWidget*) pl;
    Panel *panel = PLUGIN_PANEL(plugin)->priv;

    lxpanel_plugin_show_config_dialog(plugin);

    /* FIXME: this should be more elegant */
    panel->config_changed = TRUE;
}

static void activate_remove_plugin(GSimpleAction *action, GVariant *param, gpointer pl)
{
    GtkWidget* plugin = (GtkWidget*) pl;
    Panel* panel = PLUGIN_PANEL(plugin)->priv;

    /* If the configuration dialog is open, there will certainly be a crash if the
     * user manipulates the Configured Plugins list, after we remove this entry.
     * Close the configuration dialog if it is open. */
    if (panel->pref_dialog != NULL)
    {
        gtk_widget_destroy(panel->pref_dialog);
        panel->pref_dialog = NULL;
    }
    config_setting_destroy(g_object_get_qdata(G_OBJECT(plugin), lxpanel_plugin_qconf));
    /* reset conf pointer because the widget still may be referenced by configurator */
    g_object_set_qdata(G_OBJECT(plugin), lxpanel_plugin_qconf, NULL);

    simple_panel_config_save(PLUGIN_PANEL(plugin));
    gtk_widget_destroy(plugin);
}

static gboolean _panel_set_monitor(SimplePanel* panel, int monitor)
{
    int monitors = 1;
    GdkScreen* screen = gdk_screen_get_default();
    if(screen)
        monitors = gdk_screen_get_n_monitors(screen);
    g_assert(monitors >= 1);
    if (monitors>=1)
        panel->priv->monitor=monitor;
    return FALSE;
}
/* FIXME: Potentially we can support multiple panels at the same edge,
 * but currently this cannot be done due to some positioning problems. */
static char* gen_panel_name( int edge, gint monitor)
{
    const char* edge_str = num2str( edge_pair, edge, "" );
    char* name = NULL;
    char* dir = _user_config_file_name("panels", NULL);
    int i;
    for( i = 0; i < G_MAXINT; ++i )
    {
        char* f;
        if(monitor != 0)
            name = g_strdup_printf( "%s-m%d-%d", edge_str, monitor, i );
        else if( G_LIKELY( i > 0 ) )
            name =  g_strdup_printf( "%s%d", edge_str, i );
        else
            name = g_strdup( edge_str );

        f = g_build_filename( dir, name, NULL );
        if( ! g_file_test( f, G_FILE_TEST_EXISTS ) )
        {
            g_free( f );
            break;
        }
        g_free( name );
        g_free( f );
    }
    g_free( dir );
    return name;
}

/* FIXME: Potentially we can support multiple panels at the same edge,
 * but currently this cannot be done due to some positioning problems. */
static void activate_new_panel(GSimpleAction *action, GVariant *param, gpointer data)
{
    SimplePanel* panel = (SimplePanel*) data;
    gint m, e, monitors;
    GdkScreen *screen;
    SimplePanel *new_panel = panel_allocate(panel->priv->app);
    Panel *p = new_panel->priv;
    config_setting_t *global;
    p->app = panel->priv->app;
    g_object_get(G_OBJECT(new_panel->priv->app),"profile",&cprofile,NULL);

    /* Allocate the edge. */
    screen = gdk_screen_get_default();
    g_assert(screen);
    monitors = gdk_screen_get_n_monitors(screen);
    for(m=0; m<monitors; ++m)
    {
        /* try each of the four edges */
        for(e=1; e<5; ++e)
        {
            if(panel_edge_available(p,e,m)) {
                p->edge = e;
                p->monitor = m;
                goto found_edge;
            }
        }
    }

    gtk_widget_destroy(GTK_WIDGET(new_panel));
    g_warning("Error adding panel: There is no room for another panel. All the edges are taken.");
    fm_show_error(NULL, NULL, _("There is no room for another panel. All the edges are taken."));
    return;

found_edge:
    p->name = gen_panel_name(p->edge, p->monitor);

    /* create new config with first group "Global" */
    global = config_group_add_subgroup(config_root_setting(p->config), "Global");
    config_group_set_string(global, "edge", num2str(edge_pair, p->edge, "none"));
    config_group_set_int(global, "monitor", p->monitor);
    panel_add_actions(new_panel);
    panel_configure(new_panel, 0);
    panel_normalize_configuration(p);
    panel_start_gui(new_panel);
    gtk_widget_show_all(GTK_WIDGET(new_panel));

    simple_panel_config_save(new_panel);
    gtk_application_add_window(GTK_APPLICATION(p->app),GTK_WINDOW(new_panel));
    all_panels = gtk_application_get_windows(GTK_APPLICATION(p->app));
}

static void activate_remove_panel(GSimpleAction *action, GVariant *param, gpointer data)
{
    SimplePanel* panel = (SimplePanel*)data;
    GtkWidget* dlg;
    gboolean ok;
    dlg = gtk_message_dialog_new_with_markup( GTK_WINDOW(panel),
                                                    GTK_DIALOG_MODAL,
                                                    GTK_MESSAGE_QUESTION,
                                                    GTK_BUTTONS_OK_CANCEL,
                                                    _("Really delete this panel?\n<b>Warning: This can not be recovered.</b>") );
    panel_apply_icon(GTK_WINDOW(dlg));
    gtk_window_set_title( (GtkWindow*)dlg, _("Confirm") );
    ok = ( gtk_dialog_run( (GtkDialog*)dlg ) == GTK_RESPONSE_OK );
    gtk_widget_destroy( dlg );
    if( ok )
    {
        gchar *fname;

        /* delete the config file of this panel */
        fname = _user_config_file_name("panels", panel->priv->name);
        g_unlink( fname );
        g_free(fname);
        panel->priv->config_changed = 0;
        gtk_application_remove_window(GTK_APPLICATION(panel->priv->app),GTK_WINDOW(panel));
        gtk_widget_destroy(GTK_WIDGET(panel));
    }
}

void panel_apply_icon( GtkWindow *w )
{
    GdkPixbuf* window_icon;

    if(gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), "video-display"))
    {
        window_icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "video-display", 24, 0, NULL);
    }
    else if(gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), "start-here"))
    {
        window_icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "start-here", 24, 0, NULL);
    }
    else
    {
        window_icon = gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/images/my-computer.png", NULL);
    }
    gtk_window_set_icon(w, window_icon);
}

GtkMenu* lxpanel_get_plugin_menu(SimplePanel* panel, GtkWidget* plugin)
{
    GMenu *gmenu, *gmenusection;
    GtkMenu *ret;
    const SimplePanelPluginInit *init;
    char* tmp;
    gmenu = g_menu_new();
    if (plugin)
    {
        init = PLUGIN_CLASS(plugin);
        /* create single item - plugin instance settings */
        g_action_map_remove_action(G_ACTION_MAP(panel),"remove-plugin");
        g_action_map_remove_action(G_ACTION_MAP(panel),"config-plugin");
        const GActionEntry plugin_entries[] = {
            {"remove-plugin",activate_remove_plugin,NULL,NULL,NULL},
            {"config-plugin",activate_config_plugin,NULL,NULL,NULL},
        };
        g_action_map_add_action_entries(G_ACTION_MAP(panel),plugin_entries,
                                        G_N_ELEMENTS(plugin_entries),
                                        (gpointer)plugin);
        gmenusection = g_menu_new();
        tmp = g_strdup_printf( _("\"%s\" Settings"), _(init->name) );
        g_menu_append(gmenusection,tmp,"win.config-plugin");
        g_free( tmp );
        if(! init->config )
            g_action_map_remove_action(G_ACTION_MAP(panel),"config-plugin");
        tmp = g_strdup_printf( _("Remove \"%s\" From Panel"), _(init->name) );
        g_menu_append(gmenusection,tmp,"win.remove-plugin");
        g_free( tmp );
        /* add custom items by plugin if requested */
        if (init->update_context_menu != NULL)
        {
            tmp = g_strdup_printf("%s", _(init->name) );
            g_menu_append_submenu(gmenusection,tmp,G_MENU_MODEL(init->update_context_menu(plugin)));
            g_free( tmp );
        }
        g_menu_append_section(gmenu,NULL,G_MENU_MODEL(gmenusection));
        g_object_unref(gmenusection);
    }
    gmenusection = g_menu_new();
    g_menu_append(gmenusection,_("Add / Remove Panel Items"),"win.panel-settings(1)");
    g_menu_append_section(gmenu,NULL,G_MENU_MODEL(gmenusection));
    g_object_unref(gmenusection);
    gmenusection = g_menu_new();
    g_menu_append(gmenusection,_("Panel Settings"),"win.panel-settings(0)");
    g_menu_append(gmenusection,_("Create New Panel"),"win.new-panel");
    gboolean enabled = g_list_length(gtk_application_get_windows(panel->priv->app))>1;
    g_simple_action_set_enabled (
                G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(panel),"remove-panel")),
                enabled
                );
    g_menu_append(gmenusection,_("Delete This Panel"),"win.remove-panel");
    g_menu_append_section(gmenu,NULL,G_MENU_MODEL(gmenusection));
    g_object_unref(gmenusection);
    gmenusection = g_menu_new();
    g_menu_append(gmenusection,_("About"),"app.about");
    g_menu_append_section(gmenu,NULL,G_MENU_MODEL(gmenusection));
    g_object_unref(gmenusection);

    ret = GTK_MENU(gtk_menu_new_from_model(G_MENU_MODEL(gmenu)));
    if (plugin)
        gtk_menu_attach_to_widget(ret,GTK_WIDGET(plugin),NULL);
    else
        gtk_menu_attach_to_widget(ret,GTK_WIDGET(panel),NULL);
    gtk_widget_show_all(GTK_WIDGET(ret));
    g_object_unref(gmenu);
    return ret;
}

/****************************************************
 *         panel creation                           *
 ****************************************************/

void update_panel_geometry( SimplePanel* p )
{
    /* Guard against being called early in panel creation. */
    _calculate_position(p);
    gtk_widget_hide(GTK_WIDGET(p));
    gtk_widget_set_size_request(GTK_WIDGET(p), p->priv->aw, p->priv->ah);
    gtk_window_move(GTK_WINDOW(p),p->priv->ax,p->priv->ay);
    gtk_widget_show(GTK_WIDGET(p));
    _panel_queue_update_background(p);
    _panel_establish_autohide(p);
    _panel_set_wm_strut(p);
}

static void
make_round_corners(Panel *p)
{
    /* FIXME: This should be re-written with shape extension of X11 */
    /* gdk_window_shape_combine_mask() can be used */
}

void panel_set_dock_type(SimplePanel *p)
{
    if (p->priv->setdocktype) {
        gtk_window_set_type_hint(GTK_WINDOW(p),GDK_WINDOW_TYPE_HINT_DOCK);
    }
    else {
        gtk_window_set_type_hint(GTK_WINDOW(p),GDK_WINDOW_TYPE_HINT_NORMAL);
    }
}

void panel_establish_autohide(Panel *p)
{
    _panel_establish_autohide(p->topgwin);
}

void _panel_establish_autohide(SimplePanel *p)
{
    if (p->priv->autohide)
        ah_start(p);
    else
    {
        ah_stop(p);
        ah_state_set(p, AH_STATE_VISIBLE);
    }
}

/* Set an image from a file with scaling to the panel icon size. */
void panel_image_set_from_file(Panel * p, GtkWidget * image, const char * file)
{
    GdkPixbuf * pixbuf = gdk_pixbuf_new_from_file_at_scale(file, p->icon_size, p->icon_size, TRUE, NULL);
    if (pixbuf != NULL)
    {
        gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
        g_object_unref(pixbuf);
    }
}

void lxpanel_image_set_from_file(SimplePanel * p, GtkWidget * image, const char * file)
{
    panel_image_set_from_file(p->priv, image, file);
}

/* Set an image from a icon theme with scaling to the panel icon size. */
gboolean panel_image_set_icon_theme(Panel * p, GtkWidget * image, const gchar * icon)
{
    if (gtk_icon_theme_has_icon(p->icon_theme, icon))
    {
        GdkPixbuf * pixbuf = gtk_icon_theme_load_icon(p->icon_theme, icon, p->icon_size, 0, NULL);
        gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
        g_object_unref(pixbuf);
        return TRUE;
    }
    return FALSE;
}

gboolean lxpanel_image_set_icon_theme(SimplePanel * p, GtkWidget * image, const gchar * icon)
{
    return panel_image_set_icon_theme(p->priv, image, icon);
}

static void
panel_start_gui(SimplePanel *panel)
{
    gulong val;
    Panel *p = panel->priv;
    GtkWidget *w = GTK_WIDGET(panel);

    ENTER;

    /* main toplevel window */
    /* p->topgwin =  gtk_window_new(GTK_WINDOW_TOPLEVEL); */
    p->display = gdk_display_get_default();
    gtk_container_set_border_width(GTK_CONTAINER(panel), 0);
    gtk_window_set_resizable(GTK_WINDOW(panel), FALSE);
    gtk_window_set_wmclass(GTK_WINDOW(panel), "panel", "lxpanel");
    gtk_window_set_title(GTK_WINDOW(panel), "panel");
    gtk_window_set_position(GTK_WINDOW(panel), GTK_WIN_POS_NONE);
    gtk_window_set_decorated(GTK_WINDOW(panel), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(panel),TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(panel),TRUE);
    gtk_window_stick(GTK_WINDOW(panel));
    gtk_window_set_accept_focus(GTK_WINDOW(panel),FALSE);

    gtk_application_add_window(GTK_APPLICATION(win_grp), (GtkWindow*)panel );

    gtk_widget_add_events( w, GDK_BUTTON_PRESS_MASK );

    gtk_widget_realize(w);
    //gdk_window_set_decorations(gtk_widget_get_window(p->topgwin), 0);

    // main layout manager as a single child of panel
    p->box = panel_box_new(panel, 0);
    gtk_container_set_border_width(GTK_CONTAINER(p->box), 0);
    gtk_container_add(GTK_CONTAINER(panel), p->box);
    gtk_widget_show(p->box);
    if (p->round_corners)
        make_round_corners(p);

    panel_set_dock_type(panel);

    /* window mapping point */
    gtk_widget_show_all(w);
    /* the settings that should be done after window is mapped */
    _panel_establish_autohide(panel);

    val = G_MAXULONG;
    gdk_property_change(gtk_widget_get_window(w),
                        gdk_atom_intern_static_string("_NET_WM_DESKTOP"),
                        gdk_atom_intern_static_string("CARDINAL"),
                        32, GDK_PROP_MODE_REPLACE, (guchar*)&val, 1);
    _calculate_position(panel);
    gdk_window_move_resize(gtk_widget_get_window(w), p->ax, p->ay, p->aw, p->ah);
    _panel_set_wm_strut(panel);
    p->initialized = TRUE;

    RET();
}

/* Exchange the "width" and "height" terminology for vertical and horizontal panels. */
void panel_adjust_geometry_terminology(Panel * p)
{
    if ((p->alignment_left_label != NULL) && (p->alignment_right_label != NULL))
    {
        if ((p->edge == GTK_POS_TOP) || (p->edge == GTK_POS_BOTTOM))
        {
            gtk_button_set_label(GTK_BUTTON(p->alignment_left_label), _("Left"));
            gtk_button_set_label(GTK_BUTTON(p->alignment_right_label), _("Right"));
        }
        else
        {
            gtk_button_set_label(GTK_BUTTON(p->alignment_left_label), _("Top"));
            gtk_button_set_label(GTK_BUTTON(p->alignment_right_label), _("Bottom"));
        }
    }
}

/* Draw text into a label, with the user preference color and optionally bold. */
void panel_draw_label_text(Panel * p, GtkWidget * label, const char * text,
                           gboolean bold, float custom_size_factor,
                           gboolean custom_color)
{
    gtk_label_set_text(GTK_LABEL(label),text);
    gchar* css = css_generate_font_weight(bold);
    css_apply_with_class(label,css,"-simple-panel-font-weight",FALSE);
    g_free(css);
}


void lxpanel_draw_label_text(SimplePanel * p, GtkWidget * label, const char * text,
                           gboolean bold, float custom_size_factor,
                           gboolean custom_color)
{
    panel_draw_label_text(p->priv, label, text, bold, custom_size_factor, custom_color);
}

void panel_set_panel_configuration_changed(Panel *p)
{
    _panel_set_panel_configuration_changed(p->topgwin);
}

void _panel_set_panel_configuration_changed(SimplePanel *panel)
{
    Panel *p = panel->priv;
    GList *plugins, *l;

    GtkOrientation previous_orientation = p->orientation;
    p->orientation = (p->edge == GTK_POS_TOP || p->edge == GTK_POS_BOTTOM)
        ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;

    /* either first run or orientation was changed */
    if (!p->initialized || previous_orientation != p->orientation)
    {
        if (p->initialized)
            p->height = ((p->orientation == GTK_ORIENTATION_HORIZONTAL) ? PANEL_HEIGHT_DEFAULT : PANEL_WIDTH_DEFAULT);
        if (p->height_control != NULL)
            simple_panel_scale_button_set_value_labeled(GTK_SCALE_BUTTON(p->height_control), p->height);
        if ((p->widthtype == PANEL_SIZE_PIXEL) && (p->width_control != NULL))
        {
            int value = ((p->orientation == GTK_ORIENTATION_HORIZONTAL) ? gdk_screen_width() : gdk_screen_height());
            simple_panel_scale_button_set_range(GTK_SCALE_BUTTON(p->width_control), 0, value);
            simple_panel_scale_button_set_value_labeled(GTK_SCALE_BUTTON(p->width_control), value);
        }
    }

    /* recreate the main layout box */
    if (p->box != NULL)
    {
        gtk_orientable_set_orientation(GTK_ORIENTABLE(p->box), p->orientation);
    }

    /* NOTE: This loop won't be executed when panel started since
       plugins are not loaded at that time.
       This is used when the orientation of the panel is changed
       from the config dialog, and plugins should be re-layout.
    */
    plugins = p->box ? gtk_container_get_children(GTK_CONTAINER(p->box)) : NULL;
    for( l = plugins; l; l = l->next ) {
        GtkWidget *w = (GtkWidget*)l->data;
        const SimplePanelPluginInit *init = PLUGIN_CLASS(w);
        if (init->reconfigure)
            init->reconfigure(panel, w);
    }
    g_list_free(plugins);
    /* panel geometry changed? update panel background then */
    _panel_queue_update_background(panel);
}

static int
panel_parse_global(Panel *p, config_setting_t *cfg)
{
    const char *str;
    gint i;

    /* check Global config */
    if (!cfg || strcmp(config_setting_get_name(cfg), "Global") != 0)
    {
        g_warning( "lxpanel: Global section not found");
        RET(0);
    }
    if (config_setting_lookup_string(cfg, "edge", &str))
        p->edge = str2num(edge_pair, str, GTK_POS_BOTTOM);
    if (config_setting_lookup_string(cfg, "allign", &str))
        p->allign = str2num(allign_pair, str, PANEL_ALLIGN_NONE);
    config_setting_lookup_int(cfg, "monitor", &p->monitor);
    config_setting_lookup_int(cfg, "margin", &p->margin);
    if (config_setting_lookup_string(cfg, "widthtype", &str))
        p->widthtype = str2num(strut_pair, str, PANEL_SIZE_FILL);
    config_setting_lookup_int(cfg, "width", &p->width);
    config_setting_lookup_int(cfg, "height", &p->height);
//    if (config_setting_lookup_int(cfg, "spacing", &i) && i > 0)
//        p->spacing = i;
    if (config_setting_lookup_int(cfg, "setdocktype", &i))
        p->setdocktype = i != 0;
    if (config_setting_lookup_int(cfg, "setpartialstrut", &i))
        p->setstrut = i != 0;
    if (config_setting_lookup_int(cfg, "RoundCorners", &i))
        p->round_corners = i != 0;
    if (config_setting_lookup_int(cfg, "autohide", &i))
        p->autohide = i != 0;
    if (config_setting_lookup_int(cfg, "heightwhenhidden", &i))
        p->height_when_hidden = MAX(0, i);
    if (config_setting_lookup_string(cfg, "tintcolor", &str))
    {
        if (!gdk_rgba_parse (&p->gtintcolor,str))
            gdk_rgba_parse (&p->gtintcolor,"white");
    }
    if (config_setting_lookup_int(cfg, "usefontcolor", &i))
        p->usefontcolor = i != 0;
    if (config_setting_lookup_string(cfg, "fontcolor", &str))
    {
        if (!gdk_rgba_parse (&p->gfontcolor,str))
            gdk_rgba_parse (&p->gfontcolor,"black");
    }
    if (config_setting_lookup_int(cfg, "usefontsize", &i))
        p->usefontsize = i != 0;
    if (config_setting_lookup_int(cfg, "fontsize", &i) && i > 0)
        p->fontsize = i;
    if (config_setting_lookup_string(cfg, "background", &str))
        p->background = str2num(background_pair, str, PANEL_BACKGROUND_GTK);
    if (config_setting_lookup_string(cfg, "backgroundfile", &str))
        p->background_file = g_strdup(str);
    config_setting_lookup_int(cfg, "iconsize", &p->icon_size);

    panel_normalize_configuration(p);

    return 1;
}

void simple_panel_config_save( SimplePanel* panel )
{
    Panel* p = panel->priv;
    gchar *fname;

    fname = _user_config_file_name("panels", p->name);
    /* existance of 'panels' dir ensured in main() */

    if (!config_write_file(p->config, fname)) {
        g_warning("can't open for write %s:", fname);
        g_free( fname );
        return;
    }
    g_free( fname );

    /* save the global config file */
    p->config_changed = 0;
}

static int
panel_parse_plugin(SimplePanel *p, config_setting_t *cfg)
{
    const char *type = NULL;

    ENTER;
    config_setting_lookup_string(cfg, "type", &type);
    DBG("plug %s\n", type);

    if (!type || lxpanel_add_plugin(p, type, cfg, -1) == NULL) {
        g_warning( "lxpanel: can't load %s plugin", type);
        goto error;
    }
    RET(1);

error:
    RET(0);
}

static void panel_add_actions( SimplePanel* p)
{
    const GActionEntry win_action_entries[] =
    {
        {"new-panel", activate_new_panel, NULL, NULL, NULL},
        {"remove-panel", activate_remove_panel, NULL, NULL, NULL},
        {"panel-settings", activate_panel_settings, "i", NULL, NULL},
    };
    g_action_map_add_action_entries(G_ACTION_MAP(p),win_action_entries,G_N_ELEMENTS(win_action_entries),p);
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"edge");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"alignment");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"height");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"width");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"size-type");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"auto-hide");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"auto-hide-size");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"strut");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"dock");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"monitor");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"icon-size");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"margin");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"monitor");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"enable-font-size");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"font-size");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"enable-font-color");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"font-color");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"background-color");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"background-file");
    simple_panel_add_prop_as_action(G_ACTION_MAP(p),"background-type");
}

static int panel_start( SimplePanel *p )
{
    config_setting_t *list, *s;
    int i;

    /* parse global section */
    ENTER;

    list = config_setting_get_member(config_root_setting(p->priv->config), "");
    if (!list || !panel_parse_global(p->priv, config_setting_get_elem(list, 0)))
        RET(0);

    panel_start_gui(p);
    panel_add_actions(p);

    for (i = 1; (s = config_setting_get_elem(list, i)) != NULL; )
        if (strcmp(config_setting_get_name(s), "Plugin") == 0 &&
            panel_parse_plugin(p, s)) /* success on plugin start */
            i++;
        else /* remove invalid data from config */
            config_setting_remove_elem(list, i);

    /* update backgrond of panel and all plugins */
    _panel_update_background(p);
    _panel_update_fonts(p);
    return 1;
}

void panel_destroy(Panel *p)
{
    gtk_widget_destroy(GTK_WIDGET(p->topgwin));
}

SimplePanel* panel_new(GtkApplication* app,const char* config_file, const char* config_name)
{
    SimplePanel* panel = NULL;

    if (G_LIKELY(config_file))
    {
        panel = panel_allocate(app);
        panel->priv->name = g_strdup(config_name);
        panel->priv->app = app;
        win_grp=app;
        g_object_get(G_OBJECT(panel->priv->app),"profile",&cprofile,NULL);
        g_debug("starting panel from file %s",config_file);
        if (!config_read_file(panel->priv->config, config_file) ||
            !panel_start(panel))
        {
            g_warning( "lxpanel: can't start panel");
            gtk_widget_destroy(GTK_WIDGET(panel));
            panel = NULL;
        }
    }
    return panel;
}

GtkOrientation panel_get_orientation(SimplePanel *panel)
{
    return panel->priv->orientation;
}

gint panel_get_icon_size(SimplePanel *panel)
{
    return panel->priv->icon_size;
}

gint panel_get_height(SimplePanel *panel)
{
    return panel->priv->height;
}

gint panel_get_monitor(SimplePanel *panel)
{
    return panel->priv->monitor;
}

GtkIconTheme *panel_get_icon_theme(SimplePanel *panel)
{
    return panel->priv->icon_theme;
}

GtkPositionType panel_get_edge(SimplePanel *panel)
{
    return panel->priv->edge;
}

gboolean panel_is_dynamic(SimplePanel *panel)
{
    return panel->priv->widthtype == PANEL_SIZE_DYNAMIC;
}

GtkWidget *panel_box_new(SimplePanel *panel, gint spacing)
{
    return gtk_box_new(panel->priv->orientation, spacing);
}

GtkWidget *panel_separator_new(SimplePanel *panel)
{
    return gtk_separator_new(panel->priv->orientation);
}

gboolean _class_is_present(const SimplePanelPluginInit *init)
{
    GList *sl;

    for (sl = all_panels; sl; sl = sl->next )
    {
        SimplePanel *panel = (SimplePanel*)sl->data;
        GList *plugins, *p;

        plugins = gtk_container_get_children(GTK_CONTAINER(panel->priv->box));
        for (p = plugins; p; p = p->next)
            if (PLUGIN_CLASS(p->data) == init)
            {
                g_list_free(plugins);
                return TRUE;
            }
        g_list_free(plugins);
    }
    return FALSE;
}
