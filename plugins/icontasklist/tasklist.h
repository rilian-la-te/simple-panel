/* tasklist object */
/* vim: set sw=2 et: */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003, 2005-2007 Vincent Untz
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ICON_TASKLIST_H
#define ICON_TASKLIST_H

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <gtk/gtk.h>
#include <libwnck/libwnck.h>

G_BEGIN_DECLS

#define ICON_TYPE_TASKLIST              (icon_tasklist_get_type ())
#define ICON_TASKLIST(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), ICON_TYPE_TASKLIST, IconTasklist))
#define ICON_TASKLIST_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), ICON_TYPE_TASKLIST, IconTasklistClass))
#define ICON_IS_TASKLIST(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), ICON_TYPE_TASKLIST))
#define ICON_IS_TASKLIST_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), ICON_TYPE_TASKLIST))
#define ICON_TASKLIST_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), ICON_TYPE_TASKLIST, IconTasklistClass))

typedef struct _IconTasklist        IconTasklist;
typedef struct _IconTasklistClass   IconTasklistClass;
typedef struct _IconTasklistPrivate IconTasklistPrivate;

/**
 * IconTasklist:
 *
 * The #IconTasklist struct contains only private fields and should not be
 * directly accessed.
 */
struct _IconTasklist
{
  GtkContainer parent_instance;

  IconTasklistPrivate *priv;
};

struct _IconTasklistClass
{
  GtkContainerClass parent_class;
  
  /* Padding for future expansion */
  void (* pad1) (void);
  void (* pad2) (void);
  void (* pad3) (void);
  void (* pad4) (void);
};

/**
 * IconTasklistGroupingType:
 * @ICON_TASKLIST_NEVER_GROUP: never group multiple #WnckWindow of the same
 * #WnckApplication.
 * @ICON_TASKLIST_AUTO_GROUP: group multiple #WnckWindow of the same
 * #WnckApplication for some #WnckApplication, when there is not enough place
 * to have a good-looking list of all #WnckWindow.
 * @ICON_TASKLIST_ALWAYS_GROUP: always group multiple #WnckWindow of the same
 * #WnckApplication, for all #WnckApplication.
 *
 * Type defining the policy of the #IconTasklist for grouping multiple
 * #WnckWindow of the same #WnckApplication.
 */
typedef enum {
  ICON_TASKLIST_NEVER_GROUP,
  ICON_TASKLIST_AUTO_GROUP,
  ICON_TASKLIST_ALWAYS_GROUP
} IconTasklistGroupingType;

GType icon_tasklist_get_type (void) G_GNUC_CONST;

GtkWidget *icon_tasklist_new (void);
const int *icon_tasklist_get_size_hint_list (IconTasklist  *tasklist,
					      int           *n_elements);

void icon_tasklist_set_grouping (IconTasklist             *tasklist,
                 IconTasklistGroupingType  grouping);
void icon_tasklist_set_switch_workspace_on_unminimize (IconTasklist  *tasklist,
						       gboolean       switch_workspace_on_unminimize);
void icon_tasklist_set_middle_click_close (IconTasklist  *tasklist,
					   gboolean       middle_click_close);
void icon_tasklist_set_grouping_limit (IconTasklist *tasklist,
				       gint          limit);
void icon_tasklist_set_include_all_workspaces (IconTasklist *tasklist,
					       gboolean      include_all_workspaces);
void icon_tasklist_set_button_relief (IconTasklist *tasklist,
                                      GtkReliefStyle relief);
void icon_tasklist_set_orientation (IconTasklist *tasklist,
                                    GtkOrientation orient);


void icon_tasklist_set_icon_loader (IconTasklist         *tasklist,
                                    WnckLoadIconFunction  load_icon_func,
                                    void                 *data,
                                    GDestroyNotify        free_data_func);

G_END_DECLS

#endif /* ICON_TASKLIST_H */
