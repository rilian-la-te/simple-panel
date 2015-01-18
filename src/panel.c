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
static gulong monitors_handler = 0;
GList* all_panels = NULL;

gboolean is_restarting = FALSE;

gboolean is_in_lxde = FALSE;

gchar* cprofile = NULL;

static void simple_panel_get_preferred_size(GtkWidget* widget, GtkRequisition* min, GtkRequisition* nat);
static void panel_start_gui(SimplePanel *p);
static void ah_start(SimplePanel *p);
static void ah_stop(SimplePanel *p);
static void panel_update_fonts(SimplePanel * p);
static void activate_remove_plugin(GSimpleAction* action, GVariant* param, gpointer pl);
static void activate_new_panel(GSimpleAction* action, GVariant* param, gpointer data);
static void activate_remove_panel(GSimpleAction* action, GVariant* param, gpointer data);
static void activate_panel_settings(GSimpleAction* action, GVariant* param, gpointer data);
static gboolean _panel_set_monitor(SimplePanel* panel, int monitor);
static void panel_add_actions( SimplePanel* p);
PanelGSettings* simple_panel_create_gsettings( SimplePanel* panel );
static void panel_update_background(SimplePanel * panel);

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

    if (p->settings)
        panel_gsettings_free(p->settings,FALSE);

    g_free( p->background_file );

    g_free( p->name );
    g_free(p);

    G_OBJECT_CLASS(lxpanel_parent_class)->finalize(object);
}

static void panel_stop_gui(SimplePanel *self)
{
    Panel *p = self->priv;

    if (p->autohide)
        ah_stop(self);

    if (p->pref_dialog != NULL)
        gtk_widget_destroy(p->pref_dialog);
    p->pref_dialog = NULL;

    if (p->plugin_pref_dialog != NULL)
        /* just close the dialog, it will do all required cleanup */
        gtk_dialog_response(GTK_DIALOG(p->plugin_pref_dialog), GTK_RESPONSE_CLOSE);
    if (p->settings != NULL)
    {
        panel_gsettings_free(p->settings,FALSE);
        p->settings = NULL;
    }
    if (p->initialized)
    {
        gtk_application_remove_window(GTK_APPLICATION(win_grp), GTK_WINDOW(self));
        all_panels = gtk_application_get_windows(GTK_APPLICATION(p->app));
        gdk_flush();
        p->initialized = FALSE;
    }

    if (gtk_bin_get_child(GTK_BIN(self)))
    {
        gtk_widget_destroy(p->box);
        p->box = NULL;
    }
}
static void simple_panel_destroy(GtkWidget* object)
{
    SimplePanel* self = LXPANEL(object);

    panel_stop_gui(self);

	GTK_WIDGET_CLASS(lxpanel_parent_class)->destroy(object);
}

static void lxpanel_size_allocate(GtkWidget *widget, GtkAllocation *a)
{
    SimplePanel *panel = LXPANEL(widget);
    Panel *p = panel->priv;
    gint x, y, w;
    GTK_WIDGET_CLASS(lxpanel_parent_class)->size_allocate(widget, a);
    gtk_widget_set_allocation(widget,a);
    if (p->widthtype == PANEL_SIZE_DYNAMIC && p->box)
    {
        (p->orientation == GTK_ORIENTATION_HORIZONTAL) ? gtk_widget_get_preferred_width (p->box,NULL, &w) : gtk_widget_get_preferred_height (p->box,NULL, &w);
        if (w!=p->width)
            g_settings_set_int(p->settings->toplevel_settings, PANEL_PROP_WIDTH,w);
    }
    if (p->heighttype == PANEL_SIZE_DYNAMIC)
        p->height = (p->orientation == GTK_ORIENTATION_HORIZONTAL) ? a->height : a->width;

    if (!gtk_widget_get_realized(widget))
        return;
    gdk_window_get_origin(gtk_widget_get_window(widget),&x,&y);
    _calculate_position(panel,a);
    p->ax = a->x;
    p->ay = a->y;
    if (a->width != p->aw || a->height != p->ah || p->ax != x || p->ay != y)
    {
        p->aw = a->width;
        p->ah = a->height;
        gtk_widget_set_size_request(GTK_WIDGET(panel),p->aw,p->ah);
        gtk_window_move(GTK_WINDOW(widget), p->ax, p->ay);
        _panel_set_wm_strut(LXPANEL(widget));
    }
    if (gtk_widget_get_mapped(widget))
        _panel_establish_autohide(panel);
}

static void
lxpanel_get_preferred_width (GtkWidget *widget,
							   gint      *minimal_width,
							   gint      *natural_width)
{
  GTK_WIDGET_CLASS(lxpanel_parent_class)->get_preferred_width(widget, minimal_width,natural_width);
  GtkRequisition min;
  simple_panel_get_preferred_size(widget,&min,NULL);
  *minimal_width=*natural_width=min.width;
}

static void
lxpanel_get_preferred_height (GtkWidget *widget,
								gint      *minimal_height,
								gint      *natural_height)
{
    GTK_WIDGET_CLASS(lxpanel_parent_class)->get_preferred_height(widget, minimal_height,natural_height);
    GtkRequisition min;
    simple_panel_get_preferred_size(widget,&min,NULL);
    *minimal_height=*natural_height=min.height;
}

static void simple_panel_get_preferred_size(GtkWidget* widget, GtkRequisition* min, GtkRequisition* nat)
{
    SimplePanel *panel = LXPANEL(widget);
    Panel *p = panel->priv;
    GdkRectangle rect;

    if (!p->visible)
        /* When the panel is in invisible state, the content box also got hidden, thus always
         * report 0 size.  Ask the content box instead for its size. */
        gtk_widget_get_preferred_size(p->box, min, nat);

    rect.width = min->width;
    rect.height = min->height;
    _calculate_position(panel, &rect);
    min->width = rect.width;
    min->height = rect.height;
    nat=min;
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
    gboolean geometry,background,configuration,fonts, updatestrut;
    switch (prop_id) {
    case PROP_NAME:
        toplevel->priv->name = g_strdup(g_value_get_string(value));
        break;
    case PROP_EDGE:
        toplevel->priv->edge = g_value_get_enum (value);
        configuration = TRUE;
        geometry = TRUE;
        updatestrut = TRUE;
        break;
    case PROP_HEIGHT:
        toplevel->priv->height = g_value_get_int(value);
        geometry = TRUE;
        updatestrut = TRUE;
        break;
    case PROP_WIDTH:
        toplevel->priv->width = g_value_get_int(value);
        geometry = TRUE;
        updatestrut = TRUE;
        break;
    case PROP_SIZE_TYPE:
        toplevel->priv->widthtype = g_value_get_enum(value);
        geometry = TRUE;
        updatestrut = TRUE;
        break;
    case PROP_MONITOR:
        _panel_set_monitor(toplevel,g_value_get_int(value));
        geometry = TRUE;
        configuration = TRUE;
        break;
    case PROP_ALIGNMENT:
        toplevel->priv->align = g_value_get_enum(value);
        geometry = TRUE;
        updatestrut = TRUE;
        break;
    case PROP_MARGIN:
        toplevel->priv->margin = g_value_get_int(value);
        geometry = TRUE;
        updatestrut = TRUE;
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
        updatestrut = TRUE;
        break;
    case PROP_AUTOHIDE_SIZE:
        toplevel->priv->height_when_hidden = g_value_get_int(value);
        geometry=TRUE;
        updatestrut = TRUE;
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
        updatestrut = TRUE;
        break;
    case PROP_STRUT:
        toplevel->priv->setstrut = g_value_get_boolean(value);
        geometry = TRUE;
        updatestrut = TRUE;
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
            gtk_widget_queue_resize(GTK_WIDGET(toplevel));
        if (background)
            panel_update_background(toplevel);
        if (fonts)
            panel_update_fonts(toplevel);
        if (updatestrut)
            _panel_set_wm_strut(toplevel);
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
        break;
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
        g_value_set_enum(value, toplevel->priv->align);
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
    widget_class->destroy = simple_panel_destroy;
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
                    PANEL_PROP_EDGE,
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
                    PANEL_PROP_HEIGHT,
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
                    PANEL_PROP_WIDTH,
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
                    PANEL_PROP_SIZE_TYPE,
                    "Size Type",
                    "Type of panel size counting",
                    PANEL_SIZE_TYPE,
                    PANEL_SIZE_PERCENT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
                gobject_class,
                PROP_ALIGNMENT,
                g_param_spec_enum(
                    PANEL_PROP_ALIGNMENT,
                    "Alignment",
                    "Panel alignment side",
                    PANEL_ALIGN_TYPE,
                    PANEL_ALIGN_CENTER,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
                gobject_class,
                PROP_MARGIN,
                g_param_spec_int(
                    PANEL_PROP_MARGIN,
                    "Margin",
                    "If alignment is not centered, margin is used to calculate panel start point",
                    0,
                    G_MAXINT,
                    0,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_MONITOR,
                g_param_spec_int (
                    PANEL_PROP_MONITOR,
                    "Xinerama monitor",
                    "The monitor (in terms of Xinerama) which the panel is on",
                    -1,
                    G_MAXINT,
                    G_MAXINT,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_AUTOHIDE,
                g_param_spec_boolean (
                    PANEL_PROP_AUTOHIDE,
                    "Auto hide",
                    "Automatically hide the panel when the mouse leaves the panel",
                    FALSE,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_AUTOHIDE_SIZE,
                g_param_spec_int (
                    PANEL_PROP_AUTOHIDE_SIZE,
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
                    PANEL_PROP_BACKGROUND_TYPE,
                    "Background Type",
                    "Type of panel background",
                    PANEL_BACKGROUND_TYPE,
                    PANEL_BACKGROUND_GTK,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_ENABLEFONTCOLOR,
                g_param_spec_boolean(
                    PANEL_PROP_ENABLE_FONT_COLOR,
                    "Enable font color",
                    "Enable custom font color",
                    FALSE,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_ENABLEFONTSIZE,
                g_param_spec_boolean(
                    PANEL_PROP_ENABLE_FONT_SIZE,
                    "Enable font size",
                    "Enable custom font size",
                    FALSE,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
                gobject_class,
                PROP_ICON_SIZE,
                g_param_spec_int(
                    PANEL_PROP_ICON_SIZE,
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
                    PANEL_PROP_BACKGROUND_FILE,
                    "Background file",
                    "Background file of this panel",
                    NULL,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_TINTCOLOR,
                g_param_spec_string (
                    PANEL_PROP_BACKGROUND_COLOR,
                    "Background color",
                    "Background color of this panel",
                    "white",
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_FONTCOLOR,
                g_param_spec_string (
                    PANEL_PROP_FONT_COLOR,
                    "Font color",
                    "Font color color of this panel",
                    "black",
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(
                gobject_class,
                PROP_FONTSIZE,
                g_param_spec_int(
                    PANEL_PROP_FONT_SIZE,
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
                    PANEL_PROP_STRUT,
                    "Set strut",
                    "Set strut to crop it from maximized windows",
                    TRUE,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_DOCK,
                g_param_spec_boolean (
                    PANEL_PROP_DOCK,
                    "As dock",
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
	GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(self));
	GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
	gtk_widget_set_visual(GTK_WIDGET(self), visual);
}


/* Allocate and initialize new Panel structure. */
static SimplePanel* panel_allocate(GtkApplication* app)
{
    return g_object_new(LX_TYPE_PANEL,
                        "border-width", 0,
                        "decorated", FALSE,
                        "name", "PanelWindow",
                        "resizable", FALSE,
                        "title", "panel",
                        "type-hint", GDK_WINDOW_TYPE_HINT_DOCK,
                        "window-position", GTK_WIN_POS_NONE,
                        "skip-taskbar-hint", TRUE,
                        "skip-pager-hint", TRUE,
                        "accept-focus", FALSE,
                        NULL);
}

/* TODO: Make constructor from various functions like panel_new */
///* Allocate and initialize new Panel structure. */
//static SimplePanel* panel_constructor(GtkApplication* app)
//{
//    return g_object_new(LX_TYPE_PANEL, NULL);
//}

/* Normalize panel configuration after load from file or reconfiguration. */
static void panel_normalize_configuration(Panel* p)
{
    _panel_set_panel_configuration_changed( p->topgwin );
    if (p->widthtype == PANEL_SIZE_PERCENT && p->width > 100)
        p->width = 100;
    if (p->monitor < 0)
        p->monitor = -1;
}


/****************************************************
 *         panel's handlers for WM events           *
 ****************************************************/

gboolean _panel_edge_can_strut(SimplePanel *panel, int edge, gint monitor, gulong *size)
{
    Panel *p;
    GdkScreen *screen;
    GdkRectangle rect;
    GdkRectangle rect2;
    gint n, i;
    gulong s;

    if (!gtk_widget_get_mapped(GTK_WIDGET(panel)))
        return FALSE;

    p = panel->priv;
    /* Handle autohide case.  EWMH recommends having the strut be the minimized size. */
    if (p->autohide)
        s = p->height_when_hidden;
    else switch (edge)
    {
    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
        s = p->aw;
        break;
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
        s = p->ah;
        break;
    default: /* error! */
        return FALSE;
    }

    if (monitor < 0) /* screen span */
    {
        if (G_LIKELY(size))
            *size = s;
        return TRUE;
    }

    screen = gtk_widget_get_screen(GTK_WIDGET(panel));
    n = gdk_screen_get_n_monitors(screen);
    if (monitor >= n) /* hidden now */
        return FALSE;
    gdk_screen_get_monitor_geometry(screen, monitor, &rect);
    switch (edge)
    {
        case GTK_POS_LEFT:
            rect.width = rect.x;
            rect.x = 0;
            s += rect.width;
            break;
        case GTK_POS_RIGHT:
            rect.x += rect.width;
            rect.width = gdk_screen_get_width(screen) - rect.x;
            s += rect.width;
            break;
        case GTK_POS_TOP:
            rect.height = rect.y;
            rect.y = 0;
            s += rect.height;
            break;
        case GTK_POS_BOTTOM:
            rect.y += rect.height;
            rect.height = gdk_screen_get_height(screen) - rect.y;
            s += rect.height;
            break;
        default: ;
    }
    if (rect.height == 0 || rect.width == 0) ; /* on a border of monitor */
    else
    {
        n = gdk_screen_get_n_monitors(screen);
        for (i = 0; i < n; i++)
        {
            if (i == monitor)
                continue;
            gdk_screen_get_monitor_geometry(screen, i, &rect2);
            if (gdk_rectangle_intersect(&rect, &rect2, NULL))
                /* that monitor lies over the edge */
                return FALSE;
        }
    }
    if (G_LIKELY(size))
        *size = s;
    return TRUE;
}


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
            strut_lower = p->ay;
            strut_upper = p->ay + p->ah;
            break;
        case GTK_POS_RIGHT:
            index = 1;
            strut_lower = p->ay;
            strut_upper = p->ay + p->ah;
            break;
        case GTK_POS_TOP:
            index = 2;
            strut_lower = p->ax;
            strut_upper = p->ax + p->aw;
            break;
        case GTK_POS_BOTTOM:
            index = 3;
            strut_lower = p->ax;
            strut_upper = p->ax + p->aw;
            break;
        default:
            return;
    }

    /* Set up strut value in property format. */
    gulong desired_strut[12];
    memset(desired_strut, 0, sizeof(desired_strut));
    if (p->setstrut &&
        _panel_edge_can_strut(panel, p->edge, p->monitor, &strut_size))
    {
        desired_strut[index] = strut_size;
        desired_strut[4 + index * 2] = strut_lower;
        desired_strut[5 + index * 2] = strut_upper-1;
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
static void panel_update_background(SimplePanel * panel)
{
	Panel * p = panel->priv;
    gchar* css = NULL;
    GdkRGBA color;
    gboolean system = TRUE;
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
    else if (p->background == PANEL_BACKGROUND_GNOME)
        gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(panel)),"gnome-panel-menu-bar");
    else
        gtk_style_context_remove_class(gtk_widget_get_style_context(GTK_WIDGET(panel)),"gnome-panel-menu-bar");

    if (css)
    {
        css_apply_with_class(GTK_WIDGET(panel),css,"-simple-panel-background",system);
        g_free(css);
    }
    else if (system)
        css_apply_with_class(GTK_WIDGET(panel),"","-simple-panel-background",system);
}

void panel_update_fonts(SimplePanel * p)
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
    return TRUE;
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
    GdkRectangle rect;

    if (p->ah_state != ah_state) {
        p->ah_state = ah_state;
        switch (ah_state) {
        case AH_STATE_VISIBLE:
            p->visible = TRUE;
            _calculate_position(panel, &rect);
            gtk_window_move(GTK_WINDOW(panel), rect.x, rect.y);
            gtk_widget_show(GTK_WIDGET(panel));
            gtk_widget_show(p->box);
            gtk_widget_queue_resize(GTK_WIDGET(panel));
            gtk_window_stick(GTK_WINDOW(panel));
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
}

/* starts autohide behaviour */
static void ah_start(SimplePanel *p)
{
    if (!p->priv->mouse_timeout)
        p->priv->mouse_timeout = g_timeout_add(PERIOD, (GSourceFunc) mouse_watch, p);
}

/* stops autohide */
static void ah_stop(SimplePanel *p)
{
    if (p->priv->mouse_timeout) {
        g_source_remove(p->priv->mouse_timeout);
        p->priv->mouse_timeout = 0;
    }
    if (p->priv->hide_timeout) {
        g_source_remove(p->priv->hide_timeout);
        p->priv->hide_timeout = 0;
    }
}
/* end of the autohide code
 * ------------------------------------------------------------- */

void activate_panel_settings(GSimpleAction *action, GVariant *param, gpointer data)
{
    const gchar* page = g_variant_get_string(param,NULL);
    SimplePanel* p = (SimplePanel*)data;
    panel_configure(p,page);

}

static void activate_config_plugin(GSimpleAction *action, GVariant *param, gpointer pl)
{
    GtkWidget* plugin = (GtkWidget*) pl;
    Panel *panel = PLUGIN_PANEL(plugin)->priv;

    lxpanel_plugin_show_config_dialog(plugin);
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
    PluginGSettings* settings = g_object_get_qdata(G_OBJECT(plugin), lxpanel_plugin_qconf);
    panel_gsettings_remove_plugin_settings(panel->settings,settings->plugin_number);
    /* reset conf pointer because the widget still may be referenced by configurator */
    g_object_set_qdata(G_OBJECT(plugin), lxpanel_plugin_qconf, NULL);
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
static char* gen_panel_name( GtkPositionType edge, gint mon)
{
    const gchar* edge_str;
    if (edge == GTK_POS_TOP)
        edge_str="top";
    if (edge == GTK_POS_BOTTOM)
        edge_str="bottom";
    if (edge == GTK_POS_LEFT)
        edge_str="left";
    if (edge == GTK_POS_RIGHT)
        edge_str="right";
    char* name = NULL;
    char* dir = _user_config_file_name("panels", NULL);
    int i;
    for( i = 0; i < G_MAXINT; ++i )
    {
        char* f;
        name = g_strdup_printf( "%s-m%d-%d", edge_str, mon, i );

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
    gint m, monitors;
    GtkWidget* msg;
    gint e;
    GdkScreen *screen;
    SimplePanel *new_panel = panel_allocate(panel->priv->app);
    Panel *p = new_panel->priv;
    p->app = panel->priv->app;
    g_object_get(G_OBJECT(new_panel->priv->app),"profile",&cprofile,NULL);

    /* Allocate the edge. */
    screen = gdk_screen_get_default();
    g_assert(screen);
    monitors = gdk_screen_get_n_monitors(screen);
    /* try to allocate edge on current monitor first */
    m = panel->priv->monitor;
    if (m < 0)
    {
        /* panel is spanned over the screen, guess from pointer now */
        gint x, y;
        GdkDeviceManager *manager = gdk_display_get_device_manager (gdk_screen_get_display (screen));
        GdkDevice *device = gdk_device_manager_get_client_pointer (manager);
        gdk_device_get_position(device,&screen, &x, &y);
        m = gdk_screen_get_monitor_at_point(screen, x, y);
    }
    for (e = 3; e >= 0; e--)
    {
        if (panel_edge_available(p, e, m))
        {
            p->edge = e;
            p->monitor = m;
            goto found_edge;
        }
    }
    /* try all monitors */
    for(m=0; m<monitors; ++m)
    {
        /* try each of the four edges */
        for(e = 3; e >= 0; e--)
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
    msg = gtk_message_dialog_new
            (GTK_WINDOW(panel),
             GTK_DIALOG_DESTROY_WITH_PARENT,
             GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,
             "There is no room for another panel. All the edges are taken.");
    panel_apply_icon(GTK_WINDOW(msg));
    gtk_window_set_title( (GtkWindow*)msg, _("Error") );
    gtk_dialog_run(GTK_DIALOG(msg));
    gtk_widget_destroy(msg);
    return;

found_edge:
    p->name = gen_panel_name(p->edge,p->monitor);
    p->settings = simple_panel_create_gsettings(new_panel);
    g_settings_set_enum(p->settings->toplevel_settings,PANEL_PROP_EDGE,p->edge);
    g_settings_set_int(p->settings->toplevel_settings,PANEL_PROP_MONITOR,p->monitor);
    panel_add_actions(new_panel);
    panel_normalize_configuration(p);
    panel_start_gui(new_panel);
    panel_configure(new_panel, "geometry");
    gtk_widget_show_all(GTK_WIDGET(new_panel));
    gtk_widget_queue_draw(GTK_WIDGET(new_panel));
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
        panel_stop_gui(panel);
        gtk_widget_destroy(GTK_WIDGET(panel));
        gchar *fname;
        /* delete the config file of this panel */
        fname = _user_config_file_name("panels", panel->priv->name);
        g_unlink( fname );
        g_free(fname);
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
    GtkBuilder* builder;
    g_action_map_remove_action(G_ACTION_MAP(panel),"remove-plugin");
    g_action_map_remove_action(G_ACTION_MAP(panel),"config-plugin");
    builder = gtk_builder_new_from_resource("/org/simple/panel/lib/menus.ui");
    gmenu = G_MENU(gtk_builder_get_object(builder,"panel-context-menu"));
    if (plugin)
    {
        init = PLUGIN_CLASS(plugin);
        /* create single item - plugin instance settings */
        const GActionEntry config_entry[] = {
            {"config-plugin",activate_config_plugin,NULL,NULL,NULL},
        };
        const GActionEntry remove_entry[] = {
            {"remove-plugin",activate_remove_plugin,NULL,NULL,NULL},
        };
        g_action_map_add_action_entries(G_ACTION_MAP(panel),remove_entry,
                                        G_N_ELEMENTS(remove_entry),
                                        (gpointer)plugin);
        if (init->config)
            g_action_map_add_action_entries(G_ACTION_MAP(panel),config_entry,
                                        G_N_ELEMENTS(config_entry),
                                        (gpointer)plugin);
        gmenusection = G_MENU(gtk_builder_get_object(builder,"plugin-section"));
        if (init->update_context_menu != NULL)
            init->update_context_menu(plugin,gmenusection);
    }
    ret = GTK_MENU(gtk_menu_new_from_model(G_MENU_MODEL(gmenu)));
    if (plugin)
        gtk_menu_attach_to_widget(ret,GTK_WIDGET(plugin),NULL);
    else
        gtk_menu_attach_to_widget(ret,GTK_WIDGET(panel),NULL);
    gtk_widget_show_all(GTK_WIDGET(ret));
    g_object_unref(builder);
    return ret;
}

/****************************************************
 *         panel creation                           *
 ****************************************************/
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

static void
panel_start_gui(SimplePanel *panel)
{
    gulong val, position;
    Panel *p = panel->priv;
    GtkWidget *w = GTK_WIDGET(panel);
    GSList* l;

    p->display = gdk_display_get_default();
    p->ax = p->ay = p->aw = p->ah = 0;
    gtk_window_set_wmclass(GTK_WINDOW(panel), "panel", "simple-panel");
    gtk_window_stick(GTK_WINDOW(panel));

    gtk_application_add_window(win_grp, (GtkWindow*)panel);
    all_panels = gtk_application_get_windows(p->app);
    gtk_widget_add_events( w, GDK_BUTTON_PRESS_MASK );

    gtk_widget_realize(w);

    // main layout manager as a single child of panel
    p->box = panel_box_new(panel, 0);
    gtk_container_set_border_width(GTK_CONTAINER(p->box), 0);
    gtk_container_add(GTK_CONTAINER(panel), p->box);
    gtk_widget_show(p->box);
    if (p->round_corners)
        make_round_corners(p);

    panel_set_dock_type(panel);

    panel_gsettings_init_plugin_list(p->settings);
    for (l = p->settings->all_settings; l != NULL; l = l->next)
    {
        position = g_settings_get_uint(((PluginGSettings*)l->data)->default_settings,DEFAULT_PLUGIN_KEY_POSITION);
        simple_panel_add_plugin(panel,l->data,position);
    }
    update_positions_on_panel(panel);
    gtk_window_present(GTK_WINDOW(panel));
    g_object_set(G_OBJECT(panel),PANEL_PROP_AUTOHIDE,p->autohide,NULL);
    g_object_set(G_OBJECT(panel),PANEL_PROP_STRUT,p->setstrut,NULL);
    p->initialized = TRUE;
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
        g_settings_set_int(p->settings->toplevel_settings,
                               PANEL_PROP_HEIGHT,
                               ((p->orientation == GTK_ORIENTATION_HORIZONTAL)
                                ? PANEL_HEIGHT_DEFAULT
                                : PANEL_WIDTH_DEFAULT));
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
}

PanelGSettings* simple_panel_create_gsettings( SimplePanel* panel )
{
    Panel* p = panel->priv;
    gchar *fname;

    fname = _user_config_file_name("panels", p->name);
    PanelGSettings* s =  panel_gsettings_create(fname);
    g_free(fname);
    return s;
}

static void panel_add_actions( SimplePanel* p)
{
    const GActionEntry win_action_entries[] =
    {
        {"new-panel", activate_new_panel, NULL, NULL, NULL},
        {"remove-panel", activate_remove_panel, NULL, NULL, NULL},
        {"panel-settings", activate_panel_settings, "s", NULL, NULL},
    };
    g_action_map_add_action_entries(G_ACTION_MAP(p),win_action_entries,G_N_ELEMENTS(win_action_entries),p);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_EDGE);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_ALIGNMENT);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_HEIGHT);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_WIDTH);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_SIZE_TYPE);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_AUTOHIDE);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_AUTOHIDE_SIZE);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_STRUT);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_DOCK);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_ICON_SIZE);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_MARGIN);
    simple_panel_bind_gsettings(G_OBJECT(p),p->priv->settings->toplevel_settings,PANEL_PROP_MONITOR);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_ENABLE_FONT_SIZE);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_FONT_SIZE);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_ENABLE_FONT_COLOR);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_FONT_COLOR);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_BACKGROUND_COLOR);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_BACKGROUND_FILE);
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(p),p->priv->settings->toplevel_settings,PANEL_PROP_BACKGROUND_TYPE);
}

static void on_monitors_changed(GdkScreen* screen, gpointer unused)
{
    GList *pl;
    int monitors = gdk_screen_get_n_monitors(screen);

    for (pl = all_panels; pl; pl = pl->next)
    {
        SimplePanel *p = pl->data;

        /* handle connecting and disconnecting monitors now */
        if (p->priv->monitor < monitors && !p->priv->initialized)
            panel_start_gui(p);
        else if (p->priv->monitor >= monitors && p->priv->initialized)
            panel_stop_gui(p);
        else
        {
            ah_state_set(p, AH_STATE_VISIBLE);
            gtk_widget_queue_resize(GTK_WIDGET(p));
        }
    }
}

static int panel_start( SimplePanel *p )
{
    GdkScreen *screen = gdk_screen_get_default();
    panel_add_actions(p);
    /* update backgrond of panel and all plugins */
    if (p->priv->monitor < gdk_screen_get_n_monitors(screen))
        panel_start_gui(p);
    if (monitors_handler == 0)
        monitors_handler = g_signal_connect(screen, "monitors-changed",
                                            G_CALLBACK(on_monitors_changed), NULL);
    return 1;
}

void panel_destroy(Panel *p)
{
    gtk_widget_destroy(GTK_WIDGET(p->topgwin));
}

SimplePanel* panel_load(GtkApplication* app,const char* config_file, const char* config_name)
{
    SimplePanel* panel = NULL;

    if (G_LIKELY(config_file))
    {
        panel = panel_allocate(app);
        panel->priv->name = g_strdup(config_name);
        panel->priv->app = app;
        win_grp=app;
        all_panels = gtk_application_get_windows(panel->priv->app);
        g_object_get(G_OBJECT(panel->priv->app),"profile",&cprofile,NULL);
        g_debug("starting panel from file %s",config_file);
        panel->priv->settings = panel_gsettings_create(config_file);
        if (!panel_start(panel))
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
GtkApplication* panel_get_application(SimplePanel *panel)
{
    return panel->priv->app;
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
        if (panel->priv->box == NULL)
            continue;
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
