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

#ifndef PRIVATE_H
#define PRIVATE_H

#include "plugin.h"

#include <gmodule.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <stdio.h>
#include "panel.h"
#include "vala.h"

/* -----------------------------------------------------------------------------
 *   Definitions used by lxpanel main code internally */

/* Extracted from panel.h */
typedef enum {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
} AlignType;

typedef enum {
    SIZE_FILL = 0,
    SIZE_DYNAMIC,
    SIZE_PIXEL,
    SIZE_PERCENT
} SizeType;

typedef enum {
    BACKGROUND_GTK=0,
    BACKGROUND_GNOME,
    BACKGROUND_COLOR,
    BACKGROUND_IMAGE
} BackgroundType;

typedef enum {
    APPEARANCE_CSS_THEME = 1,
    APPEARANCE_BACKGROUND_COLOR = 1 << 1,
    APPEARANCE_BACKGROUND_IMAGE = 1 << 2,
    APPEARANCE_REPEAT_BACKGROUND_IMAGE = 1 << 3,
    APPEARANCE_FONT = 1 << 4,
    APPEARANCE_FONT_COLOR = 1 << 5,
    APPEARANCE_SHADOW = 1 << 6,
    APPEARANCE_ROUND_CORNERS = 1 << 7,
} AppearanceFlags;

typedef enum {
    GEOMETRY_AUTOHIDE = 1,
    GEOMETRY_SHOW_HIDDEN = 1 << 1,
    GEOMETRY_ABOVE = 1 << 2,
    GEOMETRY_STRUT = 1 << 3,
    GEOMETRY_DOCK = 1 << 4,
} GeometryFlags;

#define PANEL_ICON_SIZE               24	/* Default size of panel icons */
#define PANEL_HEIGHT_DEFAULT          26	/* Default height of horizontal panel */
#define PANEL_WIDTH_DEFAULT           150	/* Default "height" of vertical panel */
#define PANEL_HEIGHT_MAX              200	/* Maximum height of panel */
#define PANEL_HEIGHT_MIN              16	/* Minimum height of panel */
#define PANEL_FONT_MAX              20	/* Maximum height of panel fonts*/
#define PANEL_FONT_MIN              2	/* Minimum height of panel fonts*/
#define PANEL_FONT_DEFAULT          10	/* Default height of horizontal panel */
#define PANEL_ICON_HIGHLIGHT          0x202020	/* Constant to pass to icon loader */
#define PANEL_AUTOHIDE_SIZE           2     /* Default autohide size */

/* Context of a panel on a given edge. */
struct _Panel {
    char* name;
    SimplePanel * topgwin;			/* Main panel window */
    GdkDisplay * display;		/* Main panel's GdkDisplay */

    GtkWidget * box;			/* Top level widget */

    GdkRGBA gtintcolor;
    GdkRGBA gfontcolor;

    int ax, ay, aw, ah;  /* prefferd allocation of a panel */
    int cx, cy, cw, ch;  /* current allocation (as reported by configure event) allocation */
    int align, margin;
    GtkPositionType edge;
    GtkOrientation orientation;
    BackgroundType background;
    AppearanceFlags appearance;
    GeometryFlags geometry;
    int widthtype, width;
    int heighttype, height;
    gint monitor;
    gulong strut_size;			/* Values for WM_STRUT_PARTIAL */
    gulong strut_lower;
    gulong strut_upper;
    int strut_edge;

    guint self_destroy : 1;
    guint setdocktype : 1;
    guint setstrut : 1;
    guint round_corners : 1;
    guint usefontcolor : 1;
    guint usefontsize : 1;
    guint fontsize;

    guint autohide : 1;
    guint visible : 1;
    int height_when_hidden;
    guint hide_timeout;
    int icon_size;			/* Icon size */

    char* background_file;

    ValaPanelToplevelSettings* settings; /* Panel configuration data */

    GtkWidget* plugin_pref_dialog;	/* Plugin preference dialog */
    GtkWidget* pref_dialog;		/* preference dialog */
    GtkWidget* margin_control;		/* Margin control in preference dialog */
    GtkWidget* height_control;		/* Height control in preference dialog */
    GtkWidget* width_control;		/* Width control in preference dialog */

    guint initialized : 1;              /* Should be grouped better later, */
    guint ah_far : 1;                   /* placed here for binary compatibility */
    guint ah_state : 3;
    guint mouse_timeout;
};

/* FIXME: optional definitions */
#define STATIC_SEPARATOR
#define STATIC_LAUNCHBAR
#define STATIC_DCLOCK
#define STATIC_DIRMENU
#define STATIC_SPACE

/* Plugins management - new style */
void _prepare_modules(void);
void _unload_modules(void);
void init_plugin_class_list(void);


GtkWidget *simple_panel_add_plugin(SimplePanel *p, ValaPanelPluginSettings* settings, guint pack_pos);
GHashTable *lxpanel_get_all_types(void); /* transfer none */
void set_widget_positions(SimplePanel* p);
void update_positions_on_panel(SimplePanel* p);

extern GQuark lxpanel_plugin_qinit; /* access to LXPanelPluginInit data */
#define PLUGIN_CLASS(_i) ((SimplePanelPluginInit*)g_object_get_qdata(G_OBJECT(_i),lxpanel_plugin_qinit))

extern GQuark lxpanel_plugin_qconf; /* access to congig_setting_t data */

#define PLUGIN_PANEL(_i) ((SimplePanel*)gtk_widget_get_toplevel(_i))

gboolean _class_is_present(const SimplePanelPluginInit *init);

void _panel_show_config_dialog(SimplePanel *panel, GtkWidget *p, GtkWidget *dlg);

void _calculate_position(SimplePanel *panel, GdkRectangle *rect);

void _panel_determine_background_pixmap(SimplePanel * p, GtkWidget * widget);
void _panel_establish_autohide(SimplePanel *p);
void _panel_set_wm_strut(SimplePanel *p);
void _panel_set_panel_configuration_changed(SimplePanel *p);
void _panel_queue_update_background(SimplePanel *p);

void panel_configure(SimplePanel* p, const gchar *sel_page);
gboolean panel_edge_available(Panel* p, int edge, gint monitor);
gboolean _panel_edge_can_strut(SimplePanel *panel, int edge, gint monitor, gulong *size);

/* -----------------------------------------------------------------------------
 *   Deprecated declarations. Kept for compatibility with old code plugins.
 *   Should be removed and appropriate code cleaned on some of next releases. */

/* Extracted from panel.h */
extern void panel_set_dock_type(SimplePanel *p);
extern void panel_set_panel_configuration_changed(Panel *p);
extern SimplePanel* panel_load(GtkApplication *app, const char* config_file, const char* config_name);

/* Extracted from misc.h */
void calculate_position(SimplePanel *np);

char* translate_exec_to_cmd( const char* exec, const char* icon,
                             const char* title, const char* fpath );

void show_error( GtkWindow* parent_win, const char* msg );
#endif
