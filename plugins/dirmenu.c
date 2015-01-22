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

#include <stdlib.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <string.h>

#include "misc.h"
#include "plugin.h"

#define DIRMENU_KEY_NAME "name"
#define DIRMENU_KEY_PATH "path"
#define DIRMENU_KEY_ICON "icon"

/* Temporary for sort of directory names. */
typedef struct _directory_name {
    struct _directory_name * flink;
    char * directory_name;
    char * directory_name_collate_key;
} DirectoryName;

/* Private context for directory menu plugin. */
typedef struct {
    SimplePanel * panel; /* The panel and settings are required to apply config */
    GSettings* settings;
    char * image;			/* Icon for top level widget */
    char * path;			/* Top level path for widget */
    char * name;			/* User's label for widget */
} DirMenuPlugin;

static GtkWidget * dirmenu_create_menu(DirMenuPlugin * dm, const char * path, gboolean open_at_top);
static void dirmenu_destructor(gpointer user_data);
static gboolean dirmenu_apply_configuration(gpointer user_data);


/* Handler for activate event on popup Open menu item. */
static void dirmenu_menuitem_open_directory(GtkWidget * item, DirMenuPlugin * dm)
{
    FmPath *path = fm_path_new_for_str(g_object_get_data(G_OBJECT(gtk_widget_get_parent(item)), "path"));
    lxpanel_launch_path(dm->panel, path);
    fm_path_unref(path);
}

/* Handler for activate event on popup Open In Terminal menu item. */
static void dirmenu_menuitem_open_in_terminal(GtkWidget * item, DirMenuPlugin * dm)
{
    fm_terminal_launch(g_object_get_data(G_OBJECT(gtk_widget_get_parent(item)), "path"), NULL);
}

/* Handler for select event on popup menu item. */
static void dirmenu_menuitem_select(GtkMenuItem * item, DirMenuPlugin * dm)
{
    GtkWidget * sub = gtk_menu_item_get_submenu(item);
    if (sub != NULL)
    {
        /* On first reference, populate the submenu using the parent directory and the item directory name. */
        GtkMenu * parent = GTK_MENU(gtk_widget_get_parent(GTK_WIDGET(item)));
        char * path = (char *) g_object_get_data(G_OBJECT(sub), "path");
        if (path == NULL)
        {
            path = g_build_filename(
                (char *) g_object_get_data(G_OBJECT(parent), "path"),
                (char *) g_object_get_data(G_OBJECT(item), "name"),
                NULL);
            sub = dirmenu_create_menu(dm, path, TRUE);
            g_free(path);
            gtk_menu_item_set_submenu(item, sub);
        }
    }
}

/* Handler for deselect event on popup menu item. */
static void dirmenu_menuitem_deselect(GtkMenuItem * item, DirMenuPlugin * dm)
{
    /* Delete old menu on deselect to save resource. */
    gtk_menu_item_set_submenu(item, gtk_menu_new());
}

/* Handler for selection-done event on popup menu. */
static void dirmenu_menu_selection_done(GtkWidget * menu, DirMenuPlugin * dm)
{
    gtk_widget_destroy(menu);
}

/* Position-calculation callback for popup menu. */
static void dirmenu_popup_set_position(GtkWidget * menu, gint * px, gint * py, gboolean * push_in, GtkWidget * p)
{
    DirMenuPlugin * dm = lxpanel_plugin_get_data(p);

    /* Determine the coordinates. */
    lxpanel_plugin_popup_set_position_helper(dm->panel, p, menu, px, py);
    *push_in = TRUE;
}

/* Create a menu populated with all subdirectories. */
static GtkWidget * dirmenu_create_menu(DirMenuPlugin * dm, const char * path, gboolean open_at_top)
{
    /* Create a menu. */
    GtkWidget * menu = gtk_menu_new();
    g_object_set_data_full(G_OBJECT(menu), "path", g_strdup(path), g_free);

    /* Scan the specified directory to populate the menu with its subdirectories. */
    DirectoryName * dir_list = NULL;
    GDir * dir = g_dir_open(path, 0, NULL);
    if (dir != NULL)
    {
        const char * name;
        while ((name = g_dir_read_name(dir)) != NULL)	/* Memory owned by glib */
        {
            /* Omit hidden files. */
            if (name[0] != '.')
            {
                char * full = g_build_filename(path, name, NULL);
                if (g_file_test(full, G_FILE_TEST_IS_DIR))
                {
                    /* Convert name to UTF-8 and to the collation key. */
                    char * directory_name = g_filename_display_name(name);
                    char * directory_name_collate_key = g_utf8_collate_key(directory_name, -1);

                    /* Locate insertion point. */
                    DirectoryName * dir_pred = NULL;
                    DirectoryName * dir_cursor;
                    for (dir_cursor = dir_list; dir_cursor != NULL; dir_pred = dir_cursor, dir_cursor = dir_cursor->flink)
                    {
                        if (strcmp(directory_name_collate_key, dir_cursor->directory_name_collate_key) <= 0)
                            break;
                    }

                    /* Allocate and initialize sorted directory name entry. */
                    dir_cursor = g_new0(DirectoryName, 1);
                    dir_cursor->directory_name = directory_name;
                    dir_cursor->directory_name_collate_key = directory_name_collate_key;
                    if (dir_pred == NULL)
                    {
                        dir_cursor->flink = dir_list;
                        dir_list = dir_cursor;
                    }
                    else
                    {
                        dir_cursor->flink = dir_pred->flink;
                        dir_pred->flink = dir_cursor;
                    }
                }
                g_free(full);
            }
        }
        g_dir_close(dir);
    }

    /* The sorted directory name list is complete.  Loop to create the menu. */
    DirectoryName * dir_cursor;
    while ((dir_cursor = dir_list) != NULL)
    {
        /* Create and initialize menu item. */
        GtkWidget * item = gtk_menu_item_new_with_label(dir_cursor->directory_name);
//        gtk_image_menu_item_set_image(
//            GTK_IMAGE_MENU_ITEM(item),
//            gtk_image_new_from_icon_name("folder", GTK_ICON_SIZE_MENU));
        GtkWidget * dummy = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), dummy);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

        /* Unlink and free sorted directory name element, but reuse the directory name string. */
        dir_list = dir_cursor->flink;
        g_object_set_data_full(G_OBJECT(item), "name", dir_cursor->directory_name, g_free);
        g_free(dir_cursor->directory_name_collate_key);
        g_free(dir_cursor);

        /* Connect signals. */
        g_signal_connect(G_OBJECT(item), "select", G_CALLBACK(dirmenu_menuitem_select), dm);
        g_signal_connect(G_OBJECT(item), "deselect", G_CALLBACK(dirmenu_menuitem_deselect), dm);
    }

    /* Create "Open" and "Open in Terminal" items. */
//    GtkWidget * item = gtk_image_menu_item_new_from_stock( GTK_STOCK_OPEN, NULL );
    GtkWidget * item = gtk_menu_item_new_with_mnemonic(  _("_Open") );
    g_signal_connect(item, "activate", G_CALLBACK(dirmenu_menuitem_open_directory), dm);
    GtkWidget * term = gtk_menu_item_new_with_mnemonic( _("Open in _Terminal") );
    g_signal_connect(term, "activate", G_CALLBACK(dirmenu_menuitem_open_in_terminal), dm);

    /* Insert or append based on caller's preference. */
    if (open_at_top)
    {
        gtk_menu_shell_insert(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new(), 0);
        gtk_menu_shell_insert(GTK_MENU_SHELL(menu), term, 0);
        gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, 0);
    }
    else {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), term);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }

    /* Show the menu and return. */
    gtk_widget_show_all(menu);
    return menu;
}

/* Show a menu of subdirectories. */
static void dirmenu_show_menu(GtkWidget * widget, DirMenuPlugin * dm, int btn, guint32 time)
{
    /* Create a menu populated with all subdirectories. */
    GtkWidget * menu = dirmenu_create_menu(dm, dm->path, FALSE);
    g_signal_connect(menu, "selection-done", G_CALLBACK(dirmenu_menu_selection_done), dm);

    /* Show the menu.  Use a positioning function to get it placed next to the top level widget. */
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, (GtkMenuPositionFunc) dirmenu_popup_set_position, widget, btn, time);
}

/* Handler for button-press-event on top level widget. */
static gboolean dirmenu_button_press_event(GtkWidget * widget, GdkEventButton * event, SimplePanel * p)
{
    DirMenuPlugin * dm = lxpanel_plugin_get_data(widget);

    if (event->button == 1)
    {
        dirmenu_show_menu(widget, dm, event->button, event->time);
    }
    else
    {
        fm_terminal_launch(dm->path, NULL);
    }
    return TRUE;
}

/* Plugin constructor. */
static GtkWidget *dirmenu_constructor(SimplePanel *panel, GSettings *settings)
{
    /* Allocate and initialize plugin context and set into Plugin private data pointer. */
    DirMenuPlugin * dm = g_new0(DirMenuPlugin, 1);
    GtkWidget * p;

    /* Load parameters from the configuration file. */
    dm->image = g_settings_get_string(settings,DIRMENU_KEY_ICON);
    g_settings_get(settings,DIRMENU_KEY_NAME,"ms",&dm->name);
    dm->path = expand_tilda(g_settings_get_string(settings,DIRMENU_KEY_PATH));
    /* Save construction pointers */
    dm->panel = panel;
    dm->settings = settings;

    /* Allocate top level widget and set into Plugin widget pointer.
     * It is not known why, but the button text will not draw if it is edited from empty to non-empty
     * unless this strategy of initializing it with a non-empty value first is followed. */

    p = simple_panel_button_new_for_icon(panel,
                            ((dm->image != NULL) ? dm->image : "file-manager"),
                            NULL, "Temp");
    lxpanel_plugin_set_data(p, dm, dirmenu_destructor);

    /* Initialize the widget. */
    dirmenu_apply_configuration(p);

    /* Show the widget and return. */
    return p;
}

/* Plugin destructor. */
static void dirmenu_destructor(gpointer user_data)
{
    DirMenuPlugin * dm = (DirMenuPlugin *)user_data;

    /* Deallocate all memory. */
    g_free(dm->image);
    g_free(dm->path);
    g_free(dm->name);
    g_free(dm);
}

/* Recursively apply a configuration change. */
static void dirmenu_apply_configuration_to_children(GtkWidget * w, DirMenuPlugin * dm)
{
    if (GTK_IS_CONTAINER(w))
        gtk_container_foreach(GTK_CONTAINER(w), (GtkCallback) dirmenu_apply_configuration_to_children, (gpointer) dm);
    else if (GTK_IS_LABEL(w))
    {
        gtk_label_set_text(GTK_LABEL(w), dm->name);
    }
}

/* Callback when the configuration dialog has recorded a configuration change. */
static gboolean dirmenu_apply_configuration(gpointer user_data)
{
    GtkWidget * p = user_data;
    DirMenuPlugin * dm = lxpanel_plugin_get_data(p);
    char * path = dm->path;

    /* Normalize path */
    if (path == NULL)
        dm->path = g_strdup(fm_get_home_dir());
    else if (path[0] == '~')
    {
        dm->path = expand_tilda(path);
        g_free(path);
    }

    /* Save configuration */
    g_settings_set_string(dm->settings,DIRMENU_KEY_PATH,dm->path);
    g_settings_set(dm->settings,DIRMENU_KEY_NAME,"ms",dm->name);
    g_settings_set_string(dm->settings,DIRMENU_KEY_ICON,dm->image);
    simple_panel_button_set_icon(p,((dm->image != NULL) ? dm->image : "file-manager"),dm->panel,-1);
    gtk_widget_set_tooltip_text(p, dm->path);
    gtk_container_foreach(GTK_CONTAINER(p), (GtkCallback) dirmenu_apply_configuration_to_children, (gpointer) dm);
    return FALSE;
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *dirmenu_configure(SimplePanel *panel, GtkWidget *p)
{
    DirMenuPlugin * dm = lxpanel_plugin_get_data(p);
    return lxpanel_generic_config_dlg(_("Directory Menu"),
        panel, dirmenu_apply_configuration, p,
        _("Directory"), &dm->path, CONF_TYPE_DIRECTORY_ENTRY,
        _("Label"), &dm->name, CONF_TYPE_STR,
        _("Icon"), &dm->image, CONF_TYPE_FILE_ENTRY,
        NULL);
}

/* Callback when panel configuration changes. */
static void dirmenu_panel_configuration_changed(SimplePanel *panel, GtkWidget *p)
{
    dirmenu_apply_configuration(p);
}

/* Plugin descriptor. */
SimplePanelPluginInit lxpanel_static_plugin_dirmenu = {
    .name = N_("Directory Menu"),
    .description = N_("Browse directory tree via menu (Author = PCMan)"),

    .new_instance = dirmenu_constructor,
    .config = dirmenu_configure,
    .has_config = TRUE,
    .reconfigure = dirmenu_panel_configuration_changed,
    .button_press_event = dirmenu_button_press_event
};
