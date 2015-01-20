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

#ifndef __PANEL_H__
#define __PANEL_H__ 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PANEL_PROP_EDGE              "edge"
#define PANEL_PROP_ALIGNMENT         "alignment"
#define PANEL_PROP_HEIGHT            "height"
#define PANEL_PROP_WIDTH             "width"
#define PANEL_PROP_SIZE_TYPE         "size-type"
#define PANEL_PROP_AUTOHIDE          "auto-hide"
#define PANEL_PROP_AUTOHIDE_SIZE     "auto-hide-size"
#define PANEL_PROP_STRUT             "strut"
#define PANEL_PROP_DOCK              "dock"
#define PANEL_PROP_MONITOR           "monitor"
#define PANEL_PROP_ICON_SIZE         "icon-size"
#define PANEL_PROP_MARGIN            "margin"
#define PANEL_PROP_ENABLE_FONT_SIZE  "enable-font-size"
#define PANEL_PROP_FONT_SIZE         "font-size"
#define PANEL_PROP_ENABLE_FONT_COLOR "enable-font-color"
#define PANEL_PROP_FONT_COLOR        "font-color"
#define PANEL_PROP_BACKGROUND_COLOR  "background-color"
#define PANEL_PROP_BACKGROUND_FILE   "background-file"
#define PANEL_PROP_BACKGROUND_TYPE   "background-type"

#define LX_TYPE_PANEL                  (lxpanel_get_type())
#define LXPANEL(obj)                   (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                        LX_TYPE_PANEL, SimplePanel))
#define LXPANEL_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST((klass), \
                                        LX_TYPE_PANEL, SimplePanelClass))
#define LX_IS_PANEL(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                        LX_TYPE_PANEL))

extern GType lxpanel_get_type          (void) G_GNUC_CONST;

/* A little trick to be compatible with some themes which rely on the
   PanelToplevel class, so we use SimplePanel as alias for PanelToplevel */
typedef struct _SimplePanel SimplePanel;
typedef struct _SimplePanel PanelWindow;
typedef struct _SimplePanelClass PanelWindowClass;

typedef struct _Panel Panel;

struct _SimplePanel
{
    GtkApplicationWindow window;
    Panel *priv;
};

struct _SimplePanelClass
{
    GtkApplicationWindowClass parent_class;
};

/**
 * panel_apply_icon
 * @w: a window to apply
 *
 * Sets appropriate icon as the window icon for @w.
 */
extern void panel_apply_icon(GtkWindow *w);

/**
 * simple_panel_draw_label_text
 * @p: a panel instance
 * @label: a label widget
 * @text: (allow-none): text for the label
 * @bold: %TRUE if text should be bold
 * @custom_size_factor: scale factor for font size
 *
 * Changes @label to contain @text with appropriate attributes using the
 * panel @p settings.
 */
extern void simple_panel_draw_label_text(GtkWidget * label, const char * text,
                                    gboolean bold, float custom_size_factor);

#define PANEL_ORIENT_HORIZONTAL(orient) \
    ((orient == GTK_POS_TOP) || (orient == GTK_POS_BOTTOM))

#define PANEL_ORIENT_VERTICAL(orient) \
    ((orient == GTK_POS_LEFT) || (orient == GTK_POS_RIGHT))

#define panel_get_orientation_from_edge(edge) \
    ((edge == GTK_POS_LEFT) || (edge == GTK_POS_RIGHT)) \
    ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL

/* Accessors APIs for Panel* */
extern GtkOrientation panel_get_orientation(SimplePanel *panel);
extern gint panel_get_icon_size(SimplePanel *panel);
extern gint panel_get_height(SimplePanel *panel);
extern gint panel_get_monitor(SimplePanel *panel);
extern GtkIconTheme *panel_get_icon_theme(SimplePanel *panel);
extern gboolean panel_is_dynamic(SimplePanel *panel);
extern GtkWidget *panel_box_new(SimplePanel *panel, gint spacing);
extern GtkWidget *panel_separator_new(SimplePanel *panel);
G_END_DECLS

#endif
