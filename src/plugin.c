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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "private.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <string.h>
#include <stdlib.h>

#include "misc.h"
#include "css.h"

#include <glib-object.h>
#include <glib/gi18n.h>
#include <libfm/fm-gtk.h>

//#define DEBUG
#include "dbg.h"

static void plugin_class_unref(PluginClass * pc);

GQuark lxpanel_plugin_qinit;
GQuark lxpanel_plugin_qconf;
GQuark lxpanel_plugin_qdata;
GQuark lxpanel_plugin_qsize;
static GHashTable *_all_types = NULL;

static inline const SimplePanelPluginInit *_find_plugin(const char *name)
{
    return g_hash_table_lookup(_all_types, name);
}

static GtkWidget *_old_plugin_config(SimplePanel *panel, GtkWidget *instance)
{
    const SimplePanelPluginInit *init = PLUGIN_CLASS(instance);
    Plugin * plugin;

    g_return_val_if_fail(init != NULL && init->new_instance == NULL, NULL);
    plugin = lxpanel_plugin_get_data(instance);
    if (plugin->klass->config)
        plugin->klass->config(plugin, GTK_WINDOW(panel));
    return NULL;
}

static void _old_plugin_reconfigure(SimplePanel *panel, GtkWidget *instance)
{
    const SimplePanelPluginInit *init = PLUGIN_CLASS(instance);
    Plugin * plugin;

    g_return_if_fail(init != NULL && init->new_instance == NULL);
    plugin = lxpanel_plugin_get_data(instance);
    if (plugin->klass->panel_configuration_changed)
        plugin->klass->panel_configuration_changed(plugin);
}

/* Register a PluginClass. */
static void register_plugin_class(PluginClass * pc, gboolean dynamic)
{
    SimplePanelPluginInit *init = g_new0(SimplePanelPluginInit, 1);
    init->_reserved1 = pc;
    init->name = pc->name;
    init->description = pc->description;
    if (pc->config)
        init->config = _old_plugin_config;
    if (pc->panel_configuration_changed)
        init->reconfigure = _old_plugin_reconfigure;
    init->one_per_system = pc->one_per_system;
    init->expand_available = pc->expand_available;
    init->expand_default = pc->expand_default;
    pc->dynamic = dynamic;
    g_hash_table_insert(_all_types, g_strdup(pc->type), init);
}

/* Load a dynamic plugin. */
static void plugin_load_dynamic(const char * type, const gchar * path)
{
    PluginClass * pc = NULL;

    /* Load the external module. */
    GModule * m = g_module_open(path, G_MODULE_BIND_LAZY);
    if (m != NULL)
    {
        /* Formulate the name of the expected external variable of type PluginClass. */
        char class_name[128];
        g_snprintf(class_name, sizeof(class_name), "%s_plugin_class", type);

        /* Validate that the external variable is of type PluginClass. */
        gpointer tmpsym;
        if (( ! g_module_symbol(m, class_name, &tmpsym))	/* Ensure symbol is present */
        || ((pc = tmpsym) == NULL)
        || (pc->structure_size != sizeof(PluginClass))		/* Then check versioning information */
        || (pc->structure_version != PLUGINCLASS_VERSION)
        || (strcmp(type, pc->type) != 0))			/* Then and only then access other fields; check name */
        {
            g_module_close(m);
            g_warning("%s.so is not a lxpanel plugin", type);
            return;
        }

        /* Register the newly loaded and valid plugin. */
        pc->gmodule = m;
        register_plugin_class(pc, TRUE);
        pc->count = 1;
    }
}

/* Unreference a dynamic plugin. */
static void plugin_class_unref(PluginClass * pc)
{
    pc->count -= 1;

    /* If the reference count drops to zero, unload the plugin if it is dynamic and has declared itself unloadable. */
    if ((pc->count == 0)
    && (pc->dynamic)
    && ( ! pc->not_unloadable))
    {
        g_module_close(pc->gmodule);
    }
}

/* Recursively set the background of all widgets on a panel background configuration change. */
void plugin_widget_set_background(GtkWidget * w, SimplePanel * panel)
{
    GdkRGBA transparent;
    gdk_rgba_parse(&transparent,"transparent");
    if (w != NULL)
    {
        gchar* css = css_generate_background("",transparent,TRUE);
        css_apply_with_class(w,css,"-lxpanel-background",FALSE);
        g_free(css);
    }
}
/* Handler for "button_press_event" signal with Plugin as parameter.
 * External so can be used from a plugin. */
static gboolean lxpanel_plugin_button_press_event(GtkWidget *plugin, GdkEventButton *event, SimplePanel *panel)
{
    if (event->button == 3 && /* right button */
        (event->state & gtk_accelerator_get_default_mod_mask()) == 0) /* no key */
    {
        GtkMenu* popup = (GtkMenu*)lxpanel_get_plugin_menu(panel, plugin, FALSE);
        gtk_menu_popup(popup, NULL, NULL, NULL, NULL, event->button, event->time);
        return TRUE;
    }
    return FALSE;
}

/* Helper for position-calculation callback for popup menus. */
void lxpanel_plugin_popup_set_position_helper(SimplePanel * p, GtkWidget * near, GtkWidget * popup, gint * px, gint * py)
{
    gint x, y;
    GtkAllocation allocation;
    GtkAllocation popup_req;

    /* Get the allocation of the popup menu. */
    gtk_widget_realize(popup);
    gtk_widget_get_allocation(popup, &popup_req);
    if (gtk_widget_is_toplevel(popup))
    {
        GdkRectangle extents;
        /* FIXME: can we wait somehow for WM drawing decorations? */
        gdk_window_process_all_updates();
        gdk_window_get_frame_extents(gtk_widget_get_window(popup), &extents);
        popup_req.width = extents.width;
        popup_req.height = extents.height;
    }

    /* Get the origin of the requested-near widget in screen coordinates. */
    gtk_widget_get_allocation(near, &allocation);
    gdk_window_get_origin(gtk_widget_get_window(near), &x, &y);
    if (!gtk_widget_get_has_window(near))
    {
        /* For non-window widgets allocation is given within the screen */
        x += allocation.x;
        y += allocation.y;
    }

    /* Dispatch on edge to lay out the popup menu with respect to the button.
     * Also set "push-in" to avoid any case where it might flow off screen. */
    switch (p->priv->edge)
    {
        case EDGE_TOP:          y += allocation.height;         break;
        case EDGE_BOTTOM:       y -= popup_req.height;                break;
        case EDGE_LEFT:         x += allocation.width;          break;
        case EDGE_RIGHT:        x -= popup_req.width;                 break;
    }

    /* Push onscreen. */
    int screen_width = gdk_screen_width();
    int screen_height = gdk_screen_height();
    x = CLAMP(x, 0, screen_width - popup_req.width);
    y = CLAMP(y, 0, screen_height - popup_req.height);
    /* FIXME: take monitor area into account not just screen */

    *px = x;
    *py = y;
}

/* Adjust the position of a popup window to ensure that it is not hidden by the panel.
 * It is observed that some window managers do not honor the strut that is set on the panel. */
void lxpanel_plugin_adjust_popup_position(GtkWidget * popup, GtkWidget * parent)
{
    gint x, y;

    /* Calculate desired position for the popup. */
    lxpanel_plugin_popup_set_position_helper(PLUGIN_PANEL(parent), parent,
                                             popup, &x, &y);
    /* Move the popup to position. */
    gdk_window_move(gtk_widget_get_window(popup), x, y);
}

/* Open a specified path in a file manager. */
static gboolean _open_dir_in_file_manager(GAppLaunchContext* ctx, GList* folder_infos,
                                          gpointer user_data, GError** err)
{
    FmFileInfo *fi = folder_infos->data; /* only first is used */
    GAppInfo *app = g_app_info_get_default_for_type("inode/directory", TRUE);
    GFile *gf;
    gboolean ret;

    if (app == NULL)
    {
        g_set_error_literal(err, G_SHELL_ERROR, G_SHELL_ERROR_EMPTY_STRING,
                            _("No file manager is configured."));
        return FALSE;
    }
    gf = fm_path_to_gfile(fm_file_info_get_path(fi));
    folder_infos = g_list_prepend(NULL, gf);
    ret = fm_app_info_launch(app, folder_infos, ctx, err);
    g_list_free(folder_infos);
    g_object_unref(gf);
    g_object_unref(app);
    return ret;
}

gboolean lxpanel_launch_path(SimplePanel *panel, FmPath *path)
{
    return fm_launch_path_simple(NULL, NULL, path, _open_dir_in_file_manager, NULL);
}

void lxpanel_plugin_show_config_dialog(GtkWidget* plugin)
{
    const SimplePanelPluginInit *init = PLUGIN_CLASS(plugin);
    SimplePanel *panel = PLUGIN_PANEL(plugin);
    GtkWidget *dlg = panel->priv->plugin_pref_dialog;

    if (dlg && g_object_get_data(G_OBJECT(dlg), "generic-config-plugin") == plugin)
        return; /* configuration dialog is already shown for this widget */
    g_return_if_fail(panel != NULL);
    dlg = init->config(panel, plugin);
    if (dlg)
        _panel_show_config_dialog(panel, plugin, dlg);
}

static GRecMutex _mutex;

#ifndef DISABLE_PLUGINS_LOADING
FM_MODULE_DEFINE_TYPE(lxpanel_gtk, SimplePanelPluginInit, 1)

static gboolean fm_module_callback_lxpanel_gtk(const char *name, gpointer init, int ver)
{
    /* ignore ver for now, only 1 exists */
    return lxpanel_register_plugin_type(name, init);
}
#endif

void _prepare_modules(void)
{
    _all_types = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    lxpanel_plugin_qdata = g_quark_from_static_string("SimplePanel::plugin-data");
    lxpanel_plugin_qinit = g_quark_from_static_string("SimplePanel::plugin-init");
    lxpanel_plugin_qconf = g_quark_from_static_string("SimplePanel::plugin-conf");
    lxpanel_plugin_qsize = g_quark_from_static_string("SimplePanel::plugin-size");
#ifndef DISABLE_PLUGINS_LOADING
    fm_modules_add_directory(PACKAGE_LIB_DIR "/simple-panel/plugins");
    fm_module_register_lxpanel_gtk();
#endif
}

void _unload_modules(void)
{
    GHashTableIter iter;
    gpointer key, val;

    g_hash_table_iter_init(&iter, _all_types);
    while(g_hash_table_iter_next(&iter, &key, &val))
    {
        register const SimplePanelPluginInit *init = val;
        if (init->new_instance == NULL) /* old type of plugin */
        {
            plugin_class_unref(init->_reserved1);
            g_free(val);
        }
    }
    g_hash_table_destroy(_all_types);
#ifndef DISABLE_PLUGINS_LOADING
    fm_module_unregister_type("lxpanel_gtk");
#endif
}

gboolean lxpanel_register_plugin_type(const char *name, const SimplePanelPluginInit *init)
{
    const SimplePanelPluginInit *data;

    /* validate it */
    if (init->new_instance == NULL || name == NULL || name[0] == '\0')
        return FALSE;
    g_rec_mutex_lock(&_mutex);
    /* test if it's registered already */
    data = _find_plugin(name);
    if (data == NULL)
    {
        if (init->init)
            init->init();
        g_hash_table_insert(_all_types, g_strdup(name), (gpointer)init);
    }
    g_rec_mutex_unlock(&_mutex);
    return (data == NULL);
}

static void on_size_allocate(GtkWidget *widget, GdkRectangle *allocation, SimplePanel *p)
{
    GdkRectangle *alloc;

    alloc = g_object_get_qdata(G_OBJECT(widget), lxpanel_plugin_qsize);
    if (alloc->x == allocation->x && alloc->y == allocation->y &&
        alloc->width == allocation->width && alloc->height == allocation->height)
        return; /* not changed */
    *alloc = *allocation;
         /* g_debug("size-allocate on %s, params: %d,%d\n", PLUGIN_CLASS(widget)->name,allocation->width,allocation->height); */
    _panel_queue_update_background(p);
//    _queue_panel_calculate_size(p);
}

GtkWidget *lxpanel_add_plugin(SimplePanel *p, const char *name, config_setting_t *cfg, gint at)
{
    const SimplePanelPluginInit *init;
    GtkWidget *widget;
    config_setting_t *s, *pconf;
    gint expand, padding = 0, border = 0, i;

    CHECK_MODULES();
    init = _find_plugin(name);
    if (init == NULL)
        return NULL;
    /* prepare widget settings */
    if (!init->expand_available)
        expand = 0;
    else if ((s = config_setting_get_member(cfg, "expand")))
        expand = config_setting_get_int(s);
    else
        expand = init->expand_default;
    s = config_setting_get_member(cfg, "padding");
    if (s)
        padding = config_setting_get_int(s);
    s = config_setting_get_member(cfg, "border");
    if (s)
        border = config_setting_get_int(s);
    /* prepare config and create it if need */
    s = config_setting_add(cfg, "", PANEL_CONF_TYPE_LIST);
    for (i = 0; (pconf = config_setting_get_elem(s, i)); i++)
        if (strcmp(config_setting_get_name(pconf), "Config") == 0)
            break;
    if (!pconf)
        pconf = config_setting_add(s, "Config", PANEL_CONF_TYPE_GROUP);
    /* If this plugin can only be instantiated once, count the instantiation.
     * This causes the configuration system to avoid displaying the plugin as one that can be added. */
    if (init->new_instance) /* new style of plugin */
    {
        widget = init->new_instance(p, pconf);
        if (widget == NULL)
            return widget;
        /* always connect lxpanel_plugin_button_press_event() */
        g_signal_connect(widget, "button-press-event",
                         G_CALLBACK(lxpanel_plugin_button_press_event), p);
        if (init->button_press_event)
            g_signal_connect(widget, "button-press-event",
                             G_CALLBACK(init->button_press_event), p);
    }
    else
    {
        g_error("simple-panel: Plugin \"%s\" is invalid",init->name);
    }
    gtk_widget_set_name(widget, name);
    gtk_box_pack_start(GTK_BOX(p->priv->box), widget, expand, TRUE, padding);
    gtk_container_set_border_width(GTK_CONTAINER(widget), border);
    g_signal_connect(widget, "size-allocate", G_CALLBACK(on_size_allocate), p);
    gtk_widget_show(widget);
    g_object_set_qdata(G_OBJECT(widget), lxpanel_plugin_qconf, cfg);
    g_object_set_qdata(G_OBJECT(widget), lxpanel_plugin_qinit, (gpointer)init);
    g_object_set_qdata_full(G_OBJECT(widget), lxpanel_plugin_qsize,
                            g_new0(GdkRectangle, 1), g_free);
    return widget;
}

/* transfer none - note that not all fields are valid there */
GHashTable *lxpanel_get_all_types(void)
{
    return _all_types;
}
