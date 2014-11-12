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
#include "conf.h"
#include "conf-gsettings.h"

#include <gmodule.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <stdio.h>
#include "panel.h"

/* -----------------------------------------------------------------------------
 *   Definitions used by lxpanel main code internally */

/* Extracted from panel.h */
typedef enum {
    PANEL_ALLIGN_LEFT,
    PANEL_ALLIGN_CENTER,
    PANEL_ALLIGN_RIGHT
} PanelAllignType;
typedef enum {
    PANEL_SIZE_FILL=0,
    PANEL_SIZE_DYNAMIC,
    PANEL_SIZE_PIXEL,
    PANEL_SIZE_PERCENT
} PanelSizeType;
typedef enum {
    PANEL_BACKGROUND_GTK=0,
    PANEL_BACKGROUND_GNOME,
    PANEL_BACKGROUND_CUSTOM_COLOR,
    PANEL_BACKGROUND_CUSTOM_IMAGE
} PanelBackgroundType;

typedef enum {
    PANEL_EDGE_TOP=0,
    PANEL_EDGE_BOTTOM,
    PANEL_EDGE_LEFT,
    PANEL_EDGE_RIGHT,
} PanelEdgeType;

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
#define GSETTINGS_PREFIX              "conf"

/* to check if we are in LXDE */
extern gboolean is_in_lxde;

/* Context of a panel on a given edge. */
struct _Panel {
    char* name;
    SimplePanel * topgwin;			/* Main panel window */
    GtkApplication* app; /* Panels holding application path */
    GdkDisplay * display;		/* Main panel's GdkDisplay */
    GtkIconTheme* icon_theme; /*Default icon theme*/

    GtkWidget * box;			/* Top level widget */

    GtkWidget *(*my_box_new) (gboolean, gint);
    GtkWidget *(*my_separator_new) ();

    GdkRGBA gtintcolor;
    GdkRGBA gfontcolor;

    int ax, ay, aw, ah;  /* prefferd allocation of a panel */
    int cx, cy, cw, ch;  /* current allocation (as reported by configure event) allocation */
    int allign, margin;
    PanelEdgeType edge;
    GtkOrientation orientation;
    PanelBackgroundType background;
    int widthtype, width;
    int heighttype, height;
    gint monitor;
    gulong strut_size;			/* Values for WM_STRUT_PARTIAL */
    gulong strut_lower;
    gulong strut_upper;
    int strut_edge;

    guint config_changed : 1;
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

    PanelConf * config;                 /* Panel configuration data */

    PanelGSettings* settings; /* Panel configuration data */

    GtkWidget* plugin_pref_dialog;	/* Plugin preference dialog */
    GtkWidget* pref_dialog;		/* preference dialog */
    GtkWidget* margin_control;		/* Margin control in preference dialog */
    GtkWidget* alignment_left_label;	/* Label of alignment: left control */
    GtkWidget* alignment_right_label;	/* Label of alignment: right control */
    GtkWidget* height_control;		/* Height control in preference dialog */
    GtkWidget* width_control;		/* Width control in preference dialog */

    guint initialized : 1;              /* Should be grouped better later, */
    guint ah_far : 1;                   /* placed here for binary compatibility */
    guint ah_state : 3;
    guint background_update_queued;
    guint mouse_timeout;
};

typedef struct {
    char *name;
    char *disp_name;
    void (*cmd)(void);
} Command;

#define FBPANEL_WIN(win)  gdk_window_lookup(win)

/* Extracted from misc.h */
typedef struct {
    int num;
    gchar *str;
} pair;

extern pair allign_pair[];
extern pair edge_pair[];
extern pair strut_pair[];
extern pair background_pair[];
extern pair bool_pair[];

int str2num(pair *p, const gchar *str, int defval);
const gchar *num2str(pair *p, int num, const gchar *defval);

//void _queue_panel_calculate_size(Panel *panel);

/* FIXME: optional definitions */
#define STATIC_SEPARATOR
#define STATIC_LAUNCHBAR
#define STATIC_LAUNCHTASKBAR
#define STATIC_DCLOCK
#define STATIC_WINCMD
#define STATIC_DIRMENU
#define STATIC_TASKBAR
#define STATIC_PAGER
#define STATIC_TRAY
#define STATIC_MENU
#define STATIC_SPACE
#define STATIC_ICONS

/* Plugins management - new style */
void _prepare_modules(void);
void _unload_modules(void);

GtkWidget *lxpanel_add_plugin(SimplePanel *p, const char *name, config_setting_t *cfg, gint at);
GtkWidget *simple_panel_add_plugin(SimplePanel *p, const char *name, PluginGSettings* settings, gint* pack_pos);
GHashTable *lxpanel_get_all_types(void); /* transfer none */

extern GQuark lxpanel_plugin_qinit; /* access to LXPanelPluginInit data */
#define PLUGIN_CLASS(_i) ((SimplePanelPluginInit*)g_object_get_qdata(G_OBJECT(_i),lxpanel_plugin_qinit))

extern GQuark lxpanel_plugin_qconf; /* access to congig_setting_t data */

#define PLUGIN_PANEL(_i) ((SimplePanel*)gtk_widget_get_toplevel(_i))

gboolean _class_is_present(const SimplePanelPluginInit *init);

void _panel_show_config_dialog(SimplePanel *panel, GtkWidget *p, GtkWidget *dlg);

void _calculate_position(SimplePanel *panel);

void _panel_determine_background_pixmap(SimplePanel * p, GtkWidget * widget);
void _panel_establish_autohide(SimplePanel *p);
void _panel_set_wm_strut(SimplePanel *p);
void _panel_set_panel_configuration_changed(SimplePanel *p);
void _panel_queue_update_background(SimplePanel *p);

void panel_configure(SimplePanel* p, int sel_page);
gboolean panel_edge_available(Panel* p, int edge, gint monitor);
void restart(void);
void logout(void);
void gtk_run(void);

/* -----------------------------------------------------------------------------
 *   Deprecated declarations. Kept for compatibility with old code plugins.
 *   Should be removed and appropriate code cleaned on some of next releases. */

extern Command commands[];

/* Extracted from panel.h */
extern int verbose;

extern void panel_destroy(Panel *p);
extern void panel_adjust_geometry_terminology(Panel *p);
extern void panel_determine_background_pixmap(Panel * p, GtkWidget * widget, GdkWindow * window);
extern void panel_draw_label_text(Panel * p, GtkWidget * label, const char * text,
                                  gboolean bold, float custom_size_factor,
                                  gboolean custom_color);
extern void panel_establish_autohide(Panel *p);
extern void panel_image_set_from_file(Panel * p, GtkWidget * image, const char * file);
extern gboolean panel_image_set_icon_theme(Panel * p, GtkWidget * image, const gchar * icon);
extern void panel_set_wm_strut(Panel *p);
extern void panel_set_dock_type(SimplePanel *p);
extern void panel_set_panel_configuration_changed(Panel *p);
extern void panel_update_background( Panel* p );
extern void panel_update_fonts( Panel * p);
extern SimplePanel* panel_new(GtkApplication *app, const char* config_file, const char* config_name);
extern SimplePanel* panel_load(GtkApplication *app, const char* config_file, const char* config_name);

/* if current window manager is EWMH conforming. */
extern gboolean is_ewmh_supported;

/*
 This function is used to re-create a new box with different
 orientation from the old one, add all children of the old one to
 the new one, and then destroy the old box.
 It's mainly used when we need to change the orientation of the panel or
 any plugin with a layout box. Since GtkHBox cannot be changed to GtkVBox,
 recreating a new box to replace the old one is required.
*/
GtkWidget* recreate_box( GtkBox* oldbox, GtkOrientation orientation );

extern const char* lxpanel_get_file_manager();


/* Extracted from misc.h */
typedef struct _Plugin Plugin;

enum { LINE_NONE, LINE_BLOCK_START, LINE_BLOCK_END, LINE_VAR };

typedef struct {
    int num, len, type;
    gchar str[256];
    gchar *t[3];
} line;

void calculate_position(Panel *np);

extern int lxpanel_get_line(char **fp, line *s);
extern int lxpanel_put_line(FILE* fp, const char* format, ...);
#define lxpanel_put_str(fp, name, val) (G_UNLIKELY( !(val) || !*(val) )) ? 0 : lxpanel_put_line(fp, "%s=%s", name, val)
#define lxpanel_put_bool(fp, name, val) lxpanel_put_line(fp, "%s=%c", name, (val) ? '1' : '0')
#define lxpanel_put_int(fp, name, val) lxpanel_put_line(fp, "%s=%d", name, val)

GtkWidget *_gtk_image_new_from_file_scaled(const gchar *file, gint width,
                                           gint height, gboolean keep_ratio);

GtkWidget * fb_button_new_from_file(
    const gchar * image_file, int width, int height, gulong highlight_color, gboolean keep_ratio);
GtkWidget * fb_button_new_from_file_with_label(
    const gchar * image_file, int width, int height, gulong highlight_color, gboolean keep_ratio, Panel * panel, const gchar * label);

void fb_button_set_from_file(GtkWidget* btn, const char* img_file, gint width, gint height, gboolean keep_ratio);

char* translate_exec_to_cmd( const char* exec, const char* icon,
                             const char* title, const char* fpath );

void show_error( GtkWindow* parent_win, const char* msg );

gboolean spawn_command_async(GtkWindow *parent_window, gchar const* workdir,
        gchar const* cmd);

/* Parameters: const char* name, gpointer ret_value, GType type, ....NULL */
GtkWidget* create_generic_config_dlg( const char* title, GtkWidget* parent,
                              GSourceFunc apply_func, Plugin * plugin,
                      const char* name, ... );

extern GtkMenu* lxpanel_get_panel_menu( Panel* panel, Plugin* plugin, gboolean use_sub_menu );

gboolean lxpanel_launch_app(const char* exec, GList* files, gboolean in_terminal, char const* in_workdir);

extern GdkPixbuf* lxpanel_load_icon( const char* name, int width, int height, gboolean use_fallback );


/* Extracted from plugin.h */
struct _Plugin;

/* Support for external plugin versioning.
 * Plugins must invoke PLUGINCLASS_VERSIONING when they instantiate PluginClass. */
#define PLUGINCLASS_VERSION 1
#define PLUGINCLASS_VERSIONING \
    .structure_size = sizeof(PluginClass), \
    .structure_version = PLUGINCLASS_VERSION

/* Representative of an available plugin. */
typedef struct {

    /* Keep these first.  Do not make unnecessary changes in structure layout. */
    unsigned short structure_size;		/* Structure size, for versioning support */
    unsigned short structure_version;		/* Structure version, for versioning support */

    char * fname;				/* Plugin file pathname */
    int count;					/* Reference count */
    GModule * gmodule;				/* Associated GModule structure */

    int dynamic : 1;				/* True if dynamically loaded */
    int unused_invisible : 1;			/* Unused; reserved bit */
    int not_unloadable : 1;			/* Not unloadable due to GModule restriction */
    int one_per_system : 1;			/* Special: only one possible per system, such as system tray */
    int one_per_system_instantiated : 1;	/* True if one instance exists */
    int expand_available : 1;			/* True if "stretch" option is available */
    int expand_default : 1;			/* True if "stretch" option is default */

    /* These fields point within the plugin image. */
    char * type;				/* Internal name of plugin, to match external filename */
    char * name;				/* Display name of plugin for selection UI */
    char * version;				/* Version of plugin */
    char * description;				/* Brief textual description of plugin for selection UI */

    int (*constructor)(struct _Plugin * plugin, char ** fp);		/* Create an instance of the plugin */
    void (*destructor)(struct _Plugin * plugin);			/* Destroy an instance of the plugin */
    void (*config)(struct _Plugin * plugin, GtkWindow * parent);	/* Request the plugin to display its configuration dialog */
    void (*save)(struct _Plugin * plugin, FILE * fp);			/* Request the plugin to save its configuration to a file */
    void (*panel_configuration_changed)(struct _Plugin * plugin);	/* Request the plugin to do a full redraw after a panel configuration change */
} PluginClass;

/* Representative of a loaded and active plugin attached to a panel. */
struct _Plugin {
    PluginClass * klass;			/* Back pointer to PluginClass */
    Panel * panel;				/* Back pointer to Panel */
    GtkWidget * pwid;				/* Top level widget; plugin allocates, but plugin mechanism, not plugin itself, destroys this */
    int expand;					/* Expand ("stretch") setting for container */
    int padding;				/* Padding setting for container */
    int border;					/* Border setting for container */
    gpointer priv;				/* Private context for plugin; plugin frees this in its destructor */
};

#endif
