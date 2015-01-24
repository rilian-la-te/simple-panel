/*
 * Copyright (C) 2008-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include <gtk/gtk.h>
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <X11/extensions/shape.h>
#endif

#include "tasklist-widget.h"



#define DEFAULT_BUTTON_SIZE          (25)
#define DEFAULT_MAX_BUTTON_LENGTH    (200)
#define DEFAULT_MENU_ICON_SIZE       (16)
#define DEFAULT_MIN_BUTTON_LENGTH    (DEFAULT_MAX_BUTTON_LENGTH / 4)
#define DEFAULT_ICON_LUCENCY         (50)
#define DEFAULT_ELLIPSIZE_MODE       (PANGO_ELLIPSIZE_END)
#define DEFAULT_MENU_MAX_WIDTH_CHARS (24)
#define ARROW_BUTTON_SIZE            (20)
#define WIREFRAME_SIZE               (5) /* same as xfwm4 */
#define DRAG_ACTIVATE_TIMEOUT        (500)



/* locking helpers for tasklist->locked */
#define xfce_taskbar_lock(tasklist)      G_STMT_START { XFCE_TASKLIST (tasklist)->locked++; } G_STMT_END
#define xfce_taskbar_unlock(tasklist)    G_STMT_START { \
                                           if (XFCE_TASKLIST (tasklist)->locked > 0) \
                                             XFCE_TASKLIST (tasklist)->locked--; \
                                           else \
                                             panel_assert_not_reached (); \
                                         } G_STMT_END
#define xfce_taskbar_is_locked(tasklist) (XFCE_TASKLIST (tasklist)->locked > 0)

#define xfce_tasklist_get_panel_plugin(tasklist) gtk_widget_get_ancestor (GTK_WIDGET (tasklist), XFCE_TYPE_PANEL_PLUGIN)
#define xfce_tasklist_horizontal(tasklist) ((tasklist)->mode == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL)
#define xfce_tasklist_deskbar(tasklist) ((tasklist)->mode == XFCE_PANEL_PLUGIN_MODE_DESKBAR)
#define xfce_tasklist_filter_monitors(tasklist) (!(tasklist)->all_monitors && (tasklist)->n_monitors > 1)
#define xfce_tasklist_geometry_set_invalid(tasklist) ((tasklist)->n_monitors = 0)



enum
{
  PROP_0,
  PROP_GROUPING,
  PROP_INCLUDE_ALL_WORKSPACES,
  PROP_INCLUDE_ALL_MONITORS,
  PROP_FLAT_BUTTONS,
  PROP_SWITCH_WORKSPACE_ON_UNMINIMIZE,
  PROP_SHOW_LABELS,
  PROP_SHOW_ONLY_MINIMIZED,
  PROP_SHOW_WIREFRAMES,
  PROP_SHOW_HANDLE,
  PROP_SORT_ORDER,
  PROP_WINDOW_SCROLLING,
  PROP_INCLUDE_ALL_BLINKING,
  PROP_MIDDLE_CLICK
};

struct _XfceTasklistClass
{
  GtkContainerClass __parent__;
};

struct _XfceTasklist
{
  GtkContainer __parent__;

  /* lock counter */
  gint                  locked;

  /* the screen of this tasklist */
  WnckScreen           *screen;
  GdkScreen            *gdk_screen;

  /* window children in the tasklist */
  GList                *windows;

  /* windows we monitor, but that are excluded from the tasklist */
  GSList               *skipped_windows;

  /* arrow button of the overflow menu */
  GtkWidget            *arrow_button;

  /* classgroups of all the windows in the taskbar */
  GHashTable           *class_groups;

  /* normal or iconbox style */
  guint                 show_labels : 1;

  /* size of the panel pluin */
  gint                  size;

  /* mode (orientation) of the tasklist */
  XfcePanelPluginMode   mode;

  /* relief of the tasklist buttons */
  GtkReliefStyle        button_relief;

  /* whether we show windows from all workspaces or
   * only the active workspace */
  guint                 all_workspaces : 1;

  /* whether we switch to another workspace when we try to
   * unminimize a window on another workspace */
  guint                 switch_workspace : 1;

  /* whether we only show monimized windows in the
   * tasklist */
  guint                 only_minimized : 1;

  /* number of rows of window buttons */
  gint                  nrows;

  /* switch window with the mouse wheel */
  guint                 window_scrolling : 1;

  /* whether we show blinking windows from all workspaces
   * or only the active workspace */
  guint                 all_blinking : 1;

  /* action to preform when middle clicking */
  XfceTasklistMClick    middle_click;

  /* whether we only show windows that are in the geometry of
   * the monitor the tasklist is on */
  guint                 all_monitors : 1;
  guint                 n_monitors;
  guint                 my_monitor;
  GdkRectangle         *all_monitors_geometry;

  /* whether we show wireframes when hovering a button in
   * the tasklist */
  guint                 show_wireframes : 1;

  /* icon geometries update timeout */
  guint                 update_icon_geometries_id;

  /* idle monitor geometry update */
  guint                 update_monitor_geometry_id;

  /* button grouping mode */
  XfceTasklistGrouping  grouping;

  /* sorting order of the buttons */
  XfceTasklistSortOrder sort_order;

  /* dummy properties */
  guint                 show_handle : 1;

#ifdef GDK_WINDOWING_X11
  /* wireframe window */
  Window                wireframe_window;
#endif

  /* gtk style properties */
  gint                  max_button_length;
  gint                  min_button_length;
  gint                  max_button_size;
  PangoEllipsizeMode    ellipsize_mode;
  gint                  minimized_icon_lucency;
  gint                  menu_icon_size;
  gint                  menu_max_width_chars;

  gint n_windows;
};

typedef enum
{
  CHILD_TYPE_WINDOW,
  CHILD_TYPE_GROUP,
  CHILD_TYPE_OVERFLOW_MENU,
  CHILD_TYPE_GROUP_MENU
}
XfceTasklistChildType;

typedef struct _XfceTasklistChild XfceTasklistChild;
struct _XfceTasklistChild
{
  /* type of this button */
  XfceTasklistChildType  type;

  /* pointer to the tasklist */
  XfceTasklist           *tasklist;

  /* button widgets */
  GtkWidget              *button;
  GtkWidget              *box;
  GtkWidget              *icon;
  GtkWidget              *label;

  /* drag motion window activate */
  guint                   motion_timeout_id;
  guint                   motion_timestamp;

  /* unique id for sorting by insert time,
   * simply increased for each new button */
  guint                   unique_id;

  /* last time this window was focused */
  GTimeVal                last_focused;

  /* list of windows in case of a group button */
  GSList                 *windows;

  /* wnck information */
  WnckWindow             *window;
  WnckClassGroup         *class_group;
};

static const GtkTargetEntry source_targets[] =
{
  { "application/x-wnck-window-id", 0, 0 }
};



static void               xfce_tasklist_get_property                     (GObject              *object,
                                                                          guint                 prop_id,
                                                                          GValue               *value,
                                                                          GParamSpec           *pspec);
static void               xfce_tasklist_set_property                     (GObject              *object,
                                                                          guint                 prop_id,
                                                                          const GValue         *value,
                                                                          GParamSpec           *pspec);
static void               xfce_tasklist_finalize                         (GObject              *object);
static void               xfce_tasklist_size_request                     (GtkWidget            *widget,
                                                                          GtkRequisition       *requisition);
static void               xfce_tasklist_size_allocate                    (GtkWidget            *widget,
                                                                          GtkAllocation        *allocation);
static void               xfce_tasklist_style_set                        (GtkWidget            *widget,
                                                                          GtkStyle             *previous_style);
static void               xfce_tasklist_realize                          (GtkWidget            *widget);
static void               xfce_tasklist_unrealize                        (GtkWidget            *widget);
static gboolean           xfce_tasklist_scroll_event                     (GtkWidget            *widget,
                                                                          GdkEventScroll       *event);
static void               xfce_tasklist_remove                           (GtkContainer         *container,
                                                                          GtkWidget            *widget);
static void               xfce_tasklist_forall                           (GtkContainer         *container,
                                                                          gboolean              include_internals,
                                                                          GtkCallback           callback,
                                                                          gpointer              callback_data);
static GType              xfce_tasklist_child_type                       (GtkContainer         *container);
static void               xfce_tasklist_arrow_button_toggled             (GtkWidget            *button,
                                                                          XfceTasklist         *tasklist);
static void               xfce_tasklist_connect_screen                   (XfceTasklist         *tasklist);
static void               xfce_tasklist_disconnect_screen                (XfceTasklist         *tasklist);
static void               xfce_tasklist_gdk_screen_changed               (GdkScreen            *gdk_screen,
                                                                          XfceTasklist         *tasklist);
static void               xfce_tasklist_active_window_changed            (WnckScreen           *screen,
                                                                          WnckWindow           *previous_window,
                                                                          XfceTasklist         *tasklist);
static void               xfce_tasklist_active_workspace_changed         (WnckScreen           *screen,
                                                                          WnckWorkspace        *previous_workspace,
                                                                          XfceTasklist         *tasklist);
static void               xfce_tasklist_window_added                     (WnckScreen           *screen,
                                                                          WnckWindow           *window,
                                                                          XfceTasklist         *tasklist);
static void               xfce_tasklist_window_removed                   (WnckScreen           *screen,
                                                                          WnckWindow           *window,
                                                                          XfceTasklist         *tasklist);
static void               xfce_tasklist_viewports_changed                (WnckScreen           *screen,
                                                                          XfceTasklist         *tasklist);
static void               xfce_tasklist_skipped_windows_state_changed    (WnckWindow           *window,
                                                                          WnckWindowState       changed_state,
                                                                          WnckWindowState       new_state,
                                                                          XfceTasklist         *tasklist);
static void               xfce_tasklist_sort                             (XfceTasklist         *tasklist);
static gboolean           xfce_tasklist_update_icon_geometries           (gpointer              data);
static void               xfce_tasklist_update_icon_geometries_destroyed (gpointer              data);

/* wireframe */
#ifdef GDK_WINDOWING_X11
static void               xfce_tasklist_wireframe_hide                   (XfceTasklist         *tasklist);
static void               xfce_tasklist_wireframe_destroy                (XfceTasklist         *tasklist);
static void               xfce_tasklist_wireframe_update                 (XfceTasklist         *tasklist,
                                                                          XfceTasklistChild    *child);
#endif

/* tasklist buttons */
static inline gboolean    xfce_tasklist_button_visible                   (XfceTasklistChild    *child,
                                                                          WnckWorkspace         *active_ws);
static gint               xfce_tasklist_button_compare                   (gconstpointer         child_a,
                                                                          gconstpointer         child_b,
                                                                          gpointer              user_data);
static GtkWidget         *xfce_tasklist_button_proxy_menu_item           (XfceTasklistChild    *child,
                                                                          gboolean              allow_wireframe);
static void               xfce_tasklist_button_activate                  (XfceTasklistChild    *child,
                                                                          guint32               timestamp);
static XfceTasklistChild *xfce_tasklist_button_new                       (WnckWindow           *window,
                                                                          XfceTasklist         *tasklist);

/* tasklist group buttons */
static void               xfce_tasklist_group_button_remove              (XfceTasklistChild    *group_child);
static void               xfce_tasklist_group_button_add_window          (XfceTasklistChild    *group_child,
                                                                          XfceTasklistChild    *window_child);
static XfceTasklistChild *xfce_tasklist_group_button_new                 (WnckClassGroup       *class_group,
                                                                          XfceTasklist         *tasklist);

/* potential public functions */
static void               xfce_tasklist_set_include_all_workspaces       (XfceTasklist         *tasklist,
                                                                          gboolean              all_workspaces);
static void               xfce_tasklist_set_include_all_monitors         (XfceTasklist         *tasklist,
                                                                          gboolean              all_monitors);
static void               xfce_tasklist_set_button_relief                (XfceTasklist         *tasklist,
                                                                          GtkReliefStyle        button_relief);
static void               xfce_tasklist_set_show_labels                  (XfceTasklist         *tasklist,
                                                                          gboolean              show_labels);
static void               xfce_tasklist_set_show_only_minimized          (XfceTasklist         *tasklist,
                                                                          gboolean              only_minimized);
static void               xfce_tasklist_set_show_wireframes              (XfceTasklist         *tasklist,
                                                                          gboolean              show_wireframes);
static void               xfce_tasklist_set_grouping                     (XfceTasklist         *tasklist,
                                                                          XfceTasklistGrouping  grouping);


G_DEFINE_TYPE (XfceTasklist, xfce_tasklist, GTK_TYPE_CONTAINER)



static GtkIconSize menu_icon_size = GTK_ICON_SIZE_INVALID;



static void
xfce_tasklist_class_init (XfceTasklistClass *klass)
{
  GObjectClass      *gobject_class;
  GtkWidgetClass    *gtkwidget_class;
  GtkContainerClass *gtkcontainer_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = xfce_tasklist_get_property;
  gobject_class->set_property = xfce_tasklist_set_property;
  gobject_class->finalize = xfce_tasklist_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->size_request = xfce_tasklist_size_request;
  gtkwidget_class->size_allocate = xfce_tasklist_size_allocate;
  gtkwidget_class->style_set = xfce_tasklist_style_set;
  gtkwidget_class->realize = xfce_tasklist_realize;
  gtkwidget_class->unrealize = xfce_tasklist_unrealize;
  gtkwidget_class->scroll_event = xfce_tasklist_scroll_event;

  gtkcontainer_class = GTK_CONTAINER_CLASS (klass);
  gtkcontainer_class->add = NULL;
  gtkcontainer_class->remove = xfce_tasklist_remove;
  gtkcontainer_class->forall = xfce_tasklist_forall;
  gtkcontainer_class->child_type = xfce_tasklist_child_type;

  g_object_class_install_property (gobject_class,
                                   PROP_GROUPING,
                                   g_param_spec_uint ("grouping",
                                                      NULL, NULL,
                                                      XFCE_TASKLIST_GROUPING_MIN,
                                                      XFCE_TASKLIST_GROUPING_MAX + 1 /* TODO drop this later */,
                                                      XFCE_TASKLIST_GROUPING_DEFAULT,
                                                      EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_INCLUDE_ALL_WORKSPACES,
                                   g_param_spec_boolean ("include-all-workspaces",
                                                         NULL, NULL,
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_INCLUDE_ALL_MONITORS,
                                   g_param_spec_boolean ("include-all-monitors",
                                                         NULL, NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_FLAT_BUTTONS,
                                   g_param_spec_boolean ("flat-buttons",
                                                         NULL, NULL,
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SWITCH_WORKSPACE_ON_UNMINIMIZE,
                                   g_param_spec_boolean ("switch-workspace-on-unminimize",
                                                         NULL, NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_LABELS,
                                   g_param_spec_boolean ("show-labels",
                                                         NULL, NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_ONLY_MINIMIZED,
                                   g_param_spec_boolean ("show-only-minimized",
                                                         NULL, NULL,
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_WIREFRAMES,
                                   g_param_spec_boolean ("show-wireframes",
                                                         NULL, NULL,
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_HANDLE,
                                   g_param_spec_boolean ("show-handle",
                                                         NULL, NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SORT_ORDER,
                                   g_param_spec_uint ("sort-order",
                                                      NULL, NULL,
                                                      XFCE_TASKLIST_SORT_ORDER_MIN,
                                                      XFCE_TASKLIST_SORT_ORDER_MAX,
                                                      XFCE_TASKLIST_SORT_ORDER_DEFAULT,
                                                      EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_WINDOW_SCROLLING,
                                   g_param_spec_boolean ("window-scrolling",
                                                         NULL, NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_INCLUDE_ALL_BLINKING,
                                   g_param_spec_boolean ("include-all-blinking",
                                                         NULL, NULL,
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_MIDDLE_CLICK,
                                   g_param_spec_uint ("middle-click",
                                                      NULL, NULL,
                                                      XFCE_TASKLIST_MIDDLE_CLICK_MIN,
                                                      XFCE_TASKLIST_MIDDLE_CLICK_MAX,
                                                      XFCE_TASKLIST_MIDDLE_CLICK_DEFAULT,
                                                      EXO_PARAM_READWRITE));

  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("max-button-length",
                                                             NULL,
                                                             "The maximum length of a window button",
                                                             -1, G_MAXINT,
                                                             DEFAULT_MAX_BUTTON_LENGTH,
                                                             EXO_PARAM_READABLE));

  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("min-button-length",
                                                             NULL,
                                                             "The minumum length of a window button",
                                                             1, G_MAXINT,
                                                             DEFAULT_MIN_BUTTON_LENGTH,
                                                             EXO_PARAM_READABLE));

  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("max-button-size",
                                                             NULL,
                                                             "The maximum size of a window button",
                                                             1, G_MAXINT,
                                                             DEFAULT_BUTTON_SIZE,
                                                             EXO_PARAM_READABLE));

  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_enum ("ellipsize-mode",
                                                              NULL,
                                                              "The ellipsize mode used for the button label",
                                                              PANGO_TYPE_ELLIPSIZE_MODE,
                                                              DEFAULT_ELLIPSIZE_MODE,
                                                              EXO_PARAM_READABLE));

  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("minimized-icon-lucency",
                                                             NULL,
                                                             "Lucent percentage of minimized icons",
                                                             0, 100,
                                                             DEFAULT_ICON_LUCENCY,
                                                             EXO_PARAM_READABLE));
  gtk_widget_class_install_style_property (gtkwidget_class,
                                           g_param_spec_int ("menu-max-width-chars",
                                                             NULL,
                                                             "Maximum chars in the overflow menu labels",
                                                             0, G_MAXINT,
                                                             DEFAULT_MENU_MAX_WIDTH_CHARS,
                                                             EXO_PARAM_READABLE));

  menu_icon_size = gtk_icon_size_from_name ("panel-tasklist-menu");
  if (menu_icon_size == GTK_ICON_SIZE_INVALID)
    menu_icon_size = gtk_icon_size_register ("panel-tasklist-menu",
                                             DEFAULT_MENU_ICON_SIZE,
                                             DEFAULT_MENU_ICON_SIZE);
}



static void
xfce_tasklist_init (XfceTasklist *tasklist)
{
  GTK_WIDGET_SET_FLAGS (tasklist, GTK_NO_WINDOW);

  tasklist->locked = 0;
  tasklist->screen = NULL;
  tasklist->windows = NULL;
  tasklist->skipped_windows = NULL;
  tasklist->mode = XFCE_PANEL_PLUGIN_MODE_HORIZONTAL;
  tasklist->nrows = 1;
  tasklist->all_workspaces = FALSE;
  tasklist->button_relief = GTK_RELIEF_NORMAL;
  tasklist->switch_workspace = TRUE;
  tasklist->only_minimized = FALSE;
  tasklist->show_labels = TRUE;
  tasklist->show_wireframes = FALSE;
  tasklist->show_handle = TRUE;
  tasklist->all_monitors = TRUE;
  tasklist->n_monitors = 0;
  tasklist->all_monitors_geometry = NULL;
  tasklist->window_scrolling = TRUE;
  tasklist->all_blinking = TRUE;
  tasklist->middle_click = XFCE_TASKLIST_MIDDLE_CLICK_DEFAULT;
  xfce_tasklist_geometry_set_invalid (tasklist);
#ifdef GDK_WINDOWING_X11
  tasklist->wireframe_window = 0;
#endif
  tasklist->update_icon_geometries_id = 0;
  tasklist->update_monitor_geometry_id = 0;
  tasklist->max_button_length = DEFAULT_MAX_BUTTON_LENGTH;
  tasklist->min_button_length = DEFAULT_MIN_BUTTON_LENGTH;
  tasklist->max_button_size = DEFAULT_BUTTON_SIZE;
  tasklist->minimized_icon_lucency = DEFAULT_ICON_LUCENCY;
  tasklist->ellipsize_mode = DEFAULT_ELLIPSIZE_MODE;
  tasklist->grouping = XFCE_TASKLIST_GROUPING_DEFAULT;
  tasklist->sort_order = XFCE_TASKLIST_SORT_ORDER_DEFAULT;
  tasklist->menu_icon_size = DEFAULT_MENU_ICON_SIZE;
  tasklist->menu_max_width_chars = DEFAULT_MENU_MAX_WIDTH_CHARS;
  tasklist->class_groups = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                  (GDestroyNotify) g_object_unref,
                                                  (GDestroyNotify) xfce_tasklist_group_button_remove);

  /* widgets for the overflow menu */
  /* TODO support drag-motion and drag-leave */
  tasklist->arrow_button = xfce_arrow_button_new (GTK_ARROW_DOWN);
  gtk_widget_set_parent (tasklist->arrow_button, GTK_WIDGET (tasklist));
  gtk_widget_set_name (tasklist->arrow_button, "panel-tasklist-arrow");
  gtk_button_set_relief (GTK_BUTTON (tasklist->arrow_button), tasklist->button_relief);
  g_signal_connect (G_OBJECT (tasklist->arrow_button), "toggled",
      G_CALLBACK (xfce_tasklist_arrow_button_toggled), tasklist);
  gtk_widget_show (tasklist->arrow_button);
}



static void
xfce_tasklist_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  XfceTasklist *tasklist = XFCE_TASKLIST (object);

  switch (prop_id)
    {
    case PROP_GROUPING:
      g_value_set_uint (value, tasklist->grouping);
      break;

    case PROP_INCLUDE_ALL_WORKSPACES:
      g_value_set_boolean (value, tasklist->all_workspaces);
      break;

    case PROP_INCLUDE_ALL_MONITORS:
      g_value_set_boolean (value, tasklist->all_monitors);
      break;

    case PROP_FLAT_BUTTONS:
      g_value_set_boolean (value, !!(tasklist->button_relief == GTK_RELIEF_NONE));
      break;

    case PROP_SWITCH_WORKSPACE_ON_UNMINIMIZE:
      g_value_set_boolean (value, tasklist->switch_workspace);
      break;

    case PROP_SHOW_LABELS:
      g_value_set_boolean (value, tasklist->show_labels);
      break;

    case PROP_SHOW_ONLY_MINIMIZED:
      g_value_set_boolean (value, tasklist->only_minimized);
      break;

    case PROP_SHOW_WIREFRAMES:
      g_value_set_boolean (value, tasklist->show_wireframes);
      break;

    case PROP_SHOW_HANDLE:
      g_value_set_boolean (value, tasklist->show_handle);
      break;

    case PROP_SORT_ORDER:
      g_value_set_uint (value, tasklist->sort_order);
      break;

    case PROP_WINDOW_SCROLLING:
      g_value_set_boolean (value, tasklist->window_scrolling);
      break;

    case PROP_INCLUDE_ALL_BLINKING:
      g_value_set_boolean (value, tasklist->all_blinking);
      break;

    case PROP_MIDDLE_CLICK:
      g_value_set_uint (value, tasklist->middle_click);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_tasklist_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  XfceTasklist          *tasklist = XFCE_TASKLIST (object);
  XfceTasklistSortOrder  sort_order;

  switch (prop_id)
    {
    case PROP_GROUPING:
      xfce_tasklist_set_grouping (tasklist, g_value_get_uint (value));
      break;

    case PROP_INCLUDE_ALL_WORKSPACES:
      xfce_tasklist_set_include_all_workspaces (tasklist, g_value_get_boolean (value));
      break;

    case PROP_INCLUDE_ALL_MONITORS:
      xfce_tasklist_set_include_all_monitors (tasklist, g_value_get_boolean (value));
      break;

    case PROP_FLAT_BUTTONS:
      xfce_tasklist_set_button_relief (tasklist,
                                       g_value_get_boolean (value) ?
                                         GTK_RELIEF_NONE : GTK_RELIEF_NORMAL);
      break;

    case PROP_SHOW_LABELS:
      xfce_tasklist_set_show_labels (tasklist, g_value_get_boolean (value));
      break;

    case PROP_SWITCH_WORKSPACE_ON_UNMINIMIZE:
      tasklist->switch_workspace = g_value_get_boolean (value);
      break;

    case PROP_SHOW_ONLY_MINIMIZED:
      xfce_tasklist_set_show_only_minimized (tasklist, g_value_get_boolean (value));
      break;

    case PROP_SHOW_WIREFRAMES:
      xfce_tasklist_set_show_wireframes (tasklist, g_value_get_boolean (value));
      break;

    case PROP_SHOW_HANDLE:
      tasklist->show_handle = g_value_get_boolean (value);
      break;

    case PROP_SORT_ORDER:
      sort_order = g_value_get_uint (value);
      if (tasklist->sort_order != sort_order)
        {
          tasklist->sort_order = sort_order;
          xfce_tasklist_sort (tasklist);
        }
      break;

    case PROP_WINDOW_SCROLLING:
      tasklist->window_scrolling = g_value_get_boolean (value);
      break;

    case PROP_INCLUDE_ALL_BLINKING:
      tasklist->all_blinking = g_value_get_boolean (value);
      break;

    case PROP_MIDDLE_CLICK:
      tasklist->middle_click= g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_tasklist_finalize (GObject *object)
{
  XfceTasklist *tasklist = XFCE_TASKLIST (object);

  /* data that should already be freed when disconnecting the screen */
  panel_return_if_fail (tasklist->windows == NULL);
  panel_return_if_fail (tasklist->skipped_windows == NULL);
  panel_return_if_fail (tasklist->screen == NULL);

  /* stop pending timeouts */
  if (tasklist->update_icon_geometries_id != 0)
    g_source_remove (tasklist->update_icon_geometries_id);
  if (tasklist->update_monitor_geometry_id != 0)
    g_source_remove (tasklist->update_monitor_geometry_id);

  /* free the class group hash table */
  g_hash_table_destroy (tasklist->class_groups);

#ifdef GDK_WINDOWING_X11
  /* destroy the wireframe window */
  xfce_tasklist_wireframe_destroy (tasklist);
#endif

  (*G_OBJECT_CLASS (xfce_tasklist_parent_class)->finalize) (object);
}



static void
xfce_tasklist_size_request (GtkWidget      *widget,
                            GtkRequisition *requisition)
{
  XfceTasklist      *tasklist = XFCE_TASKLIST (widget);
  gint               rows, cols;
  gint               n_windows;
  GtkRequisition     child_req;
  gint               length;
  GList             *li;
  XfceTasklistChild *child;
  gint               child_height = 0;

  for (li = tasklist->windows, n_windows = 0; li != NULL; li = li->next)
    {
      child = li->data;

      if (GTK_WIDGET_VISIBLE (child->button))
        {
          gtk_widget_size_request (child->button, &child_req);

          /* child_height = MAX (child_height, child_req.height); */
          child_height = MAX (child_height, tasklist->size / tasklist->nrows);

          if (child->type == CHILD_TYPE_GROUP_MENU)
            continue;

          n_windows++;
        }
    }

  tasklist->n_windows = n_windows;

  if (n_windows == 0)
    {
      length = 0;
    }
  else
    {
      rows = MAX (tasklist->nrows, 1);
      if (tasklist->show_labels && tasklist->max_button_size > 0)
        {
          rows = MAX (rows, tasklist->size / tasklist->max_button_size);
          child_height = MIN (child_height, tasklist->max_button_size);
        }

      cols = n_windows / rows;
      if (cols * rows < n_windows)
        cols++;

      if (!tasklist->show_labels)
        length = (tasklist->size / rows) * cols;
      else if (tasklist->max_button_length != -1)
        length = cols * tasklist->max_button_length;
      else
        length = cols * DEFAULT_MAX_BUTTON_LENGTH;
    }

  /* set the requested sizes */
  if (xfce_tasklist_deskbar (tasklist) && tasklist->show_labels)
    {
      requisition->height = child_height * n_windows;
      requisition->width = tasklist->size;
    }
  else if (xfce_tasklist_horizontal (tasklist))
    {
      requisition->width = length;
      requisition->height = tasklist->size;
    }
  else
    {
      requisition->width = tasklist->size;
      requisition->height = length;
    }
}



static gint
xfce_tasklist_size_sort_window (gconstpointer a,
                                gconstpointer b)
{
  const XfceTasklistChild *child_a = a;
  const XfceTasklistChild *child_b = b;
  glong                    diff;

  diff = child_a->last_focused.tv_sec - child_b->last_focused.tv_sec;
  if (diff != 0)
    return CLAMP (diff, -1, 1);

  diff = child_a->last_focused.tv_usec - child_b->last_focused.tv_usec;
  return CLAMP (diff, -1, 1);
}



static void
xfce_tasklist_size_layout (XfceTasklist  *tasklist,
                           GtkAllocation *alloc,
                           gint          *n_rows,
                           gint          *n_cols,
                           gint          *arrow_position)
{
  gint               rows;
  gint               min_button_length;
  gint               cols;
  GSList            *windows_scored = NULL, *lp;
  GList             *li;
  XfceTasklistChild *child;
  gint               max_button_length;
  gint               n_buttons;
  gint               n_buttons_target;

  /* if we're in deskbar mode, there are no columns */
  if (xfce_tasklist_deskbar (tasklist) && tasklist->show_labels)
    rows = 1;
  else if (tasklist->show_labels && tasklist->max_button_size > 0)
    rows = MAX (tasklist->nrows, tasklist->size / tasklist->max_button_size);
  else
    rows = tasklist->nrows;

  if (rows < 1)
    rows = 1;

  cols = tasklist->n_windows / rows;
  if (cols * rows < tasklist->n_windows)
    cols++;

  if (xfce_tasklist_deskbar (tasklist) && tasklist->show_labels)
    min_button_length = MIN (alloc->height / tasklist->nrows, tasklist->max_button_size);
  else if (!tasklist->show_labels)
    min_button_length = alloc->height / tasklist->nrows;
  else
    min_button_length = tasklist->min_button_length;

  *arrow_position = -1; /* not visible */

  /* unset overflow items, we decide about that again
   * later */
  for (li = tasklist->windows; li != NULL; li = li->next)
    {
      child = li->data;
      if (child->type == CHILD_TYPE_OVERFLOW_MENU)
        child->type = CHILD_TYPE_WINDOW;
    }

  if (min_button_length * cols <= alloc->width)
    {
      /* all the windows seem to fit */
      *n_rows = rows;
      *n_cols = cols;
    }
  else
    {
      /* we need to group something, first create a list with the
       * windows most suitable for grouping at the beginning that are
       * (should be) currently visible */
      for (li = tasklist->windows; li != NULL; li = li->next)
        {
          child = li->data;
          if (GTK_WIDGET_VISIBLE (child->button))
            {
              windows_scored = g_slist_insert_sorted (windows_scored, child,
                                                      xfce_tasklist_size_sort_window);
            }
        }

      if (xfce_tasklist_deskbar (tasklist) || !tasklist->show_labels)
        max_button_length = min_button_length;
      else if (tasklist->max_button_length != -1)
        max_button_length = tasklist->max_button_length;
      else
        max_button_length = DEFAULT_MAX_BUTTON_LENGTH;

      n_buttons = tasklist->n_windows;
      /* Matches the existing behavior (with a bug fix) */
      /* n_buttons_target = MIN ((alloc->width - ARROW_BUTTON_SIZE) / min_button_length * rows,          *
       *                         (((alloc->width - ARROW_BUTTON_SIZE) / max_button_length) + 1) * rows); */

      /* Perhaps a better behavior (tries to display more buttons on the panel, */
      /* yet still within the specified limits) */
      n_buttons_target = (alloc->width - ARROW_BUTTON_SIZE) / min_button_length * rows;

#if 0
      if (tasklist->grouping == XFCE_TASKLIST_GROUPING_AUTO)
        {
          /* try creating group buttons */
        }
#endif

      /* we now push the windows with the lowest score in the
       * overflow menu */
      if (n_buttons > n_buttons_target)
        {
          panel_debug (PANEL_DEBUG_TASKLIST,
                       "Putting %d windows in overflow menu",
                       n_buttons - n_buttons_target);

          for (lp = windows_scored;
               n_buttons > n_buttons_target && lp != NULL;
               lp = lp->next, n_buttons--)
            {
              child = lp->data;

              if (child->type == CHILD_TYPE_WINDOW)
                child->type = CHILD_TYPE_OVERFLOW_MENU;
            }

          /* Try to position the arrow widget at the end of the allocation area  *
           * if that's impossible (because buttons cannot be expanded enough)    *
           * position it just after the buttons.                                 */
          *arrow_position = MIN (alloc->width - ARROW_BUTTON_SIZE,
                                 n_buttons_target * max_button_length / rows);
        }

      g_slist_free (windows_scored);

      cols = n_buttons / rows;
      if (cols * rows < n_buttons)
        cols++;

      *n_rows = rows;
      *n_cols = cols;
    }
}



static void
xfce_tasklist_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
  XfceTasklist      *tasklist = XFCE_TASKLIST (widget);
  gint               rows, cols;
  gint               row;
  GtkAllocation      area = *allocation;
  GList             *li;
  XfceTasklistChild *child;
  gint               i;
  GtkAllocation      child_alloc;
  gboolean           direction_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  gint               w, x, y, h;
  gint               area_x, area_width;
  gint               arrow_position;
  GtkRequisition     child_req;

  panel_return_if_fail (GTK_WIDGET_VISIBLE (tasklist->arrow_button));

  /* set widget allocation */
  widget->allocation = *allocation;

  /* swap integers with vertical orientation */
  if (!xfce_tasklist_horizontal (tasklist))
    TRANSPOSE_AREA (area);
  panel_return_if_fail (area.height == tasklist->size);

  /* TODO if we compare the allocation with the requisition we can
   * do a fast path to the child allocation, i think */

  /* useless but hides compiler warning */
  w = h = x = y = rows = cols = 0;

  xfce_tasklist_size_layout (tasklist, &area, &rows, &cols, &arrow_position);

  /* allocate the arrow button for the overflow menu */
  child_alloc.width = ARROW_BUTTON_SIZE;
  child_alloc.height = area.height;

  if (arrow_position != -1)
    {
      child_alloc.x = area.x;
      child_alloc.y = area.y;

      if (!direction_rtl)
        child_alloc.x += arrow_position;
      else
        child_alloc.x += (area.width - arrow_position);

      area.width = arrow_position;

      /* position the arrow in the correct position */
      if (!xfce_tasklist_horizontal (tasklist))
        TRANSPOSE_AREA (child_alloc);
    }
  else
    {
      child_alloc.x = child_alloc.y = -9999;
    }

  gtk_widget_size_allocate (tasklist->arrow_button, &child_alloc);

  area_x = area.x;
  area_width = area.width;
  h = area.height / rows;

  /* allocate all the children */
  for (li = tasklist->windows, i = 0; li != NULL; li = li->next)
    {
      child = li->data;

      /* skip hidden buttons */
      if (!GTK_WIDGET_VISIBLE (child->button))
        continue;

      if (G_LIKELY (child->type == CHILD_TYPE_WINDOW
          || child->type == CHILD_TYPE_GROUP))
        {
          row = (i % rows);
          if (row == 0)
            {
              x = area_x;
              y = area.y;

              if (xfce_tasklist_deskbar (tasklist) && tasklist->show_labels)
                {
                  /* fixed width is OK because area.width==w*cols */
                  w = MIN (area.height / tasklist->nrows, tasklist->max_button_size);
                }
              else if (tasklist->show_labels)
                {
                  /* TODO, this is a work-around, something else goes wrong
                   * with counting the windows... */
                  if (cols < 1)
                    cols = 1;
                  w = area_width / cols--;
                  if (tasklist->max_button_length > 0
                      && w > tasklist->max_button_length)
                    w = tasklist->max_button_length;
                }
              else /* buttons without labels */
                {
                  w = h;
                }

              area_width -= w;
              area_x += w;
            }

          child_alloc.y = y;
          child_alloc.x = x;
          child_alloc.width = MAX (w, 1); /* TODO this is a workaround */
          child_alloc.height = h;

          y += h;

          if (direction_rtl)
            child_alloc.x = area.x + area.width - (child_alloc.x - area.x) - child_alloc.width;

          /* allocate the child */
          if (!xfce_tasklist_horizontal (tasklist))
            TRANSPOSE_AREA (child_alloc);

          /* increase the position counter */
          i++;
        }
      else
        {
          gtk_widget_get_child_requisition (child->button, &child_req);

          /* move the button offscreen */
          child_alloc.y = child_alloc.x = -9999;
          child_alloc.width = child_req.width;
          child_alloc.height = child_req.height;
        }

      gtk_widget_size_allocate (child->button, &child_alloc);
    }

  /* update icon geometries */
  if (tasklist->update_icon_geometries_id == 0)
    tasklist->update_icon_geometries_id = g_idle_add_full (G_PRIORITY_LOW, xfce_tasklist_update_icon_geometries,
                                                           tasklist, xfce_tasklist_update_icon_geometries_destroyed);
}



static void
xfce_tasklist_style_set (GtkWidget *widget,
                         GtkStyle  *previous_style)
{
  XfceTasklist *tasklist = XFCE_TASKLIST (widget);
  gint          max_button_length;
  gint          max_button_size;
  gint          min_button_length;
  gint          w, h;

  /* let gtk update the widget style */
  (*GTK_WIDGET_CLASS (xfce_tasklist_parent_class)->style_set) (widget, previous_style);

  /* read the style properties */
  gtk_widget_style_get (GTK_WIDGET (tasklist),
                        "max-button-length", &max_button_length,
                        "min-button-length", &min_button_length,
                        "ellipsize-mode", &tasklist->ellipsize_mode,
                        "max-button-size", &max_button_size,
                        "minimized-icon-lucency", &tasklist->minimized_icon_lucency,
                        "menu-max-width-chars", &tasklist->menu_max_width_chars,
                        NULL);

  if (gtk_icon_size_lookup (menu_icon_size, &w, &h))
    tasklist->menu_icon_size = MIN (w, h);

  /* update the widget */
  if (tasklist->max_button_length != max_button_length
      || tasklist->max_button_size != max_button_size
      || tasklist->min_button_length != min_button_length)
    {
      if (max_button_length > 0)
        {
          /* prevent abuse of the min/max button length */
          tasklist->max_button_length = MAX (min_button_length, max_button_length);
          tasklist->min_button_length = MIN (min_button_length, max_button_length);
        }
      else
        {
          tasklist->max_button_length = max_button_length;
          tasklist->min_button_length = min_button_length;
        }

      tasklist->max_button_size = max_button_size;

      gtk_widget_queue_resize (widget);
    }
}



static void
xfce_tasklist_realize (GtkWidget *widget)
{
  XfceTasklist *tasklist = XFCE_TASKLIST (widget);

  (*GTK_WIDGET_CLASS (xfce_tasklist_parent_class)->realize) (widget);

  /* we now have a screen */
  xfce_tasklist_connect_screen (tasklist);
}



static void
xfce_tasklist_unrealize (GtkWidget *widget)
{
  XfceTasklist *tasklist = XFCE_TASKLIST (widget);

  /* we're going to loose the screen */
  xfce_tasklist_disconnect_screen (tasklist);

  (*GTK_WIDGET_CLASS (xfce_tasklist_parent_class)->unrealize) (widget);
}



static gboolean
xfce_tasklist_scroll_event (GtkWidget      *widget,
                            GdkEventScroll *event)
{
  XfceTasklist      *tasklist = XFCE_TASKLIST (widget);
  XfceTasklistChild *child = NULL;
  GList             *li, *lnew = NULL;

  if (!tasklist->window_scrolling)
    return TRUE;

  /* get the current active button */
  for (li = tasklist->windows; li != NULL; li = li->next)
    {
      child = li->data;

      if (GTK_WIDGET_VISIBLE (child->button)
          && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (child->button)))
        break;
    }

  if (G_UNLIKELY (li == NULL))
    return TRUE;

  switch (event->direction)
    {
    case GDK_SCROLL_UP:
      /* find previous button on the tasklist */
      for (lnew = g_list_previous (li); lnew != NULL; lnew = lnew->prev)
        {
          child = lnew->data;
          if (child->window != NULL
              && GTK_WIDGET_VISIBLE (child->button))
            break;
        }

      /* wrap if the first button is reached */
      lnew = lnew ? lnew : g_list_last (li);
      break;

    case GDK_SCROLL_DOWN:
      /* find the next button on the tasklist */
      for (lnew = g_list_next (li); lnew != NULL; lnew = lnew->next)
        {
          child = lnew->data;
          if (child->window != NULL
              && GTK_WIDGET_VISIBLE (child->button))
            break;
        }

      /* wrap if the last button is reached */
      lnew = lnew ? lnew : g_list_first (li);
      break;

    case GDK_SCROLL_LEFT:
      /* TODO */
      break;

    case GDK_SCROLL_RIGHT:
      /* TODO */
      break;
    }

  if (G_LIKELY(lnew != NULL))
    xfce_tasklist_button_activate (lnew->data, event->time);

  return TRUE;
}



static void
xfce_tasklist_remove (GtkContainer *container,
                      GtkWidget    *widget)
{
  XfceTasklist      *tasklist = XFCE_TASKLIST (container);
  gboolean           was_visible;
  XfceTasklistChild *child;
  GList             *li;

  for (li = tasklist->windows; li != NULL; li = li->next)
    {
      child = li->data;

      if (child->button == widget)
        {
          tasklist->windows = g_list_delete_link (tasklist->windows, li);

          was_visible = GTK_WIDGET_VISIBLE (widget);

          gtk_widget_unparent (child->button);

          if (child->motion_timeout_id != 0)
            g_source_remove (child->motion_timeout_id);

          g_slice_free (XfceTasklistChild, child);

          /* queue a resize if needed */
          if (G_LIKELY (was_visible))
            gtk_widget_queue_resize (GTK_WIDGET (container));

          break;
        }
    }
}



static void
xfce_tasklist_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
  XfceTasklist      *tasklist = XFCE_TASKLIST (container);
  GList             *children = tasklist->windows;
  XfceTasklistChild *child;

  if (include_internals)
    (* callback) (tasklist->arrow_button, callback_data);

  while (children != NULL)
    {
      child = children->data;
      children = children->next;

      (* callback) (child->button, callback_data);
    }
}



static GType
xfce_tasklist_child_type (GtkContainer *container)
{
  return GTK_TYPE_WIDGET;
}



static void
xfce_tasklist_arrow_button_menu_destroy (GtkWidget    *menu,
                                         XfceTasklist *tasklist)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (GTK_IS_TOGGLE_BUTTON (tasklist->arrow_button));
  panel_return_if_fail (GTK_IS_WIDGET (menu));

  gtk_widget_destroy (menu);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tasklist->arrow_button), FALSE);

#ifdef GDK_WINDOWING_X11
  /* make sure the wireframe is hidden */
  xfce_tasklist_wireframe_hide (tasklist);
#endif
}



static void
xfce_tasklist_arrow_button_toggled (GtkWidget    *button,
                                    XfceTasklist *tasklist)
{
  GList             *li;
  XfceTasklistChild *child;
  GtkWidget         *mi;
  GtkWidget         *menu;

  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));
  panel_return_if_fail (tasklist->arrow_button == button);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      menu = gtk_menu_new ();
      g_signal_connect (G_OBJECT (menu), "selection-done",
          G_CALLBACK (xfce_tasklist_arrow_button_menu_destroy), tasklist);

      for (li = tasklist->windows; li != NULL; li = li->next)
        {
          child = li->data;

          if (child->type != CHILD_TYPE_OVERFLOW_MENU)
            continue;

          mi = xfce_tasklist_button_proxy_menu_item (child, TRUE);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
          gtk_widget_show (mi);
        }

      gtk_menu_attach_to_widget (GTK_MENU (menu), button, NULL);
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                      xfce_panel_plugin_position_menu,
                      xfce_tasklist_get_panel_plugin (tasklist),
                      1, gtk_get_current_event_time ());
    }
}



static void
xfce_tasklist_connect_screen (XfceTasklist *tasklist)
{
  GList *windows, *li;

  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (tasklist->screen == NULL);
  panel_return_if_fail (tasklist->gdk_screen == NULL);

  /* set the new screen */
  tasklist->gdk_screen = gtk_widget_get_screen (GTK_WIDGET (tasklist));
  tasklist->screen = wnck_screen_get (gdk_screen_get_number (tasklist->gdk_screen));

  /* add all existing windows on this screen */
  windows = wnck_screen_get_windows (tasklist->screen);
  for (li = windows; li != NULL; li = li->next)
    xfce_tasklist_window_added (tasklist->screen, li->data, tasklist);

  /* monitor gdk changes */
  g_signal_connect (G_OBJECT (tasklist->gdk_screen), "monitors-changed",
      G_CALLBACK (xfce_tasklist_gdk_screen_changed), tasklist);
  g_signal_connect (G_OBJECT (tasklist->gdk_screen), "size-changed",
      G_CALLBACK (xfce_tasklist_gdk_screen_changed), tasklist);

  /* monitor screen changes */
  g_signal_connect (G_OBJECT (tasklist->screen), "active-window-changed",
      G_CALLBACK (xfce_tasklist_active_window_changed), tasklist);
  g_signal_connect (G_OBJECT (tasklist->screen), "active-workspace-changed",
      G_CALLBACK (xfce_tasklist_active_workspace_changed), tasklist);
  g_signal_connect (G_OBJECT (tasklist->screen), "window-opened",
      G_CALLBACK (xfce_tasklist_window_added), tasklist);
  g_signal_connect (G_OBJECT (tasklist->screen), "window-closed",
      G_CALLBACK (xfce_tasklist_window_removed), tasklist);
  g_signal_connect (G_OBJECT (tasklist->screen), "viewports-changed",
      G_CALLBACK (xfce_tasklist_viewports_changed), tasklist);

  /* update the viewport if not all monitors are shown */
  xfce_tasklist_gdk_screen_changed (tasklist->gdk_screen, tasklist);
}



static void
xfce_tasklist_disconnect_screen (XfceTasklist *tasklist)
{
  GSList            *li, *lnext;
  GList             *wi, *wnext;
  XfceTasklistChild *child;
  guint              n;

  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (WNCK_IS_SCREEN (tasklist->screen));
  panel_return_if_fail (GDK_IS_SCREEN (tasklist->gdk_screen));

  /* disconnect monitor signals */
  n = g_signal_handlers_disconnect_matched (G_OBJECT (tasklist->screen),
      G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, tasklist);
  panel_return_if_fail (n == 5);

  /* disconnect geometry changed signals */
  g_signal_handlers_disconnect_by_func (G_OBJECT (tasklist->gdk_screen),
      G_CALLBACK (xfce_tasklist_gdk_screen_changed), tasklist);

  /* delete all known class groups (and their buttons) */
  g_hash_table_remove_all (tasklist->class_groups);

  /* disconnect from all skipped windows */
  for (li = tasklist->skipped_windows; li != NULL; li = lnext)
    {
      lnext = li->next;
      panel_return_if_fail (wnck_window_is_skip_tasklist (WNCK_WINDOW (li->data)));
      xfce_tasklist_window_removed (tasklist->screen, li->data, tasklist);
    }

  /* remove all the windows */
  for (wi = tasklist->windows; wi != NULL; wi = wnext)
    {
      wnext = wi->next;
      child = wi->data;

      /* do a fake window remove */
      panel_return_if_fail (child->type != CHILD_TYPE_GROUP);
      panel_return_if_fail (WNCK_IS_WINDOW (child->window));
      xfce_tasklist_window_removed (tasklist->screen, child->window, tasklist);
    }

  panel_assert (tasklist->windows == NULL);
  panel_assert (tasklist->skipped_windows == NULL);

  tasklist->screen = NULL;
  tasklist->gdk_screen = NULL;
}



static void
xfce_tasklist_gdk_screen_changed (GdkScreen    *gdk_screen,
                                  XfceTasklist *tasklist)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (GDK_IS_SCREEN (gdk_screen));
  panel_return_if_fail (tasklist->gdk_screen == gdk_screen);

  if (!tasklist->all_monitors)
    {
      /* update the monitor geometry */
      xfce_tasklist_update_monitor_geometry (tasklist);
    }
}



static void
xfce_tasklist_active_window_changed (WnckScreen   *screen,
                                     WnckWindow   *previous_window,
                                     XfceTasklist *tasklist)
{
  WnckWindow        *active_window;
  GList             *li;
  XfceTasklistChild *child;

  panel_return_if_fail (WNCK_IS_SCREEN (screen));
  panel_return_if_fail (previous_window == NULL || WNCK_IS_WINDOW (previous_window));
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (tasklist->screen == screen);

  /* get the new active window */
  active_window = wnck_screen_get_active_window (screen);

  /* lock the taskbar */
  xfce_taskbar_lock (tasklist);

  for (li = tasklist->windows; li != NULL; li = li->next)
    {
      child = li->data;

      /* update timestamp for window */
      if (child->window == active_window)
        g_get_current_time (&child->last_focused);

      /* set the toggle button state */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (child->button),
                                    !!(child->window == active_window));
    }

  /* release the lock */
  xfce_taskbar_unlock (tasklist);
}



static void
xfce_tasklist_active_workspace_changed (WnckScreen    *screen,
                                        WnckWorkspace *previous_workspace,
                                        XfceTasklist  *tasklist)
{
  GList             *li;
  WnckWorkspace     *active_ws;
  XfceTasklistChild *child;

  panel_return_if_fail (WNCK_IS_SCREEN (screen));
  panel_return_if_fail (previous_workspace == NULL || WNCK_IS_WORKSPACE (previous_workspace));
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (tasklist->screen == screen);

  /* leave when we are locked or show all workspaces. the null
   * check for @previous_workspace is used to update the tasklist
   * on setting changes */
  if (xfce_taskbar_is_locked (tasklist)
      || (previous_workspace != NULL
          && tasklist->all_workspaces))
    return;

  /* walk all the children and update their visibility */
  active_ws = wnck_screen_get_active_workspace (screen);
  for (li = tasklist->windows; li != NULL; li = li->next)
    {
      child = li->data;

      if (child->type != CHILD_TYPE_GROUP)
        {
          if (xfce_tasklist_button_visible (child, active_ws))
            gtk_widget_show (child->button);
          else
            gtk_widget_hide (child->button);
        }
    }
}



static void
xfce_tasklist_window_added (WnckScreen   *screen,
                            WnckWindow   *window,
                            XfceTasklist *tasklist)
{
  XfceTasklistChild *child;
  XfceTasklistChild *group_child = NULL;
  gboolean           found;

  panel_return_if_fail (WNCK_IS_SCREEN (screen));
  panel_return_if_fail (WNCK_IS_WINDOW (window));
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (tasklist->screen == screen);
  panel_return_if_fail (wnck_window_get_screen (window) == screen);

  /* ignore this window, but watch it for state changes */
  if (wnck_window_is_skip_tasklist (window))
    {
      tasklist->skipped_windows = g_slist_prepend (tasklist->skipped_windows, window);
      g_signal_connect (G_OBJECT (window), "state-changed",
          G_CALLBACK (xfce_tasklist_skipped_windows_state_changed), tasklist);

      return;
    }

  /* create new window button */
  child = xfce_tasklist_button_new (window, tasklist);

  /* initial visibility of the function */
  if (xfce_tasklist_button_visible (child, wnck_screen_get_active_workspace (screen)))
    gtk_widget_show (child->button);

  if (G_LIKELY (child->class_group != NULL))
    {
      /* we need to ref the class group else the value returned from
       * wnck_window_get_class_group() is null */
      panel_return_if_fail (WNCK_IS_CLASS_GROUP (child->class_group));
      g_object_ref (G_OBJECT (child->class_group));

      found = g_hash_table_lookup_extended (tasklist->class_groups,
                                            child->class_group,
                                            NULL, (gpointer *) &group_child);

      if (G_UNLIKELY (tasklist->grouping == XFCE_TASKLIST_GROUPING_ALWAYS))
        {

          if (group_child == NULL)
            {
              /* create group button for this window and add it */
              group_child = xfce_tasklist_group_button_new (child->class_group, tasklist);
              g_hash_table_insert (tasklist->class_groups,
                                   g_object_ref (child->class_group),
                                   group_child);
            }

          /* add window to the group button */
          xfce_tasklist_group_button_add_window (group_child, child);
        }
      else if (!found)
        {
          /* add group in hash table without button */
          g_hash_table_insert (tasklist->class_groups,
                               g_object_ref (child->class_group), NULL);
        }
    }

  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}



static void
xfce_tasklist_window_removed (WnckScreen   *screen,
                              WnckWindow   *window,
                              XfceTasklist *tasklist)
{
  GList             *li;
  GSList            *lp;
  XfceTasklistChild *child;
  //GList             *windows, *lp;
  //gboolean           remove_class_group = TRUE;
  guint              n;

  panel_return_if_fail (WNCK_IS_SCREEN (screen));
  panel_return_if_fail (WNCK_IS_WINDOW (window));
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (tasklist->screen == screen);

  /* check if the window is in our skipped window list */
  if (wnck_window_is_skip_tasklist (window)
      && (lp = g_slist_find (tasklist->skipped_windows, window)) != NULL)
    {
      tasklist->skipped_windows = g_slist_delete_link (tasklist->skipped_windows, lp);
      g_signal_handlers_disconnect_by_func (G_OBJECT (window),
          G_CALLBACK (xfce_tasklist_skipped_windows_state_changed), tasklist);

      return;
    }

  /* remove the child from the taskbar */
  for (li = tasklist->windows; li != NULL; li = li->next)
    {
      child = li->data;

      if (child->window == window)
        {
          if (child->class_group != NULL)
            {
              /* remove the class group from the internal list if this
               * was the last window in the group */
              /* TODO
              windows = wnck_class_group_get_windows (child->class_group);
              for (lp = windows; remove_class_group && lp != NULL; lp = lp->next)
                if (!wnck_window_is_skip_tasklist (WNCK_WINDOW (lp->data)))
                  remove_class_group = FALSE;

              if (remove_class_group)
                {
                  tasklist->class_groups = g_slist_remove (tasklist->class_groups,
                                                           child->class_group);
                }*/

              panel_return_if_fail (WNCK_IS_CLASS_GROUP (child->class_group));
              g_object_unref (G_OBJECT (child->class_group));
            }

          /* disconnect from all the window watch functions */
          panel_return_if_fail (WNCK_IS_WINDOW (window));
          n = g_signal_handlers_disconnect_matched (G_OBJECT (window),
              G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, child);

#ifdef GDK_WINDOWING_X11
          /* hide the wireframe */
          if (G_UNLIKELY (n > 5 && tasklist->show_wireframes))
            {
              xfce_tasklist_wireframe_hide (tasklist);
              n--;
            }
#endif

          panel_return_if_fail (n == 5);

          /* destroy the button, this will free the child data in the
           * container remove function */
          gtk_widget_destroy (child->button);

          break;
        }
    }
}



static void
xfce_tasklist_viewports_changed (WnckScreen   *screen,
                                 XfceTasklist *tasklist)
{
  WnckWorkspace *active_ws;

  panel_return_if_fail (WNCK_IS_SCREEN (screen));
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (tasklist->screen == screen);

  /* pretend we changed workspace, this will update the
   * visibility of all the buttons */
  active_ws = wnck_screen_get_active_workspace (screen);
  xfce_tasklist_active_workspace_changed (screen, active_ws, tasklist);
}



static void
xfce_tasklist_skipped_windows_state_changed (WnckWindow      *window,
                                             WnckWindowState  changed_state,
                                             WnckWindowState  new_state,
                                             XfceTasklist    *tasklist)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (WNCK_IS_WINDOW (window));
  panel_return_if_fail (g_slist_find (tasklist->skipped_windows, window) != NULL);

  if (PANEL_HAS_FLAG (changed_state, WNCK_WINDOW_STATE_SKIP_TASKLIST))
    {
      /* remove from list */
      tasklist->skipped_windows = g_slist_remove (tasklist->skipped_windows, window);
      g_signal_handlers_disconnect_by_func (G_OBJECT (window),
          G_CALLBACK (xfce_tasklist_skipped_windows_state_changed), tasklist);

      /* pretend a normal window insert */
      xfce_tasklist_window_added (wnck_window_get_screen (window), window, tasklist);
    }
}



static void
xfce_tasklist_sort (XfceTasklist *tasklist)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));

  if (tasklist->sort_order != XFCE_TASKLIST_SORT_ORDER_DND)
    tasklist->windows = g_list_sort_with_data (tasklist->windows,
                                               xfce_tasklist_button_compare,
                                               tasklist);

  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}



static gboolean
xfce_tasklist_update_icon_geometries (gpointer data)
{

  XfceTasklist      *tasklist = XFCE_TASKLIST (data);
  GList             *li;
  XfceTasklistChild *child, *child2;
  GtkAllocation     *alloc;
  GSList            *lp;
  gint               root_x, root_y;
  GtkWidget         *toplevel;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (tasklist));
  gtk_window_get_position (GTK_WINDOW (toplevel), &root_x, &root_y);
  panel_return_val_if_fail (XFCE_IS_TASKLIST (tasklist), FALSE);

  for (li = tasklist->windows; li != NULL; li = li->next)
    {
      child = li->data;

      switch (child->type)
        {
        case CHILD_TYPE_WINDOW:
          alloc = &child->button->allocation;
          panel_return_val_if_fail (WNCK_IS_WINDOW (child->window), FALSE);
          wnck_window_set_icon_geometry (child->window, alloc->x + root_x,
                                         alloc->y + root_y, alloc->width,
                                         alloc->height);
          break;

        case CHILD_TYPE_GROUP:
          alloc = &child->button->allocation;
          for (lp = child->windows; lp != NULL; lp = lp->next)
            {
              child2 = lp->data;
              panel_return_val_if_fail (WNCK_IS_WINDOW (child2->window), FALSE);
              wnck_window_set_icon_geometry (child2->window, alloc->x + root_x,
                                             alloc->y + root_y, alloc->width,
                                             alloc->height);
            }
          break;

        case CHILD_TYPE_OVERFLOW_MENU:
          alloc = &tasklist->arrow_button->allocation;
          panel_return_val_if_fail (WNCK_IS_WINDOW (child->window), FALSE);
          wnck_window_set_icon_geometry (child->window, alloc->x + root_x,
                                         alloc->y + root_y, alloc->width,
                                         alloc->height);
          break;

        case CHILD_TYPE_GROUP_MENU:
          /* we already handled those in the group button */
          break;
        };
    }

  return FALSE;
}



static void
xfce_tasklist_update_icon_geometries_destroyed (gpointer data)
{
  XFCE_TASKLIST (data)->update_icon_geometries_id = 0;
}



static gboolean
xfce_tasklist_update_monitor_geometry_idle (gpointer data)
{
  XfceTasklist *tasklist = XFCE_TASKLIST (data);
  GdkScreen    *screen;
  gboolean      geometry_set = FALSE;
  GdkWindow    *window;
  guint         tmp;

  panel_return_val_if_fail (XFCE_IS_TASKLIST (tasklist), FALSE);

  GDK_THREADS_ENTER ();

  if (!tasklist->all_monitors)
    {
      screen = gtk_widget_get_screen (GTK_WIDGET (tasklist));
      window = gtk_widget_get_window (GTK_WIDGET (tasklist));

      if (G_LIKELY (screen != NULL
          && window != NULL
          && (tasklist->n_monitors = gdk_screen_get_n_monitors (screen)) > 1))
        {
          /* set the monitor geometry */
          tasklist->my_monitor = gdk_screen_get_monitor_at_window (screen, window);

          if (tasklist->all_monitors_geometry)
            tasklist->all_monitors_geometry = g_renew (GdkRectangle, tasklist->all_monitors_geometry, tasklist->n_monitors);
          else
            tasklist->all_monitors_geometry = g_new (GdkRectangle, tasklist->n_monitors);

          for(tmp = 0; tmp < tasklist->n_monitors; tmp++)
            gdk_screen_get_monitor_geometry (screen, tmp, &tasklist->all_monitors_geometry[tmp]);

          geometry_set = TRUE;
        }
    }

  /* make sure we never poke the window geometry unneeded
   * in the visibility function */
  if (!geometry_set)
    xfce_tasklist_geometry_set_invalid (tasklist);

  /* update visibility of buttons */
  if (tasklist->screen != NULL)
    xfce_tasklist_active_workspace_changed (tasklist->screen,
                                            NULL, tasklist);

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
xfce_tasklist_update_monitor_geometry_idle_destroy (gpointer data)
{
  XFCE_TASKLIST (data)->update_monitor_geometry_id = 0;
}



static gboolean
xfce_tasklist_child_drag_motion_timeout (gpointer data)
{
  XfceTasklistChild *child = data;

  panel_return_val_if_fail (XFCE_IS_TASKLIST (child->tasklist), FALSE);
  panel_return_val_if_fail (WNCK_IS_SCREEN (child->tasklist->screen), FALSE);

  GDK_THREADS_ENTER ();

  if (child->type == CHILD_TYPE_WINDOW)
    {
      xfce_tasklist_button_activate (child, child->motion_timestamp);
    }
  else if (child->type == CHILD_TYPE_GROUP)
    {
       /* TODO popup menu */
    }

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
xfce_tasklist_child_drag_motion_timeout_destroyed (gpointer data)
{
  XfceTasklistChild *child = data;

  child->motion_timeout_id = 0;
  child->motion_timestamp = 0;
}



static gboolean
xfce_tasklist_child_drag_motion (XfceTasklistChild *child,
                                 GdkDragContext    *context,
                                 gint               x,
                                 gint               y,
                                 guint              timestamp)
{
  GtkWidget *dnd_widget;

  panel_return_val_if_fail (XFCE_IS_TASKLIST (child->tasklist), FALSE);

  /* don't respond to dragging our own children or panel plugins */
  dnd_widget = gtk_drag_get_source_widget (context);
  if (dnd_widget == NULL
      || (gtk_widget_get_parent (dnd_widget) != GTK_WIDGET (child->tasklist)
           && !XFCE_IS_PANEL_PLUGIN (dnd_widget)))
    {
      child->motion_timestamp = timestamp;
      if (child->motion_timeout_id == 0
          && !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (child->button)))
        {
          child->motion_timeout_id = g_timeout_add_full (G_PRIORITY_LOW, DRAG_ACTIVATE_TIMEOUT,
                                                         xfce_tasklist_child_drag_motion_timeout, child,
                                                         xfce_tasklist_child_drag_motion_timeout_destroyed);
        }

      /* keep emitting the signal */
      gdk_drag_status (context, 0, timestamp);

      /* we want to receive leave signal as well */
      return TRUE;
    }
  else if (gtk_drag_dest_find_target (child->button, context, NULL) != GDK_NONE)
    {
      /* dnd to reorder buttons */
      gdk_drag_status (context, GDK_ACTION_MOVE, timestamp);

      return TRUE;
    }

  /* also send drag-motion to other widgets */
  return FALSE;
}



static void
xfce_tasklist_child_drag_leave (XfceTasklistChild *child,
                                GdkDragContext    *context,
                                GtkDragResult      result)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (child->tasklist));

  if (child->motion_timeout_id != 0)
    g_source_remove (child->motion_timeout_id);
}



static XfceTasklistChild *
xfce_tasklist_child_new (XfceTasklist *tasklist)
{
  XfceTasklistChild *child;

  panel_return_val_if_fail (XFCE_IS_TASKLIST (tasklist), NULL);

  child = g_slice_new0 (XfceTasklistChild);
  child->tasklist = tasklist;

  /* create the window button */
  child->button = xfce_arrow_button_new (GTK_ARROW_NONE);
  gtk_widget_set_parent (child->button, GTK_WIDGET (tasklist));
  gtk_button_set_relief (GTK_BUTTON (child->button),
                         tasklist->button_relief);

  child->box = xfce_hvbox_new (!xfce_tasklist_vertical (tasklist) ?
      GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL, FALSE, 6);
  gtk_container_add (GTK_CONTAINER (child->button), child->box);
  gtk_widget_show (child->box);

  child->icon = xfce_panel_image_new ();
  if (tasklist->show_labels)
    gtk_box_pack_start (GTK_BOX (child->box), child->icon, FALSE, TRUE, 0);
  else
    gtk_box_pack_start (GTK_BOX (child->box), child->icon, TRUE, TRUE, 0);
  if (tasklist->minimized_icon_lucency > 0)
    gtk_widget_show (child->icon);

  child->label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (child->box), child->label, TRUE, TRUE, 0);
  if (!xfce_tasklist_vertical (tasklist))
    {
      /* gtk_box_reorder_child (GTK_BOX (child->box), child->icon, 0); */
      gtk_misc_set_alignment (GTK_MISC (child->label), 0.0, 0.5);
      gtk_label_set_ellipsize (GTK_LABEL (child->label), tasklist->ellipsize_mode);
    }
  else
    {
      /* gtk_box_reorder_child (GTK_BOX (child->box), child->icon, -1); */
      gtk_label_set_angle (GTK_LABEL (child->label), 270);
      gtk_misc_set_alignment (GTK_MISC (child->label), 0.50, 0.00);
      /* TODO can we already ellipsize here yet? */
    }

  /* don't show the label if we're in iconbox style */
  if (tasklist->show_labels)
    gtk_widget_show (child->label);

  gtk_drag_dest_set (GTK_WIDGET (child->button), 0,
                     NULL, 0, GDK_ACTION_DEFAULT);
  g_signal_connect_swapped (G_OBJECT (child->button), "drag-motion",
     G_CALLBACK (xfce_tasklist_child_drag_motion), child);
  g_signal_connect_swapped (G_OBJECT (child->button), "drag-leave",
     G_CALLBACK (xfce_tasklist_child_drag_leave), child);

  return child;
}



/**
 * Wire Frame
 **/
#ifdef GDK_WINDOWING_X11
static void
xfce_tasklist_wireframe_hide (XfceTasklist *tasklist)
{
  GdkDisplay *dpy;

  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));

  if (tasklist->wireframe_window != 0)
    {
      /* unmap the window */
      dpy = gtk_widget_get_display (GTK_WIDGET (tasklist));
      XUnmapWindow (GDK_DISPLAY_XDISPLAY (dpy), tasklist->wireframe_window);
    }
}



static void
xfce_tasklist_wireframe_destroy (XfceTasklist *tasklist)
{
  GdkDisplay *dpy;

  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));

  if (tasklist->wireframe_window != 0)
    {
      /* unmap and destroy the window */
      dpy = gtk_widget_get_display (GTK_WIDGET (tasklist));
      XUnmapWindow (GDK_DISPLAY_XDISPLAY (dpy), tasklist->wireframe_window);
      XDestroyWindow (GDK_DISPLAY_XDISPLAY (dpy), tasklist->wireframe_window);

      tasklist->wireframe_window = 0;
    }
}



static void
xfce_tasklist_wireframe_update (XfceTasklist      *tasklist,
                                XfceTasklistChild *child)
{
  Display              *dpy;
  GdkDisplay           *gdpy;
  gint                  x, y, width, height;
  XSetWindowAttributes  attrs;
  GC                    gc;
  XRectangle            xrect;

  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (tasklist->show_wireframes == TRUE);
  panel_return_if_fail (WNCK_IS_WINDOW (child->window));

  /* get the window geometry */
  wnck_window_get_geometry (child->window, &x, &y, &width, &height);

  gdpy = gtk_widget_get_display (GTK_WIDGET (tasklist));
  dpy = GDK_DISPLAY_XDISPLAY (gdpy);

  if (G_LIKELY (tasklist->wireframe_window != 0))
    {
      /* reposition the wireframe */
      XMoveResizeWindow (dpy, tasklist->wireframe_window, x, y, width, height);

      /* full window rectangle */
      xrect.x = 0;
      xrect.y = 0;
      xrect.width = width;
      xrect.height = height;

      /* we need to restore the window first */
      XShapeCombineRectangles (dpy, tasklist->wireframe_window, ShapeBounding,
                               0, 0, &xrect, 1, ShapeSet, Unsorted);
    }
  else
    {
      /* set window attributes */
      attrs.override_redirect = True;
      attrs.background_pixel = 0x000000;

      /* create new window */
      tasklist->wireframe_window = XCreateWindow (dpy, DefaultRootWindow (dpy),
                                                  x, y, width, height, 0,
                                                  CopyFromParent, InputOutput,
                                                  CopyFromParent,
                                                  CWOverrideRedirect | CWBackPixel,
                                                  &attrs);
    }

  /* create rectangle what will be 'transparent' in the window */
  xrect.x = WIREFRAME_SIZE;
  xrect.y = WIREFRAME_SIZE;
  xrect.width = width - WIREFRAME_SIZE * 2;
  xrect.height = height - WIREFRAME_SIZE * 2;

  /* substruct rectangle from the window */
  XShapeCombineRectangles (dpy, tasklist->wireframe_window, ShapeBounding,
                           0, 0, &xrect, 1, ShapeSubtract, Unsorted);

  /* map the window */
  XMapWindow (dpy, tasklist->wireframe_window);

  /* create a white gc */
  gc = XCreateGC (dpy, tasklist->wireframe_window, 0, NULL);
  XSetForeground (dpy, gc, 0xffffff);

  /* draw the outer white rectangle */
  XDrawRectangle (dpy, tasklist->wireframe_window, gc,
                  0, 0, width - 1, height - 1);

  /* draw the inner white rectangle */
  XDrawRectangle (dpy, tasklist->wireframe_window, gc,
                  WIREFRAME_SIZE - 1, WIREFRAME_SIZE - 1,
                  width - 2 * (WIREFRAME_SIZE - 1) - 1,
                  height - 2 * (WIREFRAME_SIZE - 1) - 1);

  XFreeGC (dpy, gc);
}
#endif



/**
 * Tasklist Buttons
 **/
static inline gboolean
xfce_tasklist_button_visible (XfceTasklistChild *child,
                              WnckWorkspace     *active_ws)
{
  XfceTasklist *tasklist = XFCE_TASKLIST (child->tasklist);
  GdkRectangle  window, intersection;
  guint         best_size = 0, best_monitor = 0, size, tmp;

  panel_return_val_if_fail (active_ws == NULL || WNCK_IS_WORKSPACE (active_ws), FALSE);
  panel_return_val_if_fail (XFCE_IS_TASKLIST (tasklist), FALSE);
  panel_return_val_if_fail (WNCK_IS_WINDOW (child->window), FALSE);

  if (xfce_tasklist_filter_monitors (tasklist))
    {
      /* center of the window must be on this screen */
      wnck_window_get_geometry (child->window, &window.x, &window.y, &window.width, &window.height);

      for (tmp = 0; tmp < tasklist->n_monitors; tmp++)
        {
          gdk_rectangle_intersect(&tasklist->all_monitors_geometry[tmp], &window, &intersection);
          size = intersection.width * intersection.height;
          if (size > best_size)
            {
              best_size = size;
              best_monitor = tmp;
            }
        }
      if (best_monitor != tasklist->my_monitor)
        return FALSE;
    }

  if (tasklist->all_workspaces
      || (active_ws != NULL
          && (G_UNLIKELY (wnck_workspace_is_virtual (active_ws))
              ? wnck_window_is_in_viewport (child->window, active_ws)
              : wnck_window_is_on_workspace (child->window, active_ws)))
      || (tasklist->all_blinking
          && xfce_arrow_button_get_blinking (XFCE_ARROW_BUTTON (child->button))))
    {
      return (!tasklist->only_minimized
              || wnck_window_is_minimized (child->window));
    }

  return FALSE;
}



static gint
xfce_tasklist_button_compare (gconstpointer child_a,
                              gconstpointer child_b,
                              gpointer      user_data)
{
  const XfceTasklistChild *a = child_a, *b = child_b;
  XfceTasklist            *tasklist = XFCE_TASKLIST (user_data);
  gint                     retval;
  WnckClassGroup          *class_group_a, *class_group_b;
  const gchar             *name_a, *name_b;
  WnckWorkspace           *workspace_a, *workspace_b;
  gint                     num_a, num_b;

  panel_return_val_if_fail (a->type == CHILD_TYPE_GROUP
                            || WNCK_IS_WINDOW (a->window), 0);
  panel_return_val_if_fail (b->type == CHILD_TYPE_GROUP
                            || WNCK_IS_WINDOW (b->window), 0);

  /* just append to the list */
  if (tasklist->sort_order == XFCE_TASKLIST_SORT_ORDER_DND)
    return a->unique_id - b->unique_id;

  if (tasklist->all_workspaces)
    {
      /* get workspace (this is slightly inefficient because the WnckWindow
       * also stores the workspace number, not the structure, and we use that
       * for comparing too */
      workspace_a = a->window != NULL ? wnck_window_get_workspace (a->window) : NULL;
      workspace_b = b->window != NULL ? wnck_window_get_workspace (b->window) : NULL;

      /* skip this if windows are in same worspace, or both pinned (== NULL) */
      if (workspace_a != workspace_b)
        {
          /* NULL means the window is pinned */
          if (workspace_a == NULL)
            workspace_a = wnck_screen_get_active_workspace (tasklist->screen);
          if (workspace_b == NULL)
            workspace_b = wnck_screen_get_active_workspace (tasklist->screen);

          /* compare by workspace number */
          num_a = wnck_workspace_get_number (workspace_a);
          num_b = wnck_workspace_get_number (workspace_b);
          if (num_a != num_b)
            return num_a - num_b;
        }
    }

  if (tasklist->sort_order == XFCE_TASKLIST_SORT_ORDER_GROUP_TITLE
      || tasklist->sort_order == XFCE_TASKLIST_SORT_ORDER_GROUP_TIMESTAMP)
    {
      /* compare by class group names */
      class_group_a = a->class_group;
      class_group_b = b->class_group;

      /* skip this if windows are in same group (or both NULL) */
      if (class_group_a != class_group_b)
        {
          name_a = NULL;
          name_b = NULL;

          /* get the group name if available */
          if (G_LIKELY (class_group_a != NULL))
            name_a = wnck_class_group_get_name (class_group_a);
          if (G_LIKELY (class_group_b != NULL))
            name_b = wnck_class_group_get_name (class_group_b);

          /* if there is no class group name, use the window name */
          if (exo_str_is_empty (name_a)
              && a->window != NULL)
            name_a = wnck_window_get_name (a->window);
          if (exo_str_is_empty (name_b)
              && b->window != NULL)
            name_b = wnck_window_get_name (b->window) ;

          if (name_a == NULL)
            name_a = "";
          if (name_b == NULL)
            name_b = "";

          retval = strcasecmp (name_a, name_b);
          if (retval != 0)
            return retval;
        }
      else if (a->type != b->type)
        {
          /* put the group in front of the other window buttons
           * with the same group */
          return b->type - a->type;
        }
    }

  if (tasklist->sort_order == XFCE_TASKLIST_SORT_ORDER_TIMESTAMP
      || tasklist->sort_order == XFCE_TASKLIST_SORT_ORDER_GROUP_TIMESTAMP)
    {
      return a->unique_id - b->unique_id;
    }
  else
    {
      if (a->window != NULL)
        name_a = wnck_window_get_name (a->window);
      else if (a->class_group != NULL)
        name_a = wnck_class_group_get_name (a->class_group);
      else
        name_a = NULL;

      if (b->window != NULL)
        name_b = wnck_window_get_name (b->window);
      else if (b->class_group != NULL)
        name_b = wnck_class_group_get_name (b->class_group);
      else
        name_b = NULL;

      if (name_a == NULL)
        name_a = "";
      if (name_b == NULL)
        name_b = "";

      return strcasecmp (name_a, name_b);
    }
}



static void
xfce_tasklist_button_icon_changed (WnckWindow        *window,
                                   XfceTasklistChild *child)
{
  GdkPixbuf    *pixbuf;
  GdkPixbuf    *lucent = NULL;
  XfceTasklist *tasklist = child->tasklist;

  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (XFCE_IS_PANEL_IMAGE (child->icon));
  panel_return_if_fail (WNCK_IS_WINDOW (window));
  panel_return_if_fail (child->window == window);

  /* 0 means icons are disabled */
  if (tasklist->minimized_icon_lucency == 0)
    return;

  /* get the window icon */
  if (tasklist->show_labels)
    pixbuf = wnck_window_get_mini_icon (window);
  else
    pixbuf = wnck_window_get_icon (window);

  /* leave when there is no valid pixbuf */
  if (G_UNLIKELY (pixbuf == NULL))
    {
      xfce_panel_image_clear (XFCE_PANEL_IMAGE (child->icon));
      return;
    }

  /* create a spotlight version of the icon when minimized */
  if (!tasklist->only_minimized
      && tasklist->minimized_icon_lucency < 100
      && wnck_window_is_minimized (window))
    {
      lucent = exo_gdk_pixbuf_lucent (pixbuf, tasklist->minimized_icon_lucency);
      if (G_UNLIKELY (lucent != NULL))
        pixbuf = lucent;
    }

  xfce_panel_image_set_from_pixbuf (XFCE_PANEL_IMAGE (child->icon), pixbuf);

  if (lucent != NULL && lucent != pixbuf)
    g_object_unref (G_OBJECT (lucent));
}



static void
xfce_tasklist_button_name_changed (WnckWindow        *window,
                                   XfceTasklistChild *child)
{
  const gchar *name;
  gchar       *label = NULL;

  panel_return_if_fail (window == NULL || child->window == window);
  panel_return_if_fail (WNCK_IS_WINDOW (child->window));
  panel_return_if_fail (XFCE_IS_TASKLIST (child->tasklist));

  name = wnck_window_get_name (child->window);
  gtk_widget_set_tooltip_text (GTK_WIDGET (child->button), name);

  /* create the button label */
  if (!child->tasklist->only_minimized
      && wnck_window_is_minimized (child->window))
    name = label = g_strdup_printf ("[%s]", name);
  else if (wnck_window_is_shaded (child->window))
    name = label = g_strdup_printf ("=%s=", name);

  gtk_label_set_text (GTK_LABEL (child->label), name);

  g_free (label);

  /* if window is null, we have not inserted the button the in
   * tasklist, so no need to sort, because we insert with sorting */
  if (window != NULL)
    xfce_tasklist_sort (child->tasklist);
}



static void
xfce_tasklist_button_state_changed (WnckWindow        *window,
                                    WnckWindowState    changed_state,
                                    WnckWindowState    new_state,
                                    XfceTasklistChild *child)
{
  gboolean       blink;
  WnckScreen    *screen;
  XfceTasklist  *tasklist;
  WnckWorkspace *active_ws;

  panel_return_if_fail (WNCK_IS_WINDOW (window));
  panel_return_if_fail (child->window == window);
  panel_return_if_fail (XFCE_IS_TASKLIST (child->tasklist));

  /* remove if the new state is hidding the window from the tasklist */
  if (PANEL_HAS_FLAG (changed_state, WNCK_WINDOW_STATE_SKIP_TASKLIST))
    {
      screen = wnck_window_get_screen (window);
      tasklist = child->tasklist;

      /* remove button from tasklist */
      xfce_tasklist_window_removed (screen, window, child->tasklist);

      /* add the window to the skipped_windows list */
      xfce_tasklist_window_added (screen, window, tasklist);

      return;
    }

  /* update the button name */
  if (PANEL_HAS_FLAG (changed_state, WNCK_WINDOW_STATE_SHADED | WNCK_WINDOW_STATE_MINIMIZED)
      && !child->tasklist->only_minimized)
    xfce_tasklist_button_name_changed (window, child);

  /* update the button icon if needed */
  if (PANEL_HAS_FLAG (changed_state, WNCK_WINDOW_STATE_MINIMIZED))
    {
      if (G_UNLIKELY (child->tasklist->only_minimized))
        {
          if (PANEL_HAS_FLAG (new_state, WNCK_WINDOW_STATE_MINIMIZED))
            gtk_widget_show (child->button);
          else
            gtk_widget_hide (child->button);
        }
      else
        {
          /* update the icon (lucent) */
          xfce_tasklist_button_icon_changed (window, child);
        }
    }

  /* update the blinking state */
  if (PANEL_HAS_FLAG (changed_state, WNCK_WINDOW_STATE_DEMANDS_ATTENTION)
      || PANEL_HAS_FLAG (changed_state, WNCK_WINDOW_STATE_URGENT))
    {
      /* only start blinking if the window requesting urgency
       * notification is not the active window */
      blink = wnck_window_or_transient_needs_attention (window);
      if (!blink || (blink && !wnck_window_is_active (window)))
        {
          /* if we have all_blinking set make sure we toggle visibility of the button
           * in case the window is not in the current workspace */
          active_ws = wnck_screen_get_active_workspace (child->tasklist->screen);
          if (child->tasklist->all_blinking && blink
              && !xfce_tasklist_button_visible (child, active_ws))
            {
              gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (child->button), FALSE);
              gtk_widget_show (child->button);
            }

          xfce_arrow_button_set_blinking (XFCE_ARROW_BUTTON (child->button), blink);

          if (child->tasklist->all_blinking
              && !xfce_tasklist_button_visible (child, active_ws))
            gtk_widget_hide (child->button);
        }
    }
}



static void
xfce_tasklist_button_workspace_changed (WnckWindow        *window,
                                        XfceTasklistChild *child)
{
  XfceTasklist *tasklist = XFCE_TASKLIST (child->tasklist);

  panel_return_if_fail (child->window == window);
  panel_return_if_fail (XFCE_IS_TASKLIST (child->tasklist));

  xfce_tasklist_sort (tasklist);

  /* make sure we don't have two active windows (bug #6474) */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (child->button), FALSE);

  if (!tasklist->all_workspaces)
    xfce_tasklist_active_workspace_changed (tasklist->screen, NULL, tasklist);
}



static void
xfce_tasklist_button_geometry_changed2 (WnckWindow        *window,
                                       XfceTasklistChild *child)
{
  WnckWorkspace *active_ws;

  panel_return_if_fail (child->window == window);
  panel_return_if_fail (XFCE_IS_TASKLIST (child->tasklist));
  panel_return_if_fail (WNCK_IS_SCREEN (child->tasklist->screen));

  if (xfce_tasklist_filter_monitors (child->tasklist))
    {
      /* check if we need to change the visibility of the button */
      active_ws = wnck_screen_get_active_workspace (child->tasklist->screen);
      if (xfce_tasklist_button_visible (child, active_ws))
        gtk_widget_show (child->button);
      else
        gtk_widget_hide (child->button);
    }
}



#ifdef GDK_WINDOWING_X11
static void
xfce_tasklist_button_geometry_changed (WnckWindow        *window,
                                       XfceTasklistChild *child)
{
  panel_return_if_fail (child->window == window);
  panel_return_if_fail (XFCE_IS_TASKLIST (child->tasklist));

  xfce_tasklist_wireframe_update (child->tasklist, child);
}



static gboolean
xfce_tasklist_button_leave_notify_event (GtkWidget         *button,
                                         GdkEventCrossing  *event,
                                         XfceTasklistChild *child)
{
  panel_return_val_if_fail (XFCE_IS_TASKLIST (child->tasklist), FALSE);
  panel_return_val_if_fail (child->type != CHILD_TYPE_GROUP, FALSE);

  /* disconnect signals */
  g_signal_handlers_disconnect_by_func (button,
      xfce_tasklist_button_leave_notify_event, child);
  g_signal_handlers_disconnect_by_func (child->window,
      xfce_tasklist_button_geometry_changed, child);

  /* unmap and destroy the wireframe window */
  xfce_tasklist_wireframe_hide (child->tasklist);

  return FALSE;
}
#endif



static gboolean
xfce_tasklist_button_enter_notify_event (GtkWidget         *button,
                                         GdkEventCrossing  *event,
                                         XfceTasklistChild *child)
{
  panel_return_val_if_fail (XFCE_IS_TASKLIST (child->tasklist), FALSE);
  panel_return_val_if_fail (child->type != CHILD_TYPE_GROUP, FALSE);
  panel_return_val_if_fail (GTK_IS_WIDGET (button), FALSE);
  panel_return_val_if_fail (WNCK_IS_WINDOW (child->window), FALSE);

#ifdef GDK_WINDOWING_X11
  /* leave when there is nothing to do */
  if (!child->tasklist->show_wireframes)
    return FALSE;

  /* show wireframe for the child */
  xfce_tasklist_wireframe_update (child->tasklist, child);

  /* connect signal to destroy the window when the user leaves the button */
  g_signal_connect (G_OBJECT (button), "leave-notify-event",
      G_CALLBACK (xfce_tasklist_button_leave_notify_event), child);

  /* watch geometry changes */
  g_signal_connect (G_OBJECT (child->window), "geometry-changed",
      G_CALLBACK (xfce_tasklist_button_geometry_changed), child);
#endif

  return FALSE;
}



static gboolean
xfce_tasklist_button_button_press_event (GtkWidget         *button,
                                         GdkEventButton    *event,
                                         XfceTasklistChild *child)
{
  GtkWidget *menu, *panel_plugin;

  panel_return_val_if_fail (XFCE_IS_TASKLIST (child->tasklist), FALSE);
  panel_return_val_if_fail (child->type != CHILD_TYPE_GROUP, FALSE);

  if (event->type != GDK_BUTTON_PRESS
      || xfce_taskbar_is_locked (child->tasklist))
    return FALSE;

  /* send the event to the panel plugin if control is pressed */
  if (PANEL_HAS_FLAG (event->state, GDK_CONTROL_MASK))
    {
      /* send the event to the panel plugin */
      panel_plugin = xfce_tasklist_get_panel_plugin (child->tasklist);
      if (G_LIKELY (panel_plugin != NULL))
        gtk_widget_event (panel_plugin, (GdkEvent *) event);

      return TRUE;
    }

  if (event->button == 3)
    {
      menu = wnck_action_menu_new (child->window);
      g_signal_connect (G_OBJECT (menu), "selection-done",
          G_CALLBACK (gtk_widget_destroy), NULL);

      gtk_menu_attach_to_widget (GTK_MENU (menu), button, NULL);
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                      child->type == CHILD_TYPE_WINDOW ? xfce_panel_plugin_position_menu : NULL,
                      xfce_tasklist_get_panel_plugin (child->tasklist),
                      event->button,
                      event->time);

      return TRUE;
    }

  return FALSE;
}



static gboolean
xfce_tasklist_button_button_release_event (GtkWidget         *button,
                                           GdkEventButton    *event,
                                           XfceTasklistChild *child)
{
  panel_return_val_if_fail (XFCE_IS_TASKLIST (child->tasklist), FALSE);
  panel_return_val_if_fail (child->type != CHILD_TYPE_GROUP, FALSE);

  /* only respond to in-button events */
  if (event->type == GDK_BUTTON_RELEASE
      && !xfce_taskbar_is_locked (child->tasklist)
      && !(event->x == 0 && event->y == 0) /* 0,0 = outside the widget in Gtk */
      && event->x >= 0 && event->x < button->allocation.width
      && event->y >= 0 && event->y < button->allocation.height)
    {
      if (event->button == 1)
        {
          /* press the button */
          xfce_tasklist_button_activate (child, event->time);
          return FALSE;
        }
      else if (event->button == 2)
        {
          switch (child->tasklist->middle_click)
            {
            case XFCE_TASKLIST_MIDDLE_CLICK_NOTHING:
              break;

            case XFCE_TASKLIST_MIDDLE_CLICK_CLOSE_WINDOW:
              wnck_window_close (child->window, event->time);
              return TRUE;

            case XFCE_TASKLIST_MIDDLE_CLICK_MINIMIZE_WINDOW:
              if (!wnck_window_is_minimized (child->window))
                wnck_window_minimize (child->window);
              return FALSE;
            }
        }
    }

  return FALSE;
}



static void
xfce_tasklist_button_enter_notify_event_disconnected (gpointer  data,
                                                      GClosure *closure)
{
  XfceTasklistChild *child = data;

  panel_return_if_fail (WNCK_IS_WINDOW (child->window));

  /* we need to detach the geometry watch because that is connected
   * to the window we proxy and thus not disconnected when the
   * proxy dies */
  g_signal_handlers_disconnect_by_func (child->window,
      xfce_tasklist_button_geometry_changed, child);

  g_object_unref (G_OBJECT (child->window));
}



static GtkWidget *
xfce_tasklist_button_proxy_menu_item (XfceTasklistChild *child,
                                      gboolean           allow_wireframe)
{
  GtkWidget    *mi;
  GtkWidget    *image;
  GtkWidget    *label;
  XfceTasklist *tasklist = child->tasklist;

  panel_return_val_if_fail (XFCE_IS_TASKLIST (child->tasklist), NULL);
  panel_return_val_if_fail (child->type == CHILD_TYPE_OVERFLOW_MENU
                            || child->type == CHILD_TYPE_GROUP_MENU, NULL);
  panel_return_val_if_fail (GTK_IS_LABEL (child->label), NULL);
  panel_return_val_if_fail (WNCK_IS_WINDOW (child->window), NULL);

  mi = gtk_image_menu_item_new ();
  exo_binding_new (G_OBJECT (child->label), "label", G_OBJECT (mi), "label");
  exo_binding_new (G_OBJECT (child->label), "label", G_OBJECT (mi), "tooltip-text");

  label = gtk_bin_get_child (GTK_BIN (mi));
  panel_return_val_if_fail (GTK_IS_LABEL (label), NULL);
  gtk_label_set_max_width_chars (GTK_LABEL (label), tasklist->menu_max_width_chars);
  gtk_label_set_ellipsize (GTK_LABEL (label), tasklist->ellipsize_mode);

  if (G_LIKELY (tasklist->menu_icon_size > 0))
    {
      image = xfce_panel_image_new ();
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), image);
      xfce_panel_image_set_size (XFCE_PANEL_IMAGE (image), tasklist->menu_icon_size);
      exo_binding_new (G_OBJECT (child->icon), "pixbuf", G_OBJECT (image), "pixbuf");
      gtk_widget_show (image);
    }

  if (allow_wireframe)
    {
      g_object_ref (G_OBJECT (child->window));
      g_signal_connect_data (G_OBJECT (mi), "enter-notify-event",
          G_CALLBACK (xfce_tasklist_button_enter_notify_event), child,
          xfce_tasklist_button_enter_notify_event_disconnected, 0);
    }

  g_signal_connect (G_OBJECT (mi), "button-press-event",
      G_CALLBACK (xfce_tasklist_button_button_press_event), child);
  g_signal_connect (G_OBJECT (mi), "button-release-event",
      G_CALLBACK (xfce_tasklist_button_button_release_event), child);

  /* TODO bold labels for urgent windows */
  /* TODO item dnd */

  return mi;
}



static void
xfce_tasklist_button_activate (XfceTasklistChild *child,
                               guint32            timestamp)
{
  WnckWorkspace *workspace;
  gint           window_x, window_y;
  gint           workspace_width, workspace_height;
  gint           screen_width, screen_height;
  gint           viewport_x, viewport_y;

  panel_return_if_fail (XFCE_IS_TASKLIST (child->tasklist));
  panel_return_if_fail (WNCK_IS_WINDOW (child->window));
  panel_return_if_fail (WNCK_IS_SCREEN (child->tasklist->screen));

  if (wnck_window_is_active (child->window)
      || wnck_window_transient_is_most_recently_activated (child->window))
    {
      /* minimize does not work when this is assigned to the
       * middle mouse button */
      if (child->tasklist->middle_click != XFCE_TASKLIST_MIDDLE_CLICK_MINIMIZE_WINDOW)
        wnck_window_minimize (child->window);
    }
  else
    {
      /* we only change worksapces/viewports for non-pinned windows
       * and if all workspaces/viewports are shown or if we have
       * all blinking enabled and the current button is blinking */
      if ((child->tasklist->all_workspaces
          && !wnck_window_is_pinned (child->window))
          || (child->tasklist->all_blinking
              && xfce_arrow_button_get_blinking (XFCE_ARROW_BUTTON (child->button))))
        {
          workspace = wnck_window_get_workspace (child->window);

          /* only switch workspaces/viewports if switch_workspace is enabled or
           * we want to restore a minimized window to the current workspace/viewport */
          if (workspace != NULL
              && (child->tasklist->switch_workspace
                  || !wnck_window_is_minimized (child->window)))
            {
              if (G_UNLIKELY (wnck_workspace_is_virtual (workspace)))
                {
                  if (!wnck_window_is_in_viewport (child->window, workspace))
                    {
                      /* viewport info */
                      workspace_width = wnck_workspace_get_width (workspace);
                      workspace_height = wnck_workspace_get_height (workspace);
                      screen_width = wnck_screen_get_width (child->tasklist->screen);
                      screen_height = wnck_screen_get_height (child->tasklist->screen);

                      /* we only support multiple viewports like compiz has
                       * (all equally spread across the screen) */
                      if ((workspace_width % screen_width) == 0
                          && (workspace_height % screen_height) == 0)
                        {
                          wnck_window_get_geometry (child->window, &window_x, &window_y, NULL, NULL);

                          /* lookup nearest workspace edge */
                          viewport_x = window_x - (window_x % screen_width);
                          viewport_x = CLAMP (viewport_x, 0, workspace_width - screen_width);

                          viewport_y = window_y - (window_y % screen_height);
                          viewport_y = CLAMP (viewport_y, 0, workspace_height - screen_height);

                          /* move to the other viewport */
                          wnck_screen_move_viewport (child->tasklist->screen, viewport_x, viewport_y);
                        }
                      else
                        {
                          g_warning ("only viewport with equally distributed screens are supported: %dx%d & %dx%d",
                                     workspace_width, workspace_height, screen_width, screen_height);
                        }
                    }
                }
              else if (wnck_screen_get_active_workspace (child->tasklist->screen) != workspace)
                {
                  /* switch to the other workspace before we activate the window */
                  wnck_workspace_activate (workspace, timestamp);
                  gtk_main_iteration ();
                }
            }
          else if (workspace != NULL
                   && wnck_workspace_is_virtual (workspace)
                   && !wnck_window_is_in_viewport (child->window, workspace))
            {
              /* viewport info */
              workspace_width = wnck_workspace_get_width (workspace);
              workspace_height = wnck_workspace_get_height (workspace);
              screen_width = wnck_screen_get_width (child->tasklist->screen);
              screen_height = wnck_screen_get_height (child->tasklist->screen);

              /* we only support multiple viewports like compiz has
               * (all equaly spread across the screen) */
              if ((workspace_width % screen_width) == 0
                  && (workspace_height % screen_height) == 0)
                {
                  viewport_x = wnck_workspace_get_viewport_x (workspace);
                  viewport_y = wnck_workspace_get_viewport_y (workspace);

                  /* note that the x and y might be negative numbers, since they are relative
                   * to the current screen, not to the edge of the screen they are on. this is
                   * not a problem since the mod result will always be positive */
                  wnck_window_get_geometry (child->window, &window_x, &window_y, NULL, NULL);

                  /* get the new screen position, with the same screen offset */
                  window_x = viewport_x + (window_x % screen_width);
                  window_y = viewport_y + (window_y % screen_height);

                  /* move the window */
                  wnck_window_set_geometry (child->window,
                                            WNCK_WINDOW_GRAVITY_CURRENT,
                                            WNCK_WINDOW_CHANGE_X | WNCK_WINDOW_CHANGE_Y,
                                            window_x, window_y, -1, -1);
                }
              else
                {
                  g_warning ("only viewport with equally distributed screens are supported: %dx%d & %dx%d",
                             workspace_width, workspace_height, screen_width, screen_height);
                }
            }
        }

      wnck_window_activate_transient (child->window, timestamp);
    }
}



static void
xfce_tasklist_button_drag_data_get (GtkWidget         *button,
                                    GdkDragContext    *context,
                                    GtkSelectionData  *selection_data,
                                    guint              info,
                                    guint              timestamp,
                                    XfceTasklistChild *child)
{
  gulong xid;

  panel_return_if_fail (WNCK_IS_WINDOW (child->window));

  xid = wnck_window_get_xid (child->window);
  gtk_selection_data_set (selection_data,
                          gtk_selection_data_get_target (selection_data),
                          8, (guchar *)&xid, sizeof (gulong));
}



static void
xfce_tasklist_button_drag_begin (GtkWidget         *button,
                                 GdkDragContext    *context,
                                 XfceTasklistChild *child)
{
  GdkPixbuf *pixbuf;
  GdkPixmap *pixmap;

  panel_return_if_fail (WNCK_IS_WINDOW (child->window));

  if (child->tasklist->show_labels)
    {
      pixmap = gtk_widget_get_snapshot (button, NULL);
      if (pixmap != NULL)
        {
          gtk_drag_set_icon_pixmap (context,
              gdk_drawable_get_colormap (GDK_DRAWABLE (pixmap)),
              pixmap, NULL, 0, 0);
          g_object_unref (G_OBJECT (pixmap));

          return;
        }
    }

  pixbuf = wnck_window_get_icon (child->window);
  if (G_LIKELY (pixbuf != NULL))
    gtk_drag_set_icon_pixbuf (context, pixbuf, 0, 0);
}



static void
xfce_tasklist_button_drag_data_received (GtkWidget         *button,
                                         GdkDragContext    *context,
                                         gint               x,
                                         gint               y,
                                         GtkSelectionData  *selection_data,
                                         guint              info,
                                         guint              drag_time,
                                         XfceTasklistChild *child2)
{
  GList             *li, *sibling;
  gulong             xid;
  XfceTasklistChild *child;
  XfceTasklist      *tasklist = XFCE_TASKLIST (child2->tasklist);

  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));

  if (tasklist->sort_order != XFCE_TASKLIST_SORT_ORDER_DND)
    return;

  sibling = g_list_find (tasklist->windows, child2);
  panel_return_if_fail (sibling != NULL);

  if ((!xfce_tasklist_vertical (tasklist) && x >= button->allocation.width / 2)
      || (xfce_tasklist_vertical (tasklist) && y >= button->allocation.height / 2))
    sibling = g_list_next (sibling);

  xid = *((gulong *) gtk_selection_data_get_data (selection_data));
  for (li = tasklist->windows; li != NULL; li = li->next)
    {
      child = li->data;

      if (sibling != li /* drop on end previous button */
          && child != child2 /* drop on the same button */
          && g_list_next (li) != sibling /* drop start of next button */
          && child->window != NULL
          && wnck_window_get_xid (child->window) == xid)
        {
          /* swap items */
          tasklist->windows = g_list_delete_link (tasklist->windows, li);
          tasklist->windows = g_list_insert_before (tasklist->windows, sibling, child);

          gtk_widget_queue_resize (GTK_WIDGET (tasklist));

          break;
        }
    }
}



static XfceTasklistChild *
xfce_tasklist_button_new (WnckWindow   *window,
                          XfceTasklist *tasklist)
{
  XfceTasklistChild *child;
  static guint       unique_id_counter = 0;

  panel_return_val_if_fail (XFCE_IS_TASKLIST (tasklist), NULL);
  panel_return_val_if_fail (WNCK_IS_WINDOW (window), NULL);

  /* avoid integer overflows */
  if (G_UNLIKELY (unique_id_counter >= G_MAXUINT))
    unique_id_counter = 0;

  child = xfce_tasklist_child_new (tasklist);
  child->type = CHILD_TYPE_WINDOW;
  child->window = window;
  child->class_group = wnck_window_get_class_group (window);
  child->unique_id = unique_id_counter++;

  /* drag and drop to the pager */
  gtk_drag_source_set (child->button, GDK_BUTTON1_MASK,
                       source_targets, G_N_ELEMENTS (source_targets),
                       GDK_ACTION_MOVE);
  gtk_drag_dest_set (child->button, GTK_DEST_DEFAULT_DROP,
                     source_targets, G_N_ELEMENTS (source_targets),
                     GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (child->button), "drag-data-get",
      G_CALLBACK (xfce_tasklist_button_drag_data_get), child);
  g_signal_connect (G_OBJECT (child->button), "drag-begin",
      G_CALLBACK (xfce_tasklist_button_drag_begin), child);
  g_signal_connect (G_OBJECT (child->button), "drag-data-received",
      G_CALLBACK (xfce_tasklist_button_drag_data_received), child);

  /* note that the same signals should be in the proxy menu item too */
  g_signal_connect (G_OBJECT (child->button), "enter-notify-event",
      G_CALLBACK (xfce_tasklist_button_enter_notify_event), child);
  g_signal_connect (G_OBJECT (child->button), "button-press-event",
      G_CALLBACK (xfce_tasklist_button_button_press_event), child);
  g_signal_connect (G_OBJECT (child->button), "button-release-event",
      G_CALLBACK (xfce_tasklist_button_button_release_event), child);

  /* monitor window changes */
  g_signal_connect (G_OBJECT (window), "icon-changed",
      G_CALLBACK (xfce_tasklist_button_icon_changed), child);
  g_signal_connect (G_OBJECT (window), "name-changed",
      G_CALLBACK (xfce_tasklist_button_name_changed), child);
  g_signal_connect (G_OBJECT (window), "state-changed",
      G_CALLBACK (xfce_tasklist_button_state_changed), child);
  g_signal_connect (G_OBJECT (window), "workspace-changed",
      G_CALLBACK (xfce_tasklist_button_workspace_changed), child);
  g_signal_connect (G_OBJECT (window), "geometry-changed",
      G_CALLBACK (xfce_tasklist_button_geometry_changed2), child);

  /* poke functions */
  xfce_tasklist_button_icon_changed (window, child);
  xfce_tasklist_button_name_changed (NULL, child);

  /* insert */
  tasklist->windows = g_list_insert_sorted_with_data (tasklist->windows, child,
                                                      xfce_tasklist_button_compare,
                                                      tasklist);

  return child;
}



/**
 * Group Buttons
 **/
static void
xfce_tasklist_group_button_menu_minimize_all (XfceTasklistChild *group_child)
{
  GSList            *li;
  XfceTasklistChild *child;

  panel_return_if_fail (group_child->type == CHILD_TYPE_GROUP);
  panel_return_if_fail (WNCK_IS_CLASS_GROUP (group_child->class_group));

  for (li = group_child->windows; li != NULL; li = li->next)
    {
      child = li->data;
      if (GTK_WIDGET_VISIBLE (child->button)
          && child->type == CHILD_TYPE_GROUP_MENU)
        {
          panel_return_if_fail (WNCK_IS_WINDOW (child->window));
          wnck_window_minimize (child->window);
        }
    }
}



static void
xfce_tasklist_group_button_menu_unminimize_all (XfceTasklistChild *group_child)
{
  GSList            *li;
  XfceTasklistChild *child;

  panel_return_if_fail (group_child->type == CHILD_TYPE_GROUP);
  panel_return_if_fail (WNCK_IS_CLASS_GROUP (group_child->class_group));

  for (li = group_child->windows; li != NULL; li = li->next)
    {
      child = li->data;
      if (GTK_WIDGET_VISIBLE (child->button)
          && child->type == CHILD_TYPE_GROUP_MENU)
        {
          panel_return_if_fail (WNCK_IS_WINDOW (child->window));
          wnck_window_unminimize (child->window, gtk_get_current_event_time ());
        }
    }
}



static void
xfce_tasklist_group_button_menu_maximize_all (XfceTasklistChild *group_child)
{
  GSList            *li;
  XfceTasklistChild *child;

  panel_return_if_fail (group_child->type == CHILD_TYPE_GROUP);
  panel_return_if_fail (WNCK_IS_CLASS_GROUP (group_child->class_group));

  for (li = group_child->windows; li != NULL; li = li->next)
    {
      child = li->data;
      if (GTK_WIDGET_VISIBLE (child->button)
          && child->type == CHILD_TYPE_GROUP_MENU)
        {
          panel_return_if_fail (WNCK_IS_WINDOW (child->window));
          wnck_window_maximize (child->window);
        }
    }
}



static void
xfce_tasklist_group_button_menu_unmaximize_all (XfceTasklistChild *group_child)
{
  GSList            *li;
  XfceTasklistChild *child;

  panel_return_if_fail (group_child->type == CHILD_TYPE_GROUP);
  panel_return_if_fail (WNCK_IS_CLASS_GROUP (group_child->class_group));

  for (li = group_child->windows; li != NULL; li = li->next)
    {
      child = li->data;
      if (GTK_WIDGET_VISIBLE (child->button)
          && child->type == CHILD_TYPE_GROUP_MENU)
        {
          panel_return_if_fail (WNCK_IS_WINDOW (child->window));
          wnck_window_unmaximize (child->window);
        }
    }
}



static void
xfce_tasklist_group_button_menu_close_all (XfceTasklistChild *group_child)
{
  GSList            *li;
  XfceTasklistChild *child;

  panel_return_if_fail (WNCK_IS_CLASS_GROUP (group_child->class_group));

  for (li = group_child->windows; li != NULL; li = li->next)
    {
      child = li->data;
      if (GTK_WIDGET_VISIBLE (child->button)
          && child->type == CHILD_TYPE_GROUP_MENU)
        {
          panel_return_if_fail (WNCK_IS_WINDOW (child->window));
          wnck_window_close (child->window, gtk_get_current_event_time ());
        }
    }
}



static GtkWidget *
xfce_tasklist_group_button_menu (XfceTasklistChild *group_child,
                                 gboolean           action_menu_entries)
{
  GSList            *li;
  XfceTasklistChild *child;
  GtkWidget         *mi;
  GtkWidget         *menu;
  GtkWidget         *image;

  panel_return_val_if_fail (XFCE_IS_TASKLIST (group_child->tasklist), NULL);
  panel_return_val_if_fail (WNCK_IS_CLASS_GROUP (group_child->class_group), NULL);

  menu = gtk_menu_new ();

  for (li = group_child->windows; li != NULL; li = li->next)
    {
      child = li->data;
      if (GTK_WIDGET_VISIBLE (child->button)
          && child->type == CHILD_TYPE_GROUP_MENU)
        {
          mi = xfce_tasklist_button_proxy_menu_item (child, !action_menu_entries);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
          gtk_widget_show (mi);

          if (action_menu_entries)
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi),
                wnck_action_menu_new (child->window));
        }
    }

  if (action_menu_entries)
    {
      mi = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_show (mi);

      mi = gtk_image_menu_item_new_with_mnemonic (_("Mi_nimize All"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect_swapped (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_tasklist_group_button_menu_minimize_all), group_child);
      gtk_widget_show (mi);
      image = gtk_image_new_from_stock ("wnck-stock-minimize", GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), image);
      gtk_widget_show (image);

      mi =  gtk_image_menu_item_new_with_mnemonic (_("Un_minimize All"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect_swapped (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_tasklist_group_button_menu_unminimize_all), group_child);
      gtk_widget_show (mi);

      mi = gtk_image_menu_item_new_with_mnemonic (_("Ma_ximize All"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect_swapped (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_tasklist_group_button_menu_maximize_all), group_child);
      gtk_widget_show (mi);
      image = gtk_image_new_from_stock ("wnck-stock-maximize", GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), image);
      gtk_widget_show (image);

      mi =  gtk_image_menu_item_new_with_mnemonic (_("_Unmaximize All"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect_swapped (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_tasklist_group_button_menu_unmaximize_all), group_child);
      gtk_widget_show (mi);

      mi = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_show (mi);

      mi = gtk_image_menu_item_new_with_mnemonic(_("_Close All"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect_swapped (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_tasklist_group_button_menu_close_all), group_child);
      gtk_widget_show (mi);

      image = gtk_image_new_from_stock ("wnck-stock-delete", GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), image);
      gtk_widget_show (image);
    }

  return menu;
}



static void
xfce_tasklist_group_button_menu_destroy (GtkWidget         *menu,
                                         XfceTasklistChild *group_child)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (group_child->tasklist));
  panel_return_if_fail (GTK_IS_TOGGLE_BUTTON (group_child->button));
  panel_return_if_fail (GTK_IS_WIDGET (menu));

  gtk_widget_destroy (menu);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (group_child->button), FALSE);

#ifdef GDK_WINDOWING_X11
  /* make sure the wireframe is hidden */
  xfce_tasklist_wireframe_hide (group_child->tasklist);
#endif
}



static gboolean
xfce_tasklist_group_button_button_press_event (GtkWidget         *button,
                                               GdkEventButton    *event,
                                               XfceTasklistChild *group_child)
{
  GtkWidget *panel_plugin;
  GtkWidget *menu;

  panel_return_val_if_fail (XFCE_IS_TASKLIST (group_child->tasklist), FALSE);
  panel_return_val_if_fail (group_child->type == CHILD_TYPE_GROUP, FALSE);

  if (event->type != GDK_BUTTON_PRESS
      || xfce_taskbar_is_locked (group_child->tasklist))
    return FALSE;

  /* send the event to the panel plugin if control is pressed */
  if (PANEL_HAS_FLAG (event->state, GDK_CONTROL_MASK))
    {
      /* send the event to the panel plugin */
      panel_plugin = xfce_tasklist_get_panel_plugin (group_child->tasklist);
      if (G_LIKELY (panel_plugin != NULL))
        gtk_widget_event (panel_plugin, (GdkEvent *) event);

      return TRUE;
    }

  if (event->button == 1 || event->button == 3)
    {
      menu = xfce_tasklist_group_button_menu (group_child, event->button == 3);
      g_signal_connect (G_OBJECT (menu), "selection-done",
          G_CALLBACK (xfce_tasklist_group_button_menu_destroy), group_child);

      gtk_menu_attach_to_widget (GTK_MENU (menu), button, NULL);
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                      xfce_panel_plugin_position_menu,
                      xfce_tasklist_get_panel_plugin (group_child->tasklist),
                      event->button,
                      event->time);

      return TRUE;
    }

  return FALSE;
}



static void
xfce_tasklist_group_button_name_changed (WnckClassGroup    *class_group,
                                         XfceTasklistChild *group_child)
{
  const gchar       *name;
  gchar             *label;
  guint              n_windows;
  GSList            *li;
  XfceTasklistChild *child;

  panel_return_if_fail (class_group == NULL || group_child->class_group == class_group);
  panel_return_if_fail (XFCE_IS_TASKLIST (group_child->tasklist));
  panel_return_if_fail (WNCK_IS_CLASS_GROUP (group_child->class_group));

  /* count number of windows in the menu */
  for (li = group_child->windows, n_windows = 0; li != NULL; li = li->next)
    {
      child = li->data;
      if (GTK_WIDGET_VISIBLE (child->button)
          && child->type == CHILD_TYPE_GROUP_MENU)
        n_windows++;
    }

  /* create the button label */
  name = wnck_class_group_get_name (group_child->class_group);
  if (!exo_str_is_empty (name))
    label = g_strdup_printf ("%s (%d)", name, n_windows);
  else
    label = g_strdup_printf ("(%d)", n_windows);
  gtk_label_set_text (GTK_LABEL (group_child->label), label);
  g_free (label);

  /* don't sort if there is no need to update the sorting (ie. only number
   * of windows is changed or button is not inserted in the tasklist yet */
  if (class_group != NULL)
    xfce_tasklist_sort (group_child->tasklist);
}



static void
xfce_tasklist_group_button_icon_changed (WnckClassGroup    *class_group,
                                         XfceTasklistChild *group_child)
{
  GdkPixbuf *pixbuf;

  panel_return_if_fail (XFCE_IS_TASKLIST (group_child->tasklist));
  panel_return_if_fail (WNCK_IS_CLASS_GROUP (class_group));
  panel_return_if_fail (group_child->class_group == class_group);
  panel_return_if_fail (XFCE_IS_PANEL_IMAGE (group_child->icon));

  /* 0 means icons are disabled, although the grouping button does
   * not use lucient icons */
  if (group_child->tasklist->minimized_icon_lucency == 0)
    return;

  /* get the class group icon */
  if (group_child->tasklist->show_labels)
    pixbuf = wnck_class_group_get_mini_icon (class_group);
  else
    pixbuf = wnck_class_group_get_icon (class_group);

  if (G_LIKELY (pixbuf != NULL))
    xfce_panel_image_set_from_pixbuf (XFCE_PANEL_IMAGE (group_child->icon), pixbuf);
  else
    xfce_panel_image_clear (XFCE_PANEL_IMAGE (group_child->icon));
}



static void
xfce_tasklist_group_button_remove (XfceTasklistChild *group_child)
{
  GSList            *li;
  guint              n;
  XfceTasklistChild *child;

  /* leave if hash table triggers this function where no group
   * child is set */
  if (group_child == NULL)
    return;

  panel_return_if_fail (XFCE_IS_TASKLIST (group_child->tasklist));
  panel_return_if_fail (WNCK_IS_CLASS_GROUP (group_child->class_group));
  panel_return_if_fail (group_child->type == CHILD_TYPE_GROUP);
  panel_return_if_fail (g_list_find (group_child->tasklist->windows, group_child) != NULL);

  /* disconnect from all the group watch functions */
  n = g_signal_handlers_disconnect_matched (G_OBJECT (group_child->class_group),
      G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, group_child);
  panel_return_if_fail (n == 2);

  /* disconnect from visible windows */
  for (li = group_child->windows; li != NULL; li = li->next)
    {
      child = li->data;
      panel_return_if_fail (GTK_IS_BUTTON (child->button));
      n = g_signal_handlers_disconnect_matched (G_OBJECT (child->button),
          G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, group_child);
      panel_return_if_fail (n == 2);
    }

  g_slist_free (group_child->windows);
  group_child->windows = NULL;

  /* destroy the button, this will free the remaining child
   * data in the container remove function */
  gtk_widget_destroy (group_child->button);
}



static void
xfce_tasklist_group_button_child_visible_changed (XfceTasklistChild *group_child)
{
  XfceTasklistChild    *child;
  GSList               *li;
  gint                  visible_counter = 0;
  XfceTasklistChildType type;

  panel_return_if_fail (group_child->type == CHILD_TYPE_GROUP);
  panel_return_if_fail (WNCK_IS_CLASS_GROUP (group_child->class_group));
  panel_return_if_fail (XFCE_IS_TASKLIST (group_child->tasklist));
  panel_return_if_fail (group_child->tasklist->grouping != XFCE_TASKLIST_GROUPING_NEVER);

  for (li = group_child->windows; li != NULL; li = li->next)
    {
      child = li->data;
      if (GTK_WIDGET_VISIBLE (child->button))
        visible_counter++;
    }

  if (visible_counter > 1)
    {
      /* show the button and take the windows */
      gtk_widget_show (group_child->button);
      type = CHILD_TYPE_GROUP_MENU;
    }
  else
    {
      /* hide the button and ungroup the buttons */
      gtk_widget_hide (group_child->button);
      type = CHILD_TYPE_WINDOW;
    }

  for (li = group_child->windows; li != NULL; li = li->next)
    {
      child = li->data;
      if (GTK_WIDGET_VISIBLE (child->button))
        child->type = type;
    }

  gtk_widget_queue_resize (GTK_WIDGET (group_child->tasklist));

  xfce_tasklist_group_button_name_changed (NULL, group_child);
}



static void
xfce_tasklist_group_button_child_destroyed (XfceTasklistChild *group_child,
                                            GtkWidget         *child_button)
{
  GSList            *li, *lnext;
  XfceTasklistChild *child;
  guint              n_children;

  panel_return_if_fail (group_child->type == CHILD_TYPE_GROUP);
  panel_return_if_fail (GTK_IS_BUTTON (child_button));
  panel_return_if_fail (group_child->windows != NULL);
  panel_return_if_fail (XFCE_IS_TASKLIST (group_child->tasklist));
  panel_return_if_fail (WNCK_IS_CLASS_GROUP (group_child->class_group));

  for (li = group_child->windows, n_children = 0; li != NULL; li = lnext)
    {
      child = li->data;
      lnext = li->next;
      if (G_UNLIKELY (child->button == child_button))
        group_child->windows = g_slist_delete_link (group_child->windows, li);
      else
        n_children++;
    }

  if ((group_child->tasklist->grouping == XFCE_TASKLIST_GROUPING_ALWAYS
       && n_children > 0))
#if 0
      || (group_child->tasklist->grouping == XFCE_TASKLIST_GROUPING_AUTO
          && n_children > 1))
#endif
    {
      xfce_tasklist_group_button_child_visible_changed (group_child);
      xfce_tasklist_group_button_name_changed (NULL, group_child);
    }
  else
    {
      /* self destroy */
      g_object_ref (G_OBJECT (group_child->class_group));
      g_hash_table_replace (group_child->tasklist->class_groups,
                            group_child->class_group, NULL);
    }
}



static void
xfce_tasklist_group_button_add_window (XfceTasklistChild *group_child,
                                       XfceTasklistChild *window_child)
{
  panel_return_if_fail (group_child->type == CHILD_TYPE_GROUP);
  panel_return_if_fail (window_child->type != CHILD_TYPE_GROUP);
  panel_return_if_fail (WNCK_IS_CLASS_GROUP (group_child->class_group));
  panel_return_if_fail (WNCK_IS_WINDOW (window_child->window));
  panel_return_if_fail (window_child->class_group == group_child->class_group);
  panel_return_if_fail (XFCE_IS_TASKLIST (group_child->tasklist));
  panel_return_if_fail (g_slist_find (group_child->windows, window_child) == NULL);

  /* watch child visibility changes */
  g_signal_connect_swapped (G_OBJECT (window_child->button), "notify::visible",
      G_CALLBACK (xfce_tasklist_group_button_child_visible_changed), group_child);
  g_signal_connect_swapped (G_OBJECT (window_child->button), "destroy",
      G_CALLBACK (xfce_tasklist_group_button_child_destroyed), group_child);

  /* add to internal list */
  group_child->windows = g_slist_prepend (group_child->windows, window_child);

  /* update visibility */
  xfce_tasklist_group_button_child_visible_changed (group_child);
}



static XfceTasklistChild *
xfce_tasklist_group_button_new (WnckClassGroup *class_group,
                                XfceTasklist   *tasklist)
{
  XfceTasklistChild *child;

  panel_return_val_if_fail (XFCE_IS_TASKLIST (tasklist), NULL);
  panel_return_val_if_fail (WNCK_IS_CLASS_GROUP (class_group), NULL);

  child = xfce_tasklist_child_new (tasklist);
  child->type = CHILD_TYPE_GROUP;
  child->class_group = class_group;

  /* note that the same signals should be in the proxy menu item too */
  g_signal_connect (G_OBJECT (child->button), "button-press-event",
      G_CALLBACK (xfce_tasklist_group_button_button_press_event), child);

  /* monitor class group changes */
  g_signal_connect (G_OBJECT (class_group), "icon-changed",
      G_CALLBACK (xfce_tasklist_group_button_icon_changed), child);
  g_signal_connect (G_OBJECT (class_group), "name-changed",
      G_CALLBACK (xfce_tasklist_group_button_name_changed), child);

  /* poke functions */
  xfce_tasklist_group_button_icon_changed (class_group, child);
  xfce_tasklist_group_button_name_changed (NULL, child);

  /* insert */
  tasklist->windows = g_list_insert_sorted_with_data (tasklist->windows, child,
                                                      xfce_tasklist_button_compare,
                                                      tasklist);

  return child;
}



/**
 * Potential Public Functions
 **/
static void
xfce_tasklist_set_include_all_workspaces (XfceTasklist *tasklist,
                                          gboolean      all_workspaces)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));

  all_workspaces = !!all_workspaces;

  if (tasklist->all_workspaces != all_workspaces)
    {
      tasklist->all_workspaces = all_workspaces;

      if (tasklist->screen != NULL)
        {
          /* update visibility of buttons */
          xfce_tasklist_active_workspace_changed (tasklist->screen,
                                                  NULL, tasklist);

          /* make sure sorting is ok */
          xfce_tasklist_sort (tasklist);
        }
    }
}



static void
xfce_tasklist_set_include_all_monitors (XfceTasklist *tasklist,
                                        gboolean      all_monitors)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));

  all_monitors = !!all_monitors;

  if (tasklist->all_monitors != all_monitors)
    {
      tasklist->all_monitors = all_monitors;

      /* set the geometry to invalid or update the geometry and
       * update the visibility of the buttons */
      if (all_monitors)
        {
          xfce_tasklist_geometry_set_invalid (tasklist);

          /* update visibility of buttons */
          xfce_tasklist_active_workspace_changed (tasklist->screen,
                                                  NULL, tasklist);
        }
      else if (tasklist->gdk_screen != NULL)
        {
          xfce_tasklist_gdk_screen_changed (tasklist->gdk_screen, tasklist);
        }
    }
}



static void
xfce_tasklist_set_button_relief (XfceTasklist   *tasklist,
                                 GtkReliefStyle  button_relief)
{
  GList             *li;
  XfceTasklistChild *child;

  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));

  if (tasklist->button_relief != button_relief)
    {
      tasklist->button_relief = button_relief;

      /* change the relief of all buttons in the list */
      for (li = tasklist->windows; li != NULL; li = li->next)
        {
          child = li->data;
          gtk_button_set_relief (GTK_BUTTON (child->button),
                                 button_relief);
        }

      /* arrow button for overflow menu */
      gtk_button_set_relief (GTK_BUTTON (tasklist->arrow_button),
                             button_relief);
    }
}



static void
xfce_tasklist_set_show_labels (XfceTasklist *tasklist,
                               gboolean      show_labels)
{
  GList             *li;
  XfceTasklistChild *child;

  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));

  show_labels = !!show_labels;

  if (tasklist->show_labels != show_labels)
    {
      tasklist->show_labels = show_labels;

      /* change the mode of all the buttons */
      for (li = tasklist->windows; li != NULL; li = li->next)
        {
          child = li->data;

          /* show or hide the label */
          if (show_labels)
            {
              gtk_widget_show (child->label);
              gtk_box_set_child_packing (GTK_BOX (child->box),
                                         child->icon,
                                         FALSE, FALSE, 0,
                                         GTK_PACK_START);
            }
          else
            {
              gtk_widget_hide (child->label);
              gtk_box_set_child_packing (GTK_BOX (child->box),
                                         child->icon,
                                         TRUE, TRUE, 0,
                                         GTK_PACK_START);
            }

          /* update the icon (we use another size for
           * icon box mode) */
          xfce_tasklist_button_icon_changed (child->window, child);
        }
    }
}



static void
xfce_tasklist_set_show_only_minimized (XfceTasklist *tasklist,
                                       gboolean      only_minimized)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));

  only_minimized = !!only_minimized;

  if (tasklist->only_minimized != only_minimized)
    {
      tasklist->only_minimized = only_minimized;

      /* update all windows */
      if (tasklist->screen != NULL)
        xfce_tasklist_active_workspace_changed (tasklist->screen,
                                                NULL, tasklist);
    }
}



static void
xfce_tasklist_set_show_wireframes (XfceTasklist *tasklist,
                                   gboolean      show_wireframes)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));

  tasklist->show_wireframes = !!show_wireframes;

#ifdef GDK_WINDOWING_X11
  /* destroy the window if needed */
  xfce_tasklist_wireframe_destroy (tasklist);
#endif
}



static void
xfce_tasklist_set_grouping (XfceTasklist         *tasklist,
                            XfceTasklistGrouping  grouping)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));

  /* TODO avoid overflow, because we allows + 1 in the object */
  if (grouping > XFCE_TASKLIST_GROUPING_MAX)
    grouping = XFCE_TASKLIST_GROUPING_MAX;

  if (tasklist->grouping != grouping)
    {
      tasklist->grouping = grouping;

      if (tasklist->screen != NULL)
        {
          xfce_tasklist_disconnect_screen (tasklist);
          xfce_tasklist_connect_screen (tasklist);
        }
    }
}


static void
xfce_tasklist_update_orientation (XfceTasklist *tasklist)
{
  gboolean           horizontal;
  GList             *li;
  XfceTasklistChild *child;

  horizontal = !xfce_tasklist_vertical (tasklist);

  /* update the tasklist */
  for (li = tasklist->windows; li != NULL; li = li->next)
    {
      child = li->data;

      /* update task box */
      xfce_hvbox_set_orientation (XFCE_HVBOX (child->box),
          horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

      /* update the label */
      if (horizontal)
        {
          /* gtk_box_reorder_child (GTK_BOX (child->box), child->icon, 0); */
          gtk_misc_set_alignment (GTK_MISC (child->label), 0.0, 0.5);
          gtk_label_set_angle (GTK_LABEL (child->label), 0);
          gtk_label_set_ellipsize (GTK_LABEL (child->label),
                                   child->tasklist->ellipsize_mode);
        }
      else
        {
          /* gtk_box_reorder_child (GTK_BOX (child->box), child->icon, -1); */
          gtk_misc_set_alignment (GTK_MISC (child->label), 0.50, 0.00);
          gtk_label_set_angle (GTK_LABEL (child->label), 270);
          gtk_label_set_ellipsize (GTK_LABEL (child->label), PANGO_ELLIPSIZE_NONE);
        }
    }

  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
}



void
xfce_tasklist_set_nrows (XfceTasklist *tasklist,
                         gint          nrows)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));
  panel_return_if_fail (nrows >= 1);

  if (tasklist->nrows != nrows)
    {
      tasklist->nrows = nrows;
      gtk_widget_queue_resize (GTK_WIDGET (tasklist));
    }
}



void
xfce_tasklist_set_mode (XfceTasklist        *tasklist,
                        XfcePanelPluginMode  mode)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));

  if (tasklist->mode != mode)
    {
      tasklist->mode = mode;
      xfce_tasklist_update_orientation (tasklist);
    }
}



void
xfce_tasklist_set_size (XfceTasklist *tasklist,
                        gint          size)
{
  panel_return_if_fail (XFCE_IS_TASKLIST (tasklist));

  if (tasklist->size != size)
    {
      tasklist->size = size;
      gtk_widget_queue_resize (GTK_WIDGET (tasklist));
    }
}



void
xfce_tasklist_update_monitor_geometry (XfceTasklist *tasklist)
{
  if (tasklist->update_monitor_geometry_id == 0)
    {
      tasklist->update_monitor_geometry_id = g_idle_add_full (G_PRIORITY_LOW, xfce_tasklist_update_monitor_geometry_idle,
                                                              tasklist, xfce_tasklist_update_monitor_geometry_idle_destroy);
    }
}

