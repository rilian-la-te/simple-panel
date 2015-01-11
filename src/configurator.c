/**
 *
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

#define __LXPANEL_INTERNALS__

#include "private.h"
#include "misc.h"
#include "css.h"
#include "panel-enum-types.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "dbg.h"
#include "app.h"
#include "panel.h"

enum{
    COL_NAME,
    COL_EXPAND,
    COL_DATA,
    N_COLS
};


#define	panel_signal_handlers_disconnect_by_func_only(instance, func)						\
    g_signal_handlers_disconnect_matched ((instance),								\
                      (GSignalMatchType) (G_SIGNAL_MATCH_FUNC),	\
                      0, 0, NULL, (func), NULL)

extern GSList* all_panels;
extern int config;
static void update_opt_menu(GtkWidget *w, int ind);
static void modify_plugin( GtkTreeView* view );
static gboolean on_entry_focus_out_old( GtkWidget* edit, GdkEventFocus *evt, gpointer user_data );
static gboolean on_entry_focus_out( GtkWidget* edit, GdkEventFocus *evt, gpointer user_data );
static gboolean _on_entry_focus_out_do_work(GtkWidget* edit, gpointer user_data);

/* If there is a panel on this edge and it is not the panel being configured, set the edge unavailable. */
gboolean panel_edge_available(Panel* p, int edge, gint monitor)
{
    GList* l;
    for (l = gtk_application_get_windows(p->app); l != NULL; l = l->next)
        {
        SimplePanel* pl = (SimplePanel*) l->data;
        if ((pl->priv != p) && (pl->priv->edge == edge) && ((pl->priv->monitor == monitor)||pl->priv->monitor<0))
            return FALSE;
        }
    return TRUE;
}

static void state_configure_monitor(GSimpleAction *action,GVariant* param, gpointer data)
{
    int state = g_variant_get_int32(g_action_get_state(G_ACTION(action)));
    GtkWidget *widget = GTK_WIDGET(data);
    SimplePanel* panel = (SimplePanel*) g_object_get_data( G_OBJECT(widget), "panel" );
    /* change monitor */
    int request_mon = g_variant_get_int32(param);
    gchar* str = request_mon < 0 ? _("Monitor: All") : g_strdup_printf(_("Monitor: %d"),request_mon+1);
    int edge = g_settings_get_enum(panel->priv->settings->toplevel_settings,PANEL_PROP_EDGE);
    if(panel_edge_available(panel->priv, edge, request_mon) || (state<-1))
    {
        g_settings_set_int(panel->priv->settings->toplevel_settings,PANEL_PROP_MONITOR,request_mon);
        g_simple_action_set_state(action,param);
        gtk_button_set_label(GTK_BUTTON(widget),str);
    }
    if (request_mon>=0)
        g_free(str);
}

static void edge_changed(SimplePanel* panel, GParamSpec* spec, GtkWidget* w)
{
    int edge = panel->priv->edge;
    gchar* str = NULL;
    switch (edge)
    {
    case PANEL_EDGE_BOTTOM:
        str = _("Edge: Bottom");
        break;
    case PANEL_EDGE_TOP:
        str = _("Edge: Top");
        break;
    case PANEL_EDGE_LEFT:
        str = _("Edge: Left");
        break;
    case PANEL_EDGE_RIGHT:
        str = _("Edge: Right");
        break;
    };
    gtk_button_set_label(GTK_BUTTON(w),str);
}

static void alignment_changed(SimplePanel* panel, GParamSpec* spec, GtkWidget* w)
{
    int alignment = panel->priv->align;
    gchar* str = NULL;
    switch (alignment)
    {
    case PANEL_ALIGN_LEFT:
        str = _("Align: Start");
        break;
    case PANEL_ALIGN_CENTER:
        str = _("Align: Center");
        break;
    case PANEL_ALIGN_RIGHT:
        str = _("Align: End");
        break;
    };
    gtk_button_set_label(GTK_BUTTON(w),str);
}

static void simple_panel_notify_scale_cb(SimplePanel* panel, GParamSpec* param, GtkScaleButton* scale)
{
    const gchar* name = g_param_spec_get_name(param);
    const gchar* retn;
    int val;
    if (g_intern_static_string(PANEL_PROP_WIDTH) == name)
        retn = PANEL_PROP_WIDTH;
    if (g_intern_static_string(PANEL_PROP_HEIGHT) == name)
        retn = PANEL_PROP_HEIGHT;
    if (g_intern_static_string(PANEL_PROP_ICON_SIZE) == name)
        retn = PANEL_PROP_ICON_SIZE;
    if (g_intern_static_string(PANEL_PROP_AUTOHIDE_SIZE) == name)
        retn = PANEL_PROP_AUTOHIDE_SIZE;
    g_object_get(panel,retn,&val);
    gchar* str = g_strdup_printf("%d",val);
    gtk_button_set_label(GTK_BUTTON(scale),str);
    g_free(str);
}

static void simple_panel_notify_color_cb(SimplePanel* panel, GParamSpec* param, GtkWidget* w)
{
    gtk_widget_set_sensitive(w,panel->priv->background==PANEL_BACKGROUND_CUSTOM_COLOR);
}

static void simple_panel_notify_image_cb(SimplePanel* panel, GParamSpec* param, GtkWidget* w)
{
    gtk_widget_set_sensitive(w,panel->priv->background==PANEL_BACKGROUND_CUSTOM_IMAGE);
}

static void simple_panel_notify_align_cb(SimplePanel* panel, GParamSpec* param, GtkWidget* w)
{
    gtk_widget_set_sensitive(w,panel->priv->align!=PANEL_ALIGN_CENTER);
}

static void
response_event(GtkDialog *widget, gint arg1, SimplePanel* panel )
{
    switch (arg1) {
    /* FIXME: what will happen if the user exit lxpanel without
              close this config dialog?
              Then the config won't be save, I guess. */
    case GTK_RESPONSE_DELETE_EVENT:
    case GTK_RESPONSE_CLOSE:
    case GTK_RESPONSE_NONE:
        /* NOTE: NO BREAK HERE*/
        panel_signal_handlers_disconnect_by_func_only(panel,simple_panel_notify_scale_cb);
        panel_signal_handlers_disconnect_by_func_only(panel,simple_panel_notify_align_cb);
        panel_signal_handlers_disconnect_by_func_only(panel,simple_panel_notify_color_cb);
        panel_signal_handlers_disconnect_by_func_only(panel,simple_panel_notify_image_cb);
        panel_signal_handlers_disconnect_by_func_only(panel,edge_changed);
        panel_signal_handlers_disconnect_by_func_only(panel,alignment_changed);
        gtk_widget_destroy(GTK_WIDGET(widget));
        break;
    }
    return;
}


static void set_strut_type( GtkWidget *item, SimplePanel* panel )
{
    GtkWidget* spin;
    Panel *p = panel->priv;
    int widthtype;
    gboolean t;

    widthtype = gtk_combo_box_get_active(GTK_COMBO_BOX(item)) + 1;
    if (p->widthtype == widthtype) /* not changed */
        return;

    p->widthtype = widthtype;

    spin = (GtkWidget*)g_object_get_data(G_OBJECT(item), "scale-width" );
    t = (widthtype != PANEL_SIZE_DYNAMIC);
    gtk_widget_set_sensitive( spin, t );
    g_settings_set_enum(panel->priv->settings->toplevel_settings,PANEL_PROP_SIZE_TYPE,widthtype);
    switch (widthtype)
    {
    case PANEL_SIZE_PERCENT:
        simple_panel_scale_button_set_range(GTK_SCALE_BUTTON(spin),0,100);
        g_settings_set_int(panel->priv->settings->toplevel_settings,PANEL_PROP_WIDTH,100);
        break;
    case PANEL_SIZE_PIXEL:
        if ((p->edge == PANEL_EDGE_TOP) || (p->edge == PANEL_EDGE_BOTTOM))
        {
            simple_panel_scale_button_set_range(GTK_SCALE_BUTTON(spin),0,gdk_screen_width());
            g_settings_set_int(panel->priv->settings->toplevel_settings,PANEL_PROP_WIDTH,gdk_screen_width());
        }
        else
        {
            simple_panel_scale_button_set_range(GTK_SCALE_BUTTON(spin),0,gdk_screen_height());
            g_settings_set_int(panel->priv->settings->toplevel_settings,PANEL_PROP_WIDTH,gdk_screen_height());
        }
        break;
    case PANEL_SIZE_DYNAMIC:
        break;
    default: ;
    }
}

/* FIXME: heighttype and spacing and RoundCorners */

static void background_changed(GtkFileChooser *file_chooser,  Panel* p )
{
    char * file = g_strdup(gtk_file_chooser_get_filename(file_chooser));
    if (file != NULL)
    {
        g_settings_set_string(p->settings->toplevel_settings,PANEL_PROP_BACKGROUND_FILE,file);
        g_free(file);
    }
}

static void
on_font_color_set( GtkColorChooser* clr,  Panel* p )
{
    gtk_color_chooser_get_rgba( clr, &p->gfontcolor );
    char* color = gdk_rgba_to_string(&p->gfontcolor);
    g_settings_set_string(p->settings->toplevel_settings,PANEL_PROP_FONT_COLOR,color);
    g_free(color);
}

static void
on_tint_color_set( GtkColorChooser* clr,  Panel* p )
{
    gtk_color_chooser_get_rgba( clr, &p->gtintcolor );
    char* color = gdk_rgba_to_string(&p->gtintcolor);
    g_settings_set_string(p->settings->toplevel_settings,PANEL_PROP_BACKGROUND_COLOR,color);
    g_free(color);
}

static void
on_sel_plugin_changed( GtkTreeSelection* tree_sel, GtkWidget* label )
{
    GtkTreeIter it;
    GtkTreeModel* model;
    GtkWidget* pl;

    if( gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
    {
        GtkTreeView* view = gtk_tree_selection_get_tree_view( tree_sel );
        GtkWidget *edit_btn = GTK_WIDGET(g_object_get_data( G_OBJECT(view), "edit_btn" ));
        const SimplePanelPluginInit *init;
        gtk_tree_model_get( model, &it, COL_DATA, &pl, -1 );
        init = PLUGIN_CLASS(pl);
        gtk_label_set_text( GTK_LABEL(label), _(init->description) );
        gtk_widget_set_sensitive( edit_btn, init->config != NULL );
    }
}

static void
on_plugin_expand_toggled(GtkCellRendererToggle* render, char* path, GtkTreeView* view)
{
    GtkTreeModel* model;
    GtkTreeIter it;
    GtkTreePath* tp = gtk_tree_path_new_from_string( path );
    model = gtk_tree_view_get_model( view );
    if( gtk_tree_model_get_iter( model, &it, tp ) )
    {
        GtkWidget* pl;
        gboolean old_expand, expand, fill;
        guint padding;
        GtkPackType pack_type;
        const SimplePanelPluginInit *init;
        SimplePanel *panel;

        gtk_tree_model_get( model, &it, COL_DATA, &pl, COL_EXPAND, &expand, -1 );
        init = PLUGIN_CLASS(pl);
        panel = PLUGIN_PANEL(pl);

        if (init->expand_available)
        {
            PluginGSettings *s = g_object_get_qdata(G_OBJECT(pl), lxpanel_plugin_qconf);
            GtkBox *box = GTK_BOX(panel->priv->box);
            /* Only honor "stretch" if allowed by the plugin. */
            expand = ! expand;
            gtk_list_store_set( GTK_LIST_STORE(model), &it, COL_EXPAND, expand, -1 );

            /* Query the old packing of the plugin widget.
             * Apply the new packing with only "expand" modified. */
            gtk_box_query_child_packing( box, pl, &old_expand, &fill, &padding, &pack_type );
            gtk_box_set_child_packing( box, pl, expand, fill, padding, pack_type );
            g_settings_set_boolean(s->default_settings,DEFAULT_PLUGIN_KEY_EXPAND,expand);
        }
    }
    gtk_tree_path_free( tp );
}

static void on_stretch_render(GtkTreeViewColumn * column, GtkCellRenderer * renderer, GtkTreeModel * model, GtkTreeIter * iter, gpointer data)
{
    /* Set the control visible depending on whether stretch is available for the plugin.
     * The g_object_set method is touchy about its parameter, so we can't pass the boolean directly. */
    GtkWidget * pl;
    gtk_tree_model_get(model, iter, COL_DATA, &pl, -1);
    g_object_set(renderer,
        "visible", ((PLUGIN_CLASS(pl)->expand_available) ? TRUE : FALSE),
        NULL);
}

static void init_plugin_list( SimplePanel* p, GtkTreeView* view, GtkWidget* label )
{
    GtkListStore* list;
    GtkTreeViewColumn* col;
    GtkCellRenderer* render;
    GtkTreeSelection* tree_sel;
    GList *plugins, *l;
    GtkTreeIter it;

    g_object_set_data( G_OBJECT(view), "panel", p );

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes(
            _("Currently loaded plugins"),
            render, "text", COL_NAME, NULL );
    gtk_tree_view_column_set_expand( col, TRUE );
    gtk_tree_view_append_column( view, col );

    render = gtk_cell_renderer_toggle_new();
    g_object_set( render, "activatable", TRUE, NULL );
    g_signal_connect( render, "toggled", G_CALLBACK( on_plugin_expand_toggled ), view );
    col = gtk_tree_view_column_new_with_attributes(
            _("Stretch"),
            render, "active", COL_EXPAND, NULL );
    gtk_tree_view_column_set_expand( col, FALSE );
    gtk_tree_view_column_set_cell_data_func(col, render, on_stretch_render, NULL, NULL);
    gtk_tree_view_append_column( view, col );

    list = gtk_list_store_new( N_COLS, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_POINTER );
    plugins = p->priv->box ? gtk_container_get_children(GTK_CONTAINER(p->priv->box)) : NULL;
    for( l = plugins; l; l = l->next )
    {
        GtkTreeIter it;
        gboolean expand;
        GtkWidget *w = (GtkWidget*)l->data;
        gtk_container_child_get(GTK_CONTAINER(p->priv->box), w, "expand", &expand, NULL);
        gtk_list_store_append( list, &it );
        gtk_list_store_set( list, &it,
                            COL_NAME, _(PLUGIN_CLASS(w)->name),
                            COL_EXPAND, expand,
                            COL_DATA, w,
                            -1);
    }
    g_list_free(plugins);
    gtk_tree_view_set_model( view, GTK_TREE_MODEL( list ) );
    g_signal_connect( view, "row-activated",
                      G_CALLBACK(modify_plugin), NULL );
    tree_sel = gtk_tree_view_get_selection( view );
    gtk_tree_selection_set_mode( tree_sel, GTK_SELECTION_BROWSE );
    g_signal_connect( tree_sel, "changed",
                      G_CALLBACK(on_sel_plugin_changed), label);
    if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL(list), &it ) )
        gtk_tree_selection_select_iter( tree_sel, &it );
}

void update_widget_positions(SimplePanel* p);

static void on_add_plugin_row_activated( GtkTreeView *view,
                                         GtkTreePath *path,
                                         GtkTreeViewColumn *col,
                                         GtkTreeView* _view)
{
    SimplePanel* p = (SimplePanel*) g_object_get_data( G_OBJECT(_view), "panel" );
    GtkTreeSelection* tree_sel;
    GtkTreeIter it;
    GtkTreePath* tree_path;
    GtkTreeModel* model;

    tree_sel = gtk_tree_view_get_selection( view );
    if( gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
    {
        char* type = NULL;
        GtkWidget *pl;
        gtk_tree_model_get( model, &it, 1, &type, -1 );
        PluginGSettings* s = panel_gsettings_add_plugin_settings(p->priv->settings,
                                                                 type,
                                                                 panel_gsettings_find_free_num(p->priv->settings));
        guint position = -1;
        if ((pl = simple_panel_add_plugin(p, s, position)))
        {
            gboolean expand;
            gtk_container_child_get(GTK_CONTAINER(p->priv->box), pl, "expand", &expand, NULL);
            model = gtk_tree_view_get_model( _view );
            gtk_list_store_append( GTK_LIST_STORE(model), &it );
            gtk_list_store_set( GTK_LIST_STORE(model), &it,
                                COL_NAME, _(PLUGIN_CLASS(pl)->name),
                                COL_EXPAND, expand,
                                COL_DATA, pl, -1 );
            tree_sel = gtk_tree_view_get_selection( _view );
            gtk_tree_selection_select_iter( tree_sel, &it );
            if ((tree_path = gtk_tree_model_get_path(model, &it)) != NULL)
            {
                gtk_tree_view_scroll_to_cell( _view, tree_path, NULL, FALSE, 0, 0 );
                gtk_tree_path_free( tree_path );
            }
            update_widget_positions(p);
        }
        else /* free unused setting */
            panel_gsettings_remove_plugin_settings(p->priv->settings,s->plugin_number);
        g_free( type );
    }
}

static gint sort_by_name(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
    char *str_a, *str_b;
    gint res;

    gtk_tree_model_get(model, a, 0, &str_a, -1);
    gtk_tree_model_get(model, b, 0, &str_b, -1);
    res = g_utf8_collate(str_a, str_b);
    g_free(str_a);
    g_free(str_b);
    return res;
}

static void on_add_plugin( GtkButton* btn, GtkTreeView* _view )
{
    GtkWidget* dlg, *parent_win, *scroll;
    GHashTable *classes;
    GtkTreeViewColumn* col;
    GtkCellRenderer* render;
    GtkTreeView* view;
    GtkListStore* list;
    GtkTreeSelection* tree_sel;
    GHashTableIter iter;
    gpointer key, val;

    classes = lxpanel_get_all_types();
    dlg=gtk_popover_new(GTK_WIDGET(btn));
    scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW(scroll),
                                          GTK_SHADOW_IN );
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC );
    gtk_container_add(GTK_CONTAINER(dlg),scroll);
    view = GTK_TREE_VIEW(gtk_tree_view_new());
    gtk_container_add( GTK_CONTAINER(scroll), GTK_WIDGET(view) );
    tree_sel = gtk_tree_view_get_selection( view );
    gtk_tree_selection_set_mode( tree_sel, GTK_SELECTION_BROWSE );

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes(
                                            _("Available plugins"),
                                            render, "text", 0, NULL );
    gtk_tree_view_append_column( view, col );

    list = gtk_list_store_new( 2,
                               G_TYPE_STRING,
                               G_TYPE_STRING );

    /* Populate list of available plugins.
     * Omit plugins that can only exist once per system if it is already configured. */
    g_hash_table_iter_init(&iter, classes);
    while(g_hash_table_iter_next(&iter, &key, &val))
    {
        register const SimplePanelPluginInit *init = val;
        if (init->superseded)
            continue;
        if (!init->one_per_system || !_class_is_present(init))
        {
            GtkTreeIter it;
            gtk_list_store_append( list, &it );
            /* it is safe to put classes data here - they will be valid until restart */
            gtk_list_store_set( list, &it,
                                0, _(init->name),
                                1, key,
                                -1 );
            /* g_debug( "%s (%s)", pc->type, _(pc->name) ); */
        }
    }
    gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(list),
                                            sort_by_name, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list),
                                         GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                         GTK_SORT_ASCENDING);
    gtk_tree_view_set_activate_on_single_click(view,TRUE);
    gtk_tree_view_set_model( view, GTK_TREE_MODEL(list) );
    g_object_unref( list );

    g_signal_connect( view, "row-activated",
                      G_CALLBACK(on_add_plugin_row_activated), _view);

    gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scroll),320);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroll),200);
    gtk_widget_show_all( dlg );
}

static void on_remove_plugin( GtkButton* btn, GtkTreeView* view )
{
    GtkTreeIter it;
    GtkTreePath* tree_path;
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection( view );
    GtkWidget* pl;

    SimplePanel* p = (SimplePanel*) g_object_get_data( G_OBJECT(view), "panel" );

    if( gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
    {
        tree_path = gtk_tree_model_get_path( model, &it );
        gtk_tree_model_get( model, &it, COL_DATA, &pl, -1 );
        if( gtk_tree_path_get_indices(tree_path)[0] >= gtk_tree_model_iter_n_children( model, NULL ) )
            gtk_tree_path_prev( tree_path );
        gtk_list_store_remove( GTK_LIST_STORE(model), &it );
        gtk_tree_selection_select_path( tree_sel, tree_path );
        gtk_tree_path_free( tree_path );
        panel_gsettings_remove_plugin_settings(p->priv->settings,
                                               ((PluginGSettings*)g_object_get_qdata(G_OBJECT(pl), lxpanel_plugin_qconf))->plugin_number);
        /* reset conf pointer because the widget still may be referenced by configurator */
        g_object_set_qdata(G_OBJECT(pl), lxpanel_plugin_qconf, NULL);
        gtk_widget_destroy(pl);
    }
}

static void modify_plugin( GtkTreeView* view )
{
    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection( view );
    GtkTreeModel* model;
    GtkTreeIter it;
    GtkWidget* pl;
    const SimplePanelPluginInit *init;

    if( ! gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
        return;

    gtk_tree_model_get( model, &it, COL_DATA, &pl, -1 );
    init = PLUGIN_CLASS(pl);
    if (init->config)
    {
        GtkWidget *dlg;
        SimplePanel *panel = PLUGIN_PANEL(pl);
        dlg = init->config(panel, pl);
        if (dlg)
            _panel_show_config_dialog(panel, pl, dlg);
    }
}

typedef struct
{
    GtkWidget *pl;
    int cur;
    int idx;
} WidgetIndexData;

static void get_widget_index_cb(GtkWidget *widget, gpointer data)
{
    if (((WidgetIndexData *)data)->pl == widget)
        ((WidgetIndexData *)data)->idx = ((WidgetIndexData *)data)->cur;
    ((WidgetIndexData *)data)->cur++;
}

static int get_widget_index(SimplePanel* p, GtkWidget* pl)
{
    WidgetIndexData data;

    data.pl = pl;
    data.idx = -1;
    data.cur = 0;
    gtk_container_foreach(GTK_CONTAINER(p->priv->box), get_widget_index_cb, &data);
    return data.idx;
}

static void set_widget_position_key(GtkWidget* widget, gpointer data)
{
    int idx = get_widget_index(LXPANEL(data),widget);
    PluginGSettings* s = g_object_get_qdata(G_OBJECT(widget), lxpanel_plugin_qconf);
    g_settings_set_uint(s->default_settings,DEFAULT_PLUGIN_KEY_POSITION,idx);
}

static void set_widget_position(GtkWidget* widget, gpointer data)
{
    PluginGSettings* s = g_object_get_qdata(G_OBJECT(widget), lxpanel_plugin_qconf);
    int idx = g_settings_get_uint(s->default_settings,DEFAULT_PLUGIN_KEY_POSITION);
    gtk_box_reorder_child(GTK_BOX(data),widget,idx);
}

void update_widget_positions(SimplePanel* p)
{
    gtk_container_foreach(GTK_CONTAINER(p->priv->box),set_widget_position_key,p);
}

void update_positions_on_panel(SimplePanel* p)
{
    GList* l, * children = gtk_container_get_children(GTK_CONTAINER(p->priv->box));
    for (l = children; l != NULL; l = l->next)
    {
        set_widget_position(GTK_WIDGET(l->data),p->priv->box);
    }
//    gtk_container_foreach(GTK_CONTAINER(p->priv->box),set_widget_position,p->priv->box); - not working
}


static void on_moveup_plugin(  GtkButton* btn, GtkTreeView* view )
{
    GtkTreeIter it, prev;
    GtkTreeModel* model = gtk_tree_view_get_model( view );
    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection( view );
    int i;

    SimplePanel* panel = (SimplePanel*) g_object_get_data( G_OBJECT(view), "panel" );

    if( ! gtk_tree_model_get_iter_first( model, &it ) )
        return;
    if( gtk_tree_selection_iter_is_selected( tree_sel, &it ) )
        return;
    do{
        if( gtk_tree_selection_iter_is_selected(tree_sel, &it) )
        {
            GtkWidget* pl;
            PluginGSettings* s;

            gtk_tree_model_get( model, &it, COL_DATA, &pl, -1 );
            gtk_list_store_move_before( GTK_LIST_STORE( model ),
                                        &it, &prev );

            i = get_widget_index(panel, pl);
            s = g_object_get_qdata(G_OBJECT(pl), lxpanel_plugin_qconf);
            /* reorder in config, 0 is Global */
            i = i > 0 ? i : 0;

            /* reorder in panel */
            gtk_box_reorder_child(GTK_BOX(panel->priv->box), pl, i - 1);
            update_widget_positions(panel);
            return;
        }
        prev = it;
    }while( gtk_tree_model_iter_next( model, &it ) );
}

static void on_movedown_plugin(  GtkButton* btn, GtkTreeView* view )
{
    GtkTreeIter it, next;
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection( view );
    GtkWidget* pl;
    PluginGSettings *s;
    int i;

    SimplePanel* panel = (SimplePanel*) g_object_get_data( G_OBJECT(view), "panel" );

    if( ! gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
        return;
    next = it;

    if( ! gtk_tree_model_iter_next( model, &next) )
        return;

    gtk_tree_model_get( model, &it, COL_DATA, &pl, -1 );

    gtk_list_store_move_after( GTK_LIST_STORE( model ), &it, &next );

    i = get_widget_index(panel, pl);
    s = g_object_get_qdata(G_OBJECT(pl), lxpanel_plugin_qconf);
    /* reorder in panel */
    gtk_box_reorder_child(GTK_BOX(panel->priv->box), pl, i + 1);
    update_widget_positions(panel);
}

static void
update_opt_menu(GtkWidget *w, int ind)
{
    int i;

    ENTER;
    /* this trick will trigger "changed" signal even if active entry is
     * not actually changing */
    i = gtk_combo_box_get_active(GTK_COMBO_BOX(w));
    if (i == ind) {
        i = i ? 0 : 1;
        gtk_combo_box_set_active(GTK_COMBO_BOX(w), i);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(w), ind);
    RET();
}

void panel_configure( SimplePanel* panel, const gchar* sel_page )
{
    Panel *p = panel->priv;
    GtkBuilder* builder;
    GtkWidget *w, *w3 , *tint_clr;
    GtkWidget *mon_control;
    GdkScreen *screen;
    gint monitors, back_type;
    GMenu* menu;
    GSimpleActionGroup* configurator = g_simple_action_group_new();

    if( p->pref_dialog )
    {
        gtk_window_present(GTK_WINDOW(p->pref_dialog));
        return;
    }

    back_type = p->background;
    builder = gtk_builder_new();
    if( !gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/panel-pref.ui", NULL) )
    {
        g_object_unref(builder);
        return;
    }

    p->pref_dialog = (GtkWidget*)gtk_builder_get_object( builder, "panel-pref" );
    gtk_window_set_transient_for(GTK_WINDOW(p->pref_dialog), GTK_WINDOW(panel));
    g_signal_connect(p->pref_dialog, "response", G_CALLBACK(response_event), panel);
    g_object_add_weak_pointer( G_OBJECT(p->pref_dialog), (gpointer) &p->pref_dialog );
    gtk_window_set_position( GTK_WINDOW(p->pref_dialog), GTK_WIN_POS_CENTER );
    panel_apply_icon(GTK_WINDOW(p->pref_dialog));

    /* position */
    w3 = w = (GtkWidget*)gtk_builder_get_object( builder, "edge-button");
    edge_changed(panel,NULL,w);
    g_signal_connect(panel,"notify::"PANEL_PROP_EDGE,G_CALLBACK(edge_changed),w);

    /* monitor */
    monitors = 1;
    const GActionEntry entries_monitor[] = {
        {"configure-monitors", NULL,"i","-2" ,state_configure_monitor},
    };
    screen = gtk_widget_get_screen(GTK_WIDGET(panel));
    if(screen) monitors = gdk_screen_get_n_monitors(screen);
    g_assert(monitors >= 1);
    mon_control = w = (GtkWidget*)gtk_builder_get_object( builder, "monitors-button");
    g_object_set_data(G_OBJECT(mon_control), "panel", panel);
    menu = g_menu_new();
    g_menu_append(menu,_("All"),"conf.configure-monitors(-1)");
    gint i;
    for (i = 0; i < monitors; i++)
    {
        gchar* tmp = g_strdup_printf("conf.configure-monitors(%d)",i);
        gchar* str_num = g_strdup_printf("%d",i+1);
        g_menu_append(menu,str_num,tmp);
        g_free(tmp);
        g_free(str_num);
    }
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(mon_control),G_MENU_MODEL(menu));
    g_object_unref(menu);
    gtk_menu_button_set_use_popover(GTK_MENU_BUTTON(mon_control),TRUE);
    g_action_map_add_action_entries(G_ACTION_MAP(configurator),entries_monitor,G_N_ELEMENTS(entries_monitor),mon_control);
    g_action_group_change_action_state(G_ACTION_GROUP(configurator),
                                       "configure-monitors",
                                       g_variant_new_int32(
                                           g_settings_get_int(
                                               p->settings->toplevel_settings,
                                               PANEL_PROP_MONITOR)));
    /* alignment */
    w3 = w = (GtkWidget*)gtk_builder_get_object( builder, "alignment-button");
    alignment_changed(panel,NULL,w);
    g_signal_connect(panel,"notify::"PANEL_PROP_ALIGNMENT,G_CALLBACK(alignment_changed),w);

    /* margin */
    p->margin_control = w = (GtkWidget*)gtk_builder_get_object( builder, "margin" );
    g_settings_bind(p->settings->toplevel_settings,PANEL_PROP_MARGIN,w,"value",G_SETTINGS_BIND_DEFAULT);
    gtk_widget_set_sensitive(w,p->align != PANEL_ALIGN_CENTER);
    g_signal_connect(panel, "notify::"PANEL_PROP_ALIGNMENT,G_CALLBACK(simple_panel_notify_align_cb),w);

    /* size */
    p->width_control = w = (GtkWidget*)gtk_builder_get_object( builder, "scale-width" );
    gint upper = 0;
    if( p->widthtype == PANEL_SIZE_PERCENT)
        upper = 100;
    else if( p->widthtype == PANEL_SIZE_PIXEL)
        upper = (((p->edge == PANEL_EDGE_TOP) || (p->edge == PANEL_EDGE_BOTTOM)) ? gdk_screen_width() : gdk_screen_height());
    simple_panel_scale_button_set_range(GTK_SCALE_BUTTON(w),0,upper);
    simple_panel_scale_button_set_value_labeled( GTK_SCALE_BUTTON(w), p->width );
    g_settings_bind(p->settings->toplevel_settings,PANEL_PROP_WIDTH,w,"value",G_SETTINGS_BIND_DEFAULT);
    g_signal_connect(panel, "notify::"PANEL_PROP_WIDTH, G_CALLBACK(simple_panel_notify_scale_cb), w );
    gtk_widget_set_sensitive( w, p->widthtype != PANEL_SIZE_DYNAMIC );


    w = (GtkWidget*)gtk_builder_get_object( builder, "width_unit" );
    update_opt_menu( w, p->widthtype - 1 );
    g_object_set_data(G_OBJECT(w), "scale-width", p->width_control );
    g_signal_connect( w, "changed",
                     G_CALLBACK(set_strut_type), panel);

    p->height_control = w = (GtkWidget*)gtk_builder_get_object( builder, "scale-height" );
    simple_panel_scale_button_set_range(GTK_SCALE_BUTTON(w),PANEL_HEIGHT_MIN,PANEL_HEIGHT_MAX);
    simple_panel_scale_button_set_value_labeled( GTK_SCALE_BUTTON(w), p->height);
    g_settings_bind(p->settings->toplevel_settings,PANEL_PROP_HEIGHT,w,"value",G_SETTINGS_BIND_DEFAULT);
    g_signal_connect(panel, "notify::"PANEL_PROP_HEIGHT, G_CALLBACK(simple_panel_notify_scale_cb), w );

    w = (GtkWidget*)gtk_builder_get_object( builder, "scale-iconsize" );
    simple_panel_scale_button_set_range(GTK_SCALE_BUTTON(w),PANEL_HEIGHT_MIN,PANEL_HEIGHT_MAX);
    simple_panel_scale_button_set_value_labeled( GTK_SCALE_BUTTON(w), p->icon_size );
    g_settings_bind(p->settings->toplevel_settings,PANEL_PROP_ICON_SIZE,w,"value",G_SETTINGS_BIND_DEFAULT);
    g_signal_connect(panel, "notify::"PANEL_PROP_ICON_SIZE, G_CALLBACK(simple_panel_notify_scale_cb), w );


    w = (GtkWidget*)gtk_builder_get_object( builder, "scale-minimized" );
    simple_panel_scale_button_set_value_labeled(GTK_SCALE_BUTTON(w), p->height_when_hidden);
    g_settings_bind(p->settings->toplevel_settings,PANEL_PROP_AUTOHIDE_SIZE,w,"value",G_SETTINGS_BIND_DEFAULT);
    g_signal_connect(panel, "notify::"PANEL_PROP_AUTOHIDE_SIZE, G_CALLBACK(simple_panel_notify_scale_cb), w );
    g_settings_bind(p->settings->toplevel_settings,PANEL_PROP_AUTOHIDE,w,"sensitive",G_SETTINGS_BIND_GET);
    /* background */
    {
        GtkIconInfo* info;
        tint_clr = w = (GtkWidget*)gtk_builder_get_object( builder, "tint_clr" );
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(w), &p->gtintcolor);
        g_signal_connect( w, "color-set", G_CALLBACK( on_tint_color_set ), p );
        gtk_widget_set_sensitive(tint_clr,p->background == PANEL_BACKGROUND_CUSTOM_COLOR);
        g_signal_connect(panel, "notify::"PANEL_PROP_BACKGROUND_TYPE,G_CALLBACK(simple_panel_notify_color_cb),w);

        w = (GtkWidget*)gtk_builder_get_object( builder, "img_file" );
        if (p->background_file != NULL)
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(w), p->background_file);
        else if ((info = gtk_icon_theme_lookup_icon(p->icon_theme, "lxpanel-background", 0, 0)))
        {
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(w), gtk_icon_info_get_filename(info));
            g_object_unref(info);
        }
        gtk_widget_set_sensitive(w,p->background == PANEL_BACKGROUND_CUSTOM_IMAGE);
        g_signal_connect(panel, "notify::"PANEL_PROP_BACKGROUND_TYPE,G_CALLBACK(simple_panel_notify_image_cb),w);
        g_signal_connect( w, "file-set", G_CALLBACK (background_changed), p);
    }
    /* font color */
    w = (GtkWidget*)gtk_builder_get_object( builder, "font_clr" );
    gtk_color_chooser_set_rgba( GTK_COLOR_CHOOSER(w), &p->gfontcolor );
    g_signal_connect( w, "color-set", G_CALLBACK( on_font_color_set ), p );
    g_settings_bind(p->settings->toplevel_settings,PANEL_PROP_ENABLE_FONT_COLOR,w,"sensitive",G_SETTINGS_BIND_GET);

    /* font size */
    w = (GtkWidget*)gtk_builder_get_object( builder, "font_size" );
    g_settings_bind(p->settings->toplevel_settings,PANEL_PROP_FONT_SIZE,w,"value",G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(p->settings->toplevel_settings,PANEL_PROP_ENABLE_FONT_SIZE,w,"sensitive",G_SETTINGS_BIND_GET);

    /* plugin list */
    {
        GtkWidget* plugin_list = (GtkWidget*)gtk_builder_get_object( builder, "plugin_list" );

        /* buttons used to edit plugin list */
        w = (GtkWidget*)gtk_builder_get_object( builder, "add_btn" );
        g_signal_connect( w, "clicked", G_CALLBACK(on_add_plugin), plugin_list );

        w = (GtkWidget*)gtk_builder_get_object( builder, "edit_btn" );
        g_signal_connect_swapped( w, "clicked", G_CALLBACK(modify_plugin), plugin_list );
        g_object_set_data( G_OBJECT(plugin_list), "edit_btn", w );

        w = (GtkWidget*)gtk_builder_get_object( builder, "remove_btn" );
        g_signal_connect( w, "clicked", G_CALLBACK(on_remove_plugin), plugin_list );
        w = (GtkWidget*)gtk_builder_get_object( builder, "moveup_btn" );
        g_signal_connect( w, "clicked", G_CALLBACK(on_moveup_plugin), plugin_list );
        w = (GtkWidget*)gtk_builder_get_object( builder, "movedown_btn" );
        g_signal_connect( w, "clicked", G_CALLBACK(on_movedown_plugin), plugin_list );

        w = (GtkWidget*)gtk_builder_get_object( builder, "plugin_desc" );
        init_plugin_list( panel, GTK_TREE_VIEW(plugin_list), w );
    }
    w = (GtkWidget*)gtk_builder_get_object( builder, "prefs" );
    gtk_stack_set_visible_child_name(GTK_STACK(w),sel_page);
    g_object_unref(builder);
    gtk_widget_insert_action_group(GTK_WIDGET(p->pref_dialog),"conf",G_ACTION_GROUP(configurator));
    gtk_widget_insert_action_group(GTK_WIDGET(p->pref_dialog),"win",G_ACTION_GROUP(panel));
    gtk_widget_insert_action_group(GTK_WIDGET(p->pref_dialog),"app",G_ACTION_GROUP(panel->priv->app));
    gtk_widget_show(GTK_WIDGET(p->pref_dialog));
    g_settings_set_enum(p->settings->toplevel_settings,PANEL_PROP_BACKGROUND_TYPE,back_type);
}

static void notify_apply_config( GtkWidget* widget )
{
    GSourceFunc apply_func;
    GtkWidget* dlg;

    dlg = gtk_widget_get_toplevel( widget );
    apply_func = g_object_get_data( G_OBJECT(dlg), "apply_func" );
    if( apply_func )
        (*apply_func)( g_object_get_data(G_OBJECT(dlg), "apply_func_data") );
}

static gboolean _on_entry_focus_out_do_work(GtkWidget* edit, gpointer user_data)
{
    char** val = (char**)user_data;
    const char *new_val;
    new_val = gtk_entry_get_text(GTK_ENTRY(edit));
    if (g_strcmp0(*val, new_val) == 0) /* not changed */
        return FALSE;
    g_free( *val );
    *val = (new_val && *new_val) ? g_strdup( new_val ) : NULL;
    return TRUE;
}

static gboolean on_entry_focus_out_old( GtkWidget* edit, GdkEventFocus *evt, gpointer user_data )
{
    if (_on_entry_focus_out_do_work(edit, user_data))
        notify_apply_config( edit );
    return FALSE;
}

/* the same but affects fm_config instead of panel config */
static gboolean on_entry_focus_out( GtkWidget* edit, GdkEventFocus *evt, gpointer user_data )
{
    if (_on_entry_focus_out_do_work(edit, user_data))
        fm_config_save(fm_config, NULL);
    return FALSE;
}

static void on_spin_changed( GtkSpinButton* spin, gpointer user_data )
{
    int* val = (int*)user_data;
    *val = (int)gtk_spin_button_get_value( spin );
    notify_apply_config( GTK_WIDGET(spin) );
}

static void on_toggle_changed( GtkToggleButton* btn, gpointer user_data )
{
    gboolean* val = (gboolean*)user_data;
    *val = gtk_toggle_button_get_active( btn );
    notify_apply_config( GTK_WIDGET(btn) );
}

static void on_file_chooser_btn_file_set(GtkFileChooser* btn, char** val)
{
    g_free( *val );
    *val = gtk_file_chooser_get_filename(btn);
    notify_apply_config( GTK_WIDGET(btn) );
}

static void on_browse_btn_clicked(GtkButton* btn, GtkEntry* entry)
{
    char* file;
    GtkFileChooserAction action = (GtkFileChooserAction) g_object_get_data(G_OBJECT(btn), "chooser-action");
    GtkWidget* dlg = GTK_WIDGET(g_object_get_data(G_OBJECT(btn), "dlg"));
    GtkWidget* fc = gtk_file_chooser_dialog_new(
                                        (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER) ? _("Select a directory") : _("Select a file"),
                                        GTK_WINDOW(dlg),
                                        action,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_OK"), GTK_RESPONSE_OK,
                                        NULL);
    file = (char*)gtk_entry_get_text(entry);
    if( file && *file )
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(fc), file );
    if( gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_OK )
    {
        file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
        gtk_entry_set_text(entry, file);
        on_entry_focus_out_old(GTK_WIDGET(entry), NULL, g_object_get_data(G_OBJECT(btn), "file-val"));
        g_free(file);
    }
    gtk_widget_destroy(fc);
}

/* if the plugin was destroyed then destroy the dialog opened for it */
static void on_plugin_destroy(GtkWidget *plugin, GtkDialog *dlg)
{
    gtk_dialog_response(dlg, GTK_RESPONSE_CLOSE);
}

/* Handler for "response" signal from standard configuration dialog. */
static void generic_config_dlg_response(GtkWidget * dlg, int response, Panel * panel)
{
    gpointer plugin = g_object_get_data(G_OBJECT(dlg), "generic-config-plugin");
    if (plugin)
        g_signal_handlers_disconnect_by_func(plugin, on_plugin_destroy, dlg);
    g_object_set_data(G_OBJECT(dlg), "generic-config-plugin", NULL);
    panel->plugin_pref_dialog = NULL;
    gtk_widget_destroy(dlg);
}

void _panel_show_config_dialog(SimplePanel *panel, GtkWidget *p, GtkWidget *dlg)
{
    gint x, y;

    /* If there is already a plugin configuration dialog open, close it.
     * Then record this one in case the panel or plugin is deleted. */
    if (panel->priv->plugin_pref_dialog != NULL)
        gtk_dialog_response(GTK_DIALOG(panel->priv->plugin_pref_dialog), GTK_RESPONSE_CLOSE);
    panel->priv->plugin_pref_dialog = dlg;

    /* add some handlers to destroy the dialog on responce or widget destroy */
    g_signal_connect(dlg, "response", G_CALLBACK(generic_config_dlg_response), panel->priv);
    g_signal_connect(p, "destroy", G_CALLBACK(on_plugin_destroy), dlg);
    g_object_set_data(G_OBJECT(dlg), "generic-config-plugin", p);

    /* adjust config dialog window position to be near plugin */
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(panel));
//    gtk_window_iconify(GTK_WINDOW(dlg));
    lxpanel_plugin_popup_set_position_helper(panel, p, dlg, &x, &y);
    gtk_window_move(GTK_WINDOW(dlg), x, y);

    gtk_window_present(GTK_WINDOW(dlg));
}

/* Parameters: const char* name, gpointer ret_value, GType type, ....NULL */
static GtkWidget *_lxpanel_generic_config_dlg(const char *title, Panel *p,
                                              GSourceFunc apply_func,
                                              gpointer plugin,
                                              const char *name, va_list args)
{
    GtkWidget* dlg = gtk_dialog_new_with_buttons( title, NULL, 0,
                                                  _("_Close"),
                                                  GTK_RESPONSE_CLOSE,
                                                  NULL );
    GtkBox *dlg_vbox = GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg)));

    panel_apply_icon(GTK_WINDOW(dlg));

    if( apply_func )
        g_object_set_data( G_OBJECT(dlg), "apply_func", apply_func );
    g_object_set_data( G_OBJECT(dlg), "apply_func_data", plugin );

    gtk_box_set_spacing( dlg_vbox, 4 );

    while( name )
    {
        GtkWidget* label = gtk_label_new( name );
        GtkWidget* entry = NULL;
        gpointer val = va_arg( args, gpointer );
        PluginConfType type = va_arg( args, PluginConfType );
        if (type != CONF_TYPE_TRIM && val == NULL)
            g_critical("NULL pointer for generic config dialog");
        else switch( type )        {
            case CONF_TYPE_STR:
            case CONF_TYPE_FILE_ENTRY: /* entry with a button to browse for files. */
            case CONF_TYPE_DIRECTORY_ENTRY: /* entry with a button to browse for directories. */
                entry = gtk_entry_new();
                if( *(char**)val )
                    gtk_entry_set_text( GTK_ENTRY(entry), *(char**)val );
                gtk_entry_set_width_chars(GTK_ENTRY(entry), 40);
                g_signal_connect( entry, "focus-out-event",
                  G_CALLBACK(on_entry_focus_out_old), val );
                break;
            case CONF_TYPE_INT:
            {
                /* FIXME: the range shouldn't be hardcoded */
                entry = gtk_spin_button_new_with_range( 0, 1000, 1 );
                gtk_spin_button_set_value( GTK_SPIN_BUTTON(entry), *(int*)val );
                g_signal_connect( entry, "value-changed",
                  G_CALLBACK(on_spin_changed), val );
                break;
            }
            case CONF_TYPE_BOOL:
                entry = gtk_check_button_new();
                gtk_container_add( GTK_CONTAINER(entry), label );
                gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(entry), *(gboolean*)val );
                g_signal_connect( entry, "toggled",
                  G_CALLBACK(on_toggle_changed), val );
                break;
            case CONF_TYPE_FILE:
                entry = gtk_file_chooser_button_new(_("Select a file"), GTK_FILE_CHOOSER_ACTION_OPEN);
                if( *(char**)val )
                    gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(entry), *(char**)val );
                g_signal_connect( entry, "file-set",
                  G_CALLBACK(on_file_chooser_btn_file_set), val );
                break;
            case CONF_TYPE_TRIM:
                {
                entry = gtk_label_new(NULL);
                char *markup = g_markup_printf_escaped ("<span style=\"italic\">%s</span>", name );
                gtk_label_set_markup (GTK_LABEL (entry), markup);
                g_free (markup);
                }
                break;
            case CONF_TYPE_EXTERNAL:
                if (GTK_IS_WIDGET(val))
                    gtk_box_pack_start(dlg_vbox, val, FALSE, FALSE, 2);
                else
                    g_critical("value for CONF_TYPE_EXTERNAL is not a GtkWidget");
                break;
        }
        if( entry )
        {
            if(( type == CONF_TYPE_BOOL ) || ( type == CONF_TYPE_TRIM ))
                gtk_box_pack_start( dlg_vbox, entry, FALSE, FALSE, 2 );
            else
            {
                GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 2 );
                gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, FALSE, 2 );
                gtk_box_pack_start( GTK_BOX(hbox), entry, TRUE, TRUE, 2 );
                gtk_box_pack_start( dlg_vbox, hbox, FALSE, FALSE, 2 );
                if ((type == CONF_TYPE_FILE_ENTRY) || (type == CONF_TYPE_DIRECTORY_ENTRY))
                {
                    GtkWidget* browse = gtk_button_new_with_mnemonic(_("_Browse"));
                    gtk_box_pack_start( GTK_BOX(hbox), browse, TRUE, TRUE, 2 );
                    g_object_set_data(G_OBJECT(browse), "file-val", val);
                    g_object_set_data(G_OBJECT(browse), "dlg", dlg);

                    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
                    if (type == CONF_TYPE_DIRECTORY_ENTRY)
                    {
                      action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
                    }

                    g_object_set_data(G_OBJECT(browse), "chooser-action", GINT_TO_POINTER(action));
                    g_signal_connect( browse, "clicked", G_CALLBACK(on_browse_btn_clicked), entry );
                }
            }
        }
        name = va_arg( args, const char* );
    }

    gtk_container_set_border_width( GTK_CONTAINER(dlg), 8 );

    gtk_widget_show_all(GTK_WIDGET(dlg_vbox));

    return dlg;
}

/* new plugins API -- apply_func() gets GtkWidget* */
GtkWidget *lxpanel_generic_config_dlg(const char *title, SimplePanel *panel,
                                      GSourceFunc apply_func, GtkWidget *plugin,
                                      const char *name, ...)
{
    GtkWidget *dlg;
    va_list args;

    if (plugin == NULL)
        return NULL;
    va_start(args, name);
    dlg = _lxpanel_generic_config_dlg(title, panel->priv, apply_func, plugin, name, args);
    va_end(args);
    return dlg;
}
/* vim: set sw=4 et sts=4 : */
