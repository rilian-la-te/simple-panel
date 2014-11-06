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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <glib/gi18n.h>
#include <libfm/fm-gtk.h>

#include "dbg.h"

enum{
    COL_NAME,
    COL_EXPAND,
    COL_DATA,
    N_COLS
};

static void save_global_config();

static char* logout_cmd = NULL;

extern GSList* all_panels;
extern int config;

/* macros to update config */
#define UPDATE_GLOBAL_INT(panel,name,val) do { \
    config_setting_t *_s = config_setting_add(config_setting_get_elem(config_setting_get_member(config_root_setting(panel->config),""),\
                                                                      0),\
                                              name,PANEL_CONF_TYPE_INT);\
    if (_s) config_setting_set_int(_s,val); } while(0)

#define UPDATE_GLOBAL_STRING(panel,name,val) do { \
    config_setting_t *_s = config_setting_add(config_setting_get_elem(config_setting_get_member(config_root_setting(panel->config),""),\
                                                                      0),\
                                              name,PANEL_CONF_TYPE_STRING);\
    if (_s) config_setting_set_string(_s,val); } while(0)

void panel_config_save(Panel *p);

static void update_opt_menu(GtkWidget *w, int ind);
static void update_toggle_button(GtkWidget *w, gboolean n);
static void modify_plugin( GtkTreeView* view );
static gboolean on_entry_focus_out_old( GtkWidget* edit, GdkEventFocus *evt, gpointer user_data );
static gboolean on_entry_focus_out( GtkWidget* edit, GdkEventFocus *evt, gpointer user_data );
static gboolean _on_entry_focus_out_do_work(GtkWidget* edit, gpointer user_data);

static void
response_event(GtkDialog *widget, gint arg1, Panel* panel )
{
    switch (arg1) {
    /* FIXME: what will happen if the user exit lxpanel without
              close this config dialog?
              Then the config won't be save, I guess. */
    case GTK_RESPONSE_DELETE_EVENT:
    case GTK_RESPONSE_CLOSE:
    case GTK_RESPONSE_NONE:
        panel_config_save( panel );
        /* NOTE: NO BREAK HERE*/
        gtk_widget_destroy(GTK_WIDGET(widget));
        break;
    }
    return;
}

static void
update_panel_geometry( SimplePanel* p )
{
    /* Guard against being called early in panel creation. */
    _calculate_position(p);
    gtk_widget_set_size_request(GTK_WIDGET(p), p->priv->aw, p->priv->ah);
    gdk_window_move(gtk_widget_get_window(GTK_WIDGET(p)), p->priv->ax, p->priv->ay);
    _panel_queue_update_background(p);
    _panel_establish_autohide(p);
    _panel_set_wm_strut(p);
}

static gboolean edge_selector(Panel* p, int edge)
{
    return (p->edge == edge);
}

/* If there is a panel on this edge and it is not the panel being configured, set the edge unavailable. */
gboolean panel_edge_available(Panel* p, int edge, gint monitor)
{
    GSList* l;
    for (l = all_panels; l != NULL; l = l->next)
        {
        SimplePanel* pl = (SimplePanel*) l->data;
        if ((pl->priv != p) && (pl->priv->edge == edge) && (pl->priv->monitor == monitor))
            return FALSE;
        }
    return TRUE;
}

static void set_edge(SimplePanel* panel, int edge)
{
    Panel *p = panel->priv;

    p->edge = edge;
    update_panel_geometry(panel);
    _panel_set_panel_configuration_changed(panel);
    UPDATE_GLOBAL_STRING(p, "edge", num2str(edge_pair, edge, "none"));
}

static void edge_bottom_toggle(GtkToggleButton *widget, SimplePanel *p)
{
    GtkButton* edge_button = (GtkButton*)g_object_get_data(G_OBJECT(widget), "edge-button" );
    if (gtk_toggle_button_get_active(widget))
    {
        gtk_button_set_label(edge_button,gtk_button_get_label(GTK_BUTTON(widget)));
        set_edge(p, GTK_POS_BOTTOM);
    }
}

static void edge_top_toggle(GtkToggleButton *widget, SimplePanel *p)
{
    GtkButton* edge_button = (GtkButton*)g_object_get_data(G_OBJECT(widget), "edge-button" );
    if (gtk_toggle_button_get_active(widget))
    {
        gtk_button_set_label(edge_button,gtk_button_get_label(GTK_BUTTON(widget)));
        set_edge(p, GTK_POS_TOP);
    }
}

static void edge_left_toggle(GtkToggleButton *widget, SimplePanel *p)
{
    GtkButton* edge_button = (GtkButton*)g_object_get_data(G_OBJECT(widget), "edge-button" );
    if (gtk_toggle_button_get_active(widget))
    {
        gtk_button_set_label(edge_button,gtk_button_get_label(GTK_BUTTON(widget)));
        set_edge(p, GTK_POS_LEFT);
    }
}

static void edge_right_toggle(GtkToggleButton *widget, SimplePanel *p)
{
    GtkButton* edge_button = (GtkButton*)g_object_get_data(G_OBJECT(widget), "edge-button" );
    if (gtk_toggle_button_get_active(widget))
    {
        gtk_button_set_label(edge_button,gtk_button_get_label(GTK_BUTTON(widget)));
        set_edge(p, GTK_POS_RIGHT);
    }
}

static void set_monitor(GtkSpinButton *widget, SimplePanel *panel)
{
    Panel *p = panel->priv;

    p->monitor = gtk_spin_button_get_value_as_int(widget) - 1;
    update_panel_geometry(panel);
    _panel_set_panel_configuration_changed(panel);
    UPDATE_GLOBAL_INT(p, "monitor", p->monitor);
}

static void set_alignment(SimplePanel* panel, int align)
{
    Panel *p = panel->priv;

    if (p->margin_control)
        gtk_widget_set_sensitive(p->margin_control, (align != PANEL_ALLIGN_CENTER));
    p->allign = align;
    update_panel_geometry(panel);
    UPDATE_GLOBAL_STRING(p, "allign", num2str(allign_pair, align, "none"));
}

static void align_left_toggle(GtkToggleButton *widget, SimplePanel *p)
{
    GtkButton* b = (GtkButton*)g_object_get_data(G_OBJECT(widget), "alignment-button" );
    if (gtk_toggle_button_get_active(widget))
    {
        gtk_button_set_label(b,gtk_button_get_label(GTK_BUTTON(widget)));
        set_alignment(p, PANEL_ALLIGN_LEFT);
    }
}

static void align_center_toggle(GtkToggleButton *widget, SimplePanel *p)
{
    GtkButton* b = (GtkButton*)g_object_get_data(G_OBJECT(widget), "alignment-button" );
    if (gtk_toggle_button_get_active(widget))
    {
        gtk_button_set_label(b,gtk_button_get_label(GTK_BUTTON(widget)));
        set_alignment(p, PANEL_ALLIGN_CENTER);
    }
}

static void align_right_toggle(GtkToggleButton *widget, SimplePanel *p)
{
    GtkButton* b = (GtkButton*)g_object_get_data(G_OBJECT(widget), "alignment-button" );
    if (gtk_toggle_button_get_active(widget))
    {
        gtk_button_set_label(b,gtk_button_get_label(GTK_BUTTON(widget)));
        set_alignment(p, PANEL_ALLIGN_RIGHT);
    }
}

static void
set_margin(GtkSpinButton* spin, SimplePanel* panel)
{
    Panel *p = panel->priv;
    p->margin = gtk_spin_button_get_value(spin);
    update_panel_geometry(panel);
    UPDATE_GLOBAL_INT(p, "margin", p->margin);
}

static void
set_width(GtkScaleButton* spin, SimplePanel* panel)
{
    Panel *p = panel->priv;

    p->width = (int)gtk_scale_button_get_value(spin);
    gchar* str = g_strdup_printf("%d",p->width);
    gtk_button_set_label(GTK_BUTTON(spin),str);
    g_free(str);
    update_panel_geometry(panel);
    UPDATE_GLOBAL_INT(p, "width", p->width);
}

static void
set_height(GtkScaleButton* spin, SimplePanel* panel)
{
    Panel *p = panel->priv;

    p->height = (int)gtk_scale_button_get_value(spin);
    gchar* str = g_strdup_printf("%d",p->height);
    gtk_button_set_label(GTK_BUTTON(spin),str);
    g_free(str);
    update_panel_geometry(panel);
    UPDATE_GLOBAL_INT(p, "height", p->height);
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
    t = (widthtype != STRUT_DYNAMIC);
    gtk_widget_set_sensitive( spin, t );
    switch (widthtype)
    {
    case STRUT_PERCENT:
        simple_panel_scale_button_set_range(GTK_SCALE_BUTTON(spin),0,100);
        gtk_scale_button_set_value( GTK_SCALE_BUTTON(spin), 100 );
        break;
    case STRUT_PIXEL:
        if ((p->edge == GTK_POS_TOP) || (p->edge == GTK_POS_BOTTOM))
        {
            simple_panel_scale_button_set_range(GTK_SCALE_BUTTON(spin),0,gdk_screen_width());
            gtk_scale_button_set_value( GTK_SCALE_BUTTON(spin), gdk_screen_width() );
        }
        else
        {
            simple_panel_scale_button_set_range(GTK_SCALE_BUTTON(spin),0,gdk_screen_height());
            gtk_scale_button_set_value( GTK_SCALE_BUTTON(spin), gdk_screen_height() );
        }
        break;
    case STRUT_DYNAMIC:
        break;
    default: ;
    }

    update_panel_geometry(panel);
    UPDATE_GLOBAL_STRING(p, "widthtype", num2str(strut_pair, widthtype, "none"));
}

/* FIXME: heighttype and spacing and RoundCorners */

static void set_background_type(GtkWidget* item, SimplePanel* panel)
{
    Panel *p = panel->priv;
    int type;

    type = gtk_combo_box_get_active(GTK_COMBO_BOX(item));
    if (p->background == type) /* not changed */
        return;

    p->background = type;
    GtkWidget* color = (GtkWidget*)g_object_get_data(G_OBJECT(item), "tint_clr" );
    GtkWidget* image = (GtkWidget*)g_object_get_data(G_OBJECT(item), "img_file" );
    gtk_widget_set_sensitive(color,p->background == PANEL_BACKGROUND_CUSTOM_COLOR);
    gtk_widget_set_sensitive(image,p->background == PANEL_BACKGROUND_CUSTOM_IMAGE);
    panel_update_background(p);
    _panel_set_panel_configuration_changed(panel);
    UPDATE_GLOBAL_STRING(p, "background", num2str(background_pair, type, "gtk-normal"));
}

static void background_file_helper(Panel * p, GtkWidget * toggle, GtkFileChooser * file_chooser)
{
    char * file = g_strdup(gtk_file_chooser_get_filename(file_chooser));
    if (file != NULL)
    {
        g_free(p->background_file);
        p->background_file = file;
        UPDATE_GLOBAL_STRING(p, "backgroundfile", p->background_file);
        panel_update_background(p);
    }
}

static void background_changed(GtkFileChooser *file_chooser,  Panel* p )
{
    GtkWidget * btn = GTK_WIDGET(g_object_get_data(G_OBJECT(file_chooser), "bg_image"));
    background_file_helper(p, btn, file_chooser);
}

static void
on_font_color_set( GtkColorChooser* clr,  Panel* p )
{
    gtk_color_chooser_get_rgba( clr, &p->gfontcolor );
    panel_update_fonts (p);
    panel_set_panel_configuration_changed(p);
    UPDATE_GLOBAL_STRING(p, "fontcolor", gdk_rgba_to_string(&p->gfontcolor));
}

static void
on_tint_color_set( GtkColorChooser* clr,  Panel* p )
{
    gtk_color_chooser_set_use_alpha(clr,TRUE);
    gtk_color_chooser_get_rgba( clr, &p->gtintcolor );
    panel_update_background( p );
    UPDATE_GLOBAL_STRING(p, "tintcolor", gdk_rgba_to_string(&p->gtintcolor));
}

static void
on_use_font_color_toggled( GtkToggleButton* btn,   Panel* p )
{
    GtkWidget* clr = (GtkWidget*)g_object_get_data( G_OBJECT(btn), "clr" );
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn)))
        gtk_widget_set_sensitive( clr, TRUE );
    else
        gtk_widget_set_sensitive( clr, FALSE );
    p->usefontcolor = gtk_toggle_button_get_active( btn );
    panel_update_fonts (p);
    panel_set_panel_configuration_changed(p);
    UPDATE_GLOBAL_INT(p, "usefontcolor", p->usefontcolor);
}

static void
on_font_size_set( GtkSpinButton* spin, Panel* p )
{
    p->fontsize = (int)gtk_spin_button_get_value(spin);
    panel_update_fonts (p);
    panel_set_panel_configuration_changed(p);
    UPDATE_GLOBAL_INT(p, "fontsize", p->fontsize);
}

static void
on_use_font_size_toggled( GtkToggleButton* btn,   Panel* p )
{
    GtkWidget* clr = (GtkWidget*)g_object_get_data( G_OBJECT(btn), "clr" );
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn)))
        gtk_widget_set_sensitive( clr, TRUE );
    else
        gtk_widget_set_sensitive( clr, FALSE );
    p->usefontsize = gtk_toggle_button_get_active( btn );
    panel_update_fonts (p);
    panel_set_panel_configuration_changed(p);
    UPDATE_GLOBAL_INT(p, "usefontsize", p->usefontsize);
}


static void
set_dock_type(GtkToggleButton* toggle, SimplePanel* panel)
{
    Panel *p = panel->priv;

    p->setdocktype = gtk_toggle_button_get_active(toggle) ? 1 : 0;
    panel_set_dock_type( p );
    update_panel_geometry(panel);
    UPDATE_GLOBAL_INT(p, "setdocktype", p->setdocktype);
}

static void
set_strut(GtkToggleButton* toggle, SimplePanel* panel)
{
    Panel *p = panel->priv;

    p->setstrut = gtk_toggle_button_get_active(toggle) ? 1 : 0;
    update_panel_geometry(panel);
    UPDATE_GLOBAL_INT(p, "setpartialstrut", p->setstrut);
}

static void
set_autohide(GtkToggleButton* toggle, SimplePanel* panel)
{
    Panel *p = panel->priv;

    p->autohide = gtk_toggle_button_get_active(toggle) ? 1 : 0;
    update_panel_geometry(panel);
    UPDATE_GLOBAL_INT(p, "autohide", p->autohide);
}

static void
set_height_when_minimized(GtkSpinButton* spin, SimplePanel* panel)
{
    Panel *p = panel->priv;

    p->height_when_hidden = (int)gtk_spin_button_get_value(spin);
    update_panel_geometry(panel);
    UPDATE_GLOBAL_INT(p, "heightwhenhidden", p->height_when_hidden);
}

static void
set_icon_size( GtkScaleButton* spin,  Panel* p  )
{
    p->icon_size = (int)gtk_scale_button_get_value(spin);
    gchar* str = g_strdup_printf("%d",p->icon_size);
    gtk_button_set_label(GTK_BUTTON(spin),str);
    g_free(str);
    panel_set_panel_configuration_changed(p);
    UPDATE_GLOBAL_INT(p, "iconsize", p->icon_size);
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
            config_setting_t *s = g_object_get_qdata(G_OBJECT(pl), lxpanel_plugin_qconf);
            GtkBox *box = GTK_BOX(panel->priv->box);
            /* Only honor "stretch" if allowed by the plugin. */
            expand = ! expand;
            gtk_list_store_set( GTK_LIST_STORE(model), &it, COL_EXPAND, expand, -1 );

            /* Query the old packing of the plugin widget.
             * Apply the new packing with only "expand" modified. */
            gtk_box_query_child_packing( box, pl, &old_expand, &fill, &padding, &pack_type );
            gtk_box_set_child_packing( box, pl, expand, fill, padding, pack_type );
            if (expand)
                config_group_set_int(s, "expand", 1);
            else
                config_setting_remove(s, "expand");
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
        config_setting_t *cfg;

        cfg = config_group_add_subgroup(config_root_setting(p->priv->config),
                                        "Plugin");
        gtk_tree_model_get( model, &it, 1, &type, -1 );
        config_group_set_string(cfg, "type", type);
        if ((pl = lxpanel_add_plugin(p, type, cfg, -1)))
        {
            gboolean expand;

            panel_config_save(p->priv);

            plugin_widget_set_background(pl, p);
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
            panel_update_background(p->priv);
        }
        else /* free unused setting */
            config_setting_destroy(cfg);
        g_free( type );
    }
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

        config_setting_destroy(g_object_get_qdata(G_OBJECT(pl), lxpanel_plugin_qconf));
        /* reset conf pointer because the widget still may be referenced by configurator */
        g_object_set_qdata(G_OBJECT(pl), lxpanel_plugin_qconf, NULL);
        gtk_widget_destroy(pl);
        panel_config_save(p->priv);
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
            config_setting_t *s;
            gtk_tree_model_get( model, &it, COL_DATA, &pl, -1 );
            gtk_list_store_move_before( GTK_LIST_STORE( model ),
                                        &it, &prev );

            i = get_widget_index(panel, pl);
            s = g_object_get_qdata(G_OBJECT(pl), lxpanel_plugin_qconf);
            /* reorder in config, 0 is Global */
            if (i == 0)
                i = 1;
            config_setting_move_elem(s, config_setting_get_parent(s), i);
            /* reorder in panel */
            gtk_box_reorder_child(GTK_BOX(panel->priv->box), pl, i - 1);
            panel_config_save(panel->priv);
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
    config_setting_t *s;
    int i;

    SimplePanel* panel = (SimplePanel*) g_object_get_data( G_OBJECT(view), "panel" );

    if( ! gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
        return;
    next = it;

    if( ! gtk_tree_model_iter_next( model, &next) )
        return;

    gtk_tree_model_get( model, &it, COL_DATA, &pl, -1 );

    gtk_list_store_move_after( GTK_LIST_STORE( model ), &it, &next );

    i = get_widget_index(panel, pl) + 1;
    s = g_object_get_qdata(G_OBJECT(pl), lxpanel_plugin_qconf);
    /* reorder in config, 0 is Global */
    config_setting_move_elem(s, config_setting_get_parent(s), i + 1);
    /* reorder in panel */
    gtk_box_reorder_child(GTK_BOX(panel->priv->box), pl, i);
    panel_config_save(panel->priv);
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

static void
update_toggle_button(GtkWidget *w, gboolean n)
{
    gboolean c;

    ENTER;
    /* this trick will trigger "changed" signal even if active entry is
     * not actually changing */
    c = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
    if (c == n) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), !n);
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), n);
    RET();
}

static void on_app_chooser_destroy(GtkComboBox *fm, gpointer _unused)
{
    gboolean is_changed;
    GAppInfo *app = fm_app_chooser_combo_box_dup_selected_app(fm, &is_changed);
    if(app)
    {
        if(is_changed)
            g_app_info_set_as_default_for_type(app, "inode/directory", NULL);
        g_object_unref(app);
    }
}

void panel_configure( SimplePanel* panel, int sel_page )
{
    Panel *p = panel->priv;
    GtkBuilder* builder;
    GtkWidget *w, *w2, *w3, *popover , *tint_clr;
    FmMimeType *mt;
    GtkComboBox *fm;
    GdkScreen *screen;
    gint monitors;

    if( p->pref_dialog )
    {
        panel_adjust_geometry_terminology(p);
        gtk_window_present(GTK_WINDOW(p->pref_dialog));
        return;
    }

    builder = gtk_builder_new();
    if( !gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/panel-pref.ui", NULL) )
    {
        g_object_unref(builder);
        return;
    }

    p->pref_dialog = (GtkWidget*)gtk_builder_get_object( builder, "panel_pref" );
    gtk_window_set_transient_for(GTK_WINDOW(p->pref_dialog), GTK_WINDOW(panel));
    g_signal_connect(p->pref_dialog, "response", G_CALLBACK(response_event), p);
    g_object_add_weak_pointer( G_OBJECT(p->pref_dialog), (gpointer) &p->pref_dialog );
    gtk_window_set_position( GTK_WINDOW(p->pref_dialog), GTK_WIN_POS_CENTER );
    panel_apply_icon(GTK_WINDOW(p->pref_dialog));

    /* position */
    w3 = w = (GtkWidget*)gtk_builder_get_object( builder, "edge-button");
    w2 = (GtkWidget*)gtk_builder_get_object( builder, "edge-box");
    popover = gtk_popover_new(NULL);
    gtk_container_add(GTK_CONTAINER(popover),w2);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(w),popover);
    w = (GtkWidget*)gtk_builder_get_object( builder, "edge_bottom" );
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), edge_selector(p, GTK_POS_BOTTOM));
    if (edge_selector(p, GTK_POS_BOTTOM))
        gtk_button_set_label(GTK_BUTTON(w3),gtk_button_get_label(GTK_BUTTON(w)));
    g_signal_connect(w, "toggled", G_CALLBACK(edge_bottom_toggle), panel);
    g_object_set_data(G_OBJECT(w), "edge-button", w3);
    w = (GtkWidget*)gtk_builder_get_object( builder, "edge_top" );
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), edge_selector(p, GTK_POS_TOP));
    if (edge_selector(p, GTK_POS_TOP))
        gtk_button_set_label(GTK_BUTTON(w3),gtk_button_get_label(GTK_BUTTON(w)));
    g_signal_connect(w, "toggled", G_CALLBACK(edge_top_toggle), panel);
    g_object_set_data(G_OBJECT(w), "edge-button", w3);
    w = (GtkWidget*)gtk_builder_get_object( builder, "edge_left" );
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), edge_selector(p, GTK_POS_LEFT));
    if (edge_selector(p, GTK_POS_LEFT))
        gtk_button_set_label(GTK_BUTTON(w3),gtk_button_get_label(GTK_BUTTON(w)));
    g_signal_connect(w, "toggled", G_CALLBACK(edge_left_toggle), panel);
    g_object_set_data(G_OBJECT(w), "edge-button", w3);
    w = (GtkWidget*)gtk_builder_get_object( builder, "edge_right" );
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), edge_selector(p, GTK_POS_RIGHT));
    if (edge_selector(p, GTK_POS_RIGHT))
        gtk_button_set_label(GTK_BUTTON(w3),gtk_button_get_label(GTK_BUTTON(w)));
    g_signal_connect(w, "toggled", G_CALLBACK(edge_right_toggle), panel);
    g_object_set_data(G_OBJECT(w), "edge-button", w3);

    /* monitor */
    monitors = 1;
    screen = gdk_screen_get_default();
    if(screen) monitors = gdk_screen_get_n_monitors(screen);
    g_assert(monitors >= 1);
    w = (GtkWidget*)gtk_builder_get_object( builder, "monitor" );
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), 1, monitors);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), p->monitor + 1);
    gtk_widget_set_sensitive(w, monitors > 1);
    g_signal_connect(w, "value-changed", G_CALLBACK(set_monitor), panel);

    /* alignment */
    w3 = w = (GtkWidget*)gtk_builder_get_object( builder, "alignment-button");
    w2 = (GtkWidget*)gtk_builder_get_object( builder, "alignment-box");
    popover = gtk_popover_new(NULL);
    gtk_container_add(GTK_CONTAINER(popover),w2);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(w),popover);
    p->alignment_left_label = w = (GtkWidget*)gtk_builder_get_object( builder, "alignment_left" );
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), (p->allign == PANEL_ALLIGN_LEFT));
    g_object_set_data(G_OBJECT(w), "alignment-button", w3);
    g_signal_connect(w, "toggled", G_CALLBACK(align_left_toggle), panel);
    if ((p->allign == PANEL_ALLIGN_LEFT))
        gtk_button_set_label(GTK_BUTTON(w3),gtk_button_get_label(GTK_BUTTON(w)));
    w = (GtkWidget*)gtk_builder_get_object( builder, "alignment_center" );
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), (p->allign == PANEL_ALLIGN_CENTER));
    g_object_set_data(G_OBJECT(w), "alignment-button", w3);
    g_signal_connect(w, "toggled", G_CALLBACK(align_center_toggle), panel);
    if ((p->allign == PANEL_ALLIGN_CENTER))
        gtk_button_set_label(GTK_BUTTON(w3),gtk_button_get_label(GTK_BUTTON(w)));
    p->alignment_right_label = w = (GtkWidget*)gtk_builder_get_object( builder, "alignment_right" );
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), (p->allign == PANEL_ALLIGN_RIGHT));
    g_object_set_data(G_OBJECT(w), "alignment-button", w3);
    g_signal_connect(w, "toggled", G_CALLBACK(align_right_toggle), panel);
    if ((p->allign == PANEL_ALLIGN_RIGHT))
        gtk_button_set_label(GTK_BUTTON(w3),gtk_button_get_label(GTK_BUTTON(w)));

    /* margin */
    p->margin_control = w = (GtkWidget*)gtk_builder_get_object( builder, "margin" );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), p->margin);
    gtk_widget_set_sensitive(p->margin_control, (p->allign != PANEL_ALLIGN_CENTER));
    g_signal_connect( w, "value-changed",
                      G_CALLBACK(set_margin), panel);

    /* size */
    p->width_control = w = (GtkWidget*)gtk_builder_get_object( builder, "scale-width" );
    gtk_widget_set_sensitive( w, p->widthtype != STRUT_DYNAMIC );
    gint upper = 0;
    if( p->widthtype == STRUT_PERCENT)
        upper = 100;
    else if( p->widthtype == STRUT_PIXEL)
        upper = (((p->edge == GTK_POS_TOP) || (p->edge == GTK_POS_BOTTOM)) ? gdk_screen_width() : gdk_screen_height());
    simple_panel_scale_button_set_range(GTK_SCALE_BUTTON(w),0,upper);
    simple_panel_scale_button_set_value_labeled( GTK_SCALE_BUTTON(w), p->width );
    g_signal_connect( w, "value-changed", G_CALLBACK(set_width), panel );

    w = (GtkWidget*)gtk_builder_get_object( builder, "width_unit" );
    update_opt_menu( w, p->widthtype - 1 );
    g_object_set_data(G_OBJECT(w), "scale-width", p->width_control );
    g_signal_connect( w, "changed",
                     G_CALLBACK(set_strut_type), panel);

    p->height_control = w = (GtkWidget*)gtk_builder_get_object( builder, "scale-height" );
    simple_panel_scale_button_set_range(GTK_SCALE_BUTTON(w),PANEL_HEIGHT_MIN,PANEL_HEIGHT_MAX);
    simple_panel_scale_button_set_value_labeled( GTK_SCALE_BUTTON(w), p->height);
    g_signal_connect( w, "value-changed", G_CALLBACK(set_height), panel );

    w = (GtkWidget*)gtk_builder_get_object( builder, "scale-iconsize" );
    simple_panel_scale_button_set_range(GTK_SCALE_BUTTON(w),PANEL_HEIGHT_MIN,PANEL_HEIGHT_MAX);
    simple_panel_scale_button_set_value_labeled( GTK_SCALE_BUTTON(w), p->icon_size );
    g_signal_connect( w, "value-changed", G_CALLBACK(set_icon_size), p );

    /* properties */

    /* Explaination from Ruediger Arp <ruediger@gmx.net>:
        "Set Dock Type", it is referring to the behaviour of
        dockable applications such as those found in WindowMaker (e.g.
        http://www.cs.mun.ca/~gstarkes/wmaker/dockapps ) and other
        lightweight window managers. These dockapps are probably being
        treated in some special way.
    */
    w = (GtkWidget*)gtk_builder_get_object( builder, "as_dock" );
    update_toggle_button( w, p->setdocktype );
    g_signal_connect( w, "toggled",
                      G_CALLBACK(set_dock_type), panel );

    /* Explaination from Ruediger Arp <ruediger@gmx.net>:
        "Set Strut": Reserve panel's space so that it will not be
        covered by maximazied windows.
        This is clearly an option to avoid the panel being
        covered/hidden by other applications so that it always is
        accessible. The panel "steals" some screen estate which cannot
        be accessed by other applications.
        GNOME Panel acts this way, too.
    */
    w = (GtkWidget*)gtk_builder_get_object( builder, "reserve_space" );
    update_toggle_button( w, p->setstrut );
    g_signal_connect( w, "toggled",
                      G_CALLBACK(set_strut), panel );

    w = (GtkWidget*)gtk_builder_get_object( builder, "autohide" );
    update_toggle_button( w, p->autohide );
    g_signal_connect( w, "toggled",
                      G_CALLBACK(set_autohide), panel );

    w = (GtkWidget*)gtk_builder_get_object( builder, "height_when_minimized" );
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), p->height_when_hidden);
    g_signal_connect( w, "value-changed",
                      G_CALLBACK(set_height_when_minimized), panel);

    /* background */
    {
        GtkWidget* type_list;
        GtkIconInfo* info;
        tint_clr = w = (GtkWidget*)gtk_builder_get_object( builder, "tint_clr" );
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(w), &p->gtintcolor);
        if (p->background != PANEL_BACKGROUND_CUSTOM_COLOR )
            gtk_widget_set_sensitive( w, FALSE );
        g_signal_connect( w, "color-set", G_CALLBACK( on_tint_color_set ), p );
        type_list = (GtkWidget*)gtk_builder_get_object( builder, "background_type_combo" );
        update_opt_menu( type_list, p->background);
        g_object_set_data(G_OBJECT(type_list), "tint_clr", tint_clr);

        w = (GtkWidget*)gtk_builder_get_object( builder, "img_file" );
        g_object_set_data(G_OBJECT(type_list), "img_file", w);
        g_signal_connect( type_list, "changed",
                         G_CALLBACK(set_background_type), panel);
        if (p->background_file != NULL)
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(w), p->background_file);
        else if ((info = gtk_icon_theme_lookup_icon(p->icon_theme, "lxpanel-background", 0, 0)))
        {
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(w), gtk_icon_info_get_filename(info));
            g_object_unref(info);
        }

        if (p->background != PANEL_BACKGROUND_CUSTOM_IMAGE)
            gtk_widget_set_sensitive( w, FALSE);
        g_object_set_data( G_OBJECT(w), "bg_image", type_list );
        g_signal_connect( w, "file-set", G_CALLBACK (background_changed), p);
    }
    /* font color */
    w = (GtkWidget*)gtk_builder_get_object( builder, "font_clr" );
    gtk_color_chooser_set_rgba( GTK_COLOR_CHOOSER(w), &p->gfontcolor );
    g_signal_connect( w, "color-set", G_CALLBACK( on_font_color_set ), p );

    w2 = (GtkWidget*)gtk_builder_get_object( builder, "use_font_clr" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(w2), p->usefontcolor );
    g_object_set_data( G_OBJECT(w2), "clr", w );
    g_signal_connect(w2, "toggled", G_CALLBACK(on_use_font_color_toggled), p);
    if( ! p->usefontcolor )
        gtk_widget_set_sensitive( w, FALSE );

    /* font size */
    w = (GtkWidget*)gtk_builder_get_object( builder, "font_size" );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(w), p->fontsize );
    g_signal_connect( w, "value-changed",
                      G_CALLBACK(on_font_size_set), p);

    w2 = (GtkWidget*)gtk_builder_get_object( builder, "use_font_size" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(w2), p->usefontsize );
    g_object_set_data( G_OBJECT(w2), "clr", w );
    g_signal_connect(w2, "toggled", G_CALLBACK(on_use_font_size_toggled), p);
    if( ! p->usefontsize )
        gtk_widget_set_sensitive( w, FALSE );

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
    /* advanced, applications */
    mt = fm_mime_type_from_name("inode/directory");
    fm = GTK_COMBO_BOX(gtk_builder_get_object(builder, "fm_combobox"));
    fm_app_chooser_combo_box_setup_for_mime_type(fm, mt);
    fm_mime_type_unref(mt);
    g_signal_connect(fm, "destroy", G_CALLBACK(on_app_chooser_destroy), NULL);

    w = (GtkWidget*)gtk_builder_get_object( builder, "term" );
    if (fm_config->terminal)
        gtk_entry_set_text( GTK_ENTRY(w), fm_config->terminal );
    g_signal_connect( w, "focus-out-event",
                      G_CALLBACK(on_entry_focus_out),
                      &fm_config->terminal);

    /* If we are under LXSession, setting logout command is not necessary. */
    w = (GtkWidget*)gtk_builder_get_object( builder, "logout" );
    if( getenv("_LXSESSION_PID") ) {
        gtk_widget_hide( w );
        w = (GtkWidget*)gtk_builder_get_object( builder, "logout_label" );
        gtk_widget_hide( w );
    }
    else {
        if(logout_cmd)
            gtk_entry_set_text( GTK_ENTRY(w), logout_cmd );
        g_signal_connect( w, "focus-out-event",
                        G_CALLBACK(on_entry_focus_out_old),
                        &logout_cmd);
    }

    panel_adjust_geometry_terminology(p);
    gtk_widget_show(GTK_WIDGET(p->pref_dialog));
    w = (GtkWidget*)gtk_builder_get_object( builder, "notebook" );
    gtk_notebook_set_current_page( GTK_NOTEBOOK(w), sel_page );

    g_object_unref(builder);
}

void panel_config_save( Panel* p )
{
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
    save_global_config();
    p->config_changed = 0;
}

void lxpanel_config_save(SimplePanel *p)
{
    panel_config_save(p->priv);
}

void restart(void)
{
    /* This is defined in panel.c */
    extern gboolean is_restarting;
    ENTER;
    is_restarting = TRUE;

    gtk_main_quit();
    RET();
}

void logout(void)
{
    const char* l_logout_cmd = logout_cmd;
    /* If LXSession is running, _LXSESSION_PID will be set */
    if( ! l_logout_cmd && getenv("_LXSESSION_PID") )
        l_logout_cmd = "lxsession-logout";

    if( l_logout_cmd )
        fm_launch_command_simple(NULL, NULL, 0, l_logout_cmd, NULL);
    else
        fm_show_error(NULL, NULL, _("Logout command is not set"));
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
    panel_config_save(panel);
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
    gtk_widget_show(dlg);
    lxpanel_plugin_popup_set_position_helper(panel, p, dlg, &x, &y);
    gdk_window_move(gtk_widget_get_window(dlg), x, y);

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

#define COMMAND_GROUP "Command"

void load_global_config()
{
    GKeyFile* kf = g_key_file_new();
    char* file = NULL;
    gboolean loaded = FALSE;
    const gchar * const * dir = g_get_system_config_dirs();

    /* try to load system config file first */
    if (dir) while (dir[0] && !loaded)
    {
        g_free(file);
        file = _system_config_file_name(dir[0], "config");
        if (g_key_file_load_from_file(kf, file, 0, NULL))
            loaded = TRUE;
        dir++;
    }
    if (!loaded) /* fallback to old config place for backward compatibility */
    {
        g_free(file);
        file = _old_system_config_file_name("config");
        if (g_key_file_load_from_file(kf, file, 0, NULL))
            loaded = TRUE;
    }
    /* now try to load user config file */
    g_free(file);
    file = _user_config_file_name("config", NULL);
    if (g_key_file_load_from_file(kf, file, 0, NULL))
        loaded = TRUE;
    g_free(file);

    if( loaded )
    {
        char *fm, *tmp;
        GList *apps, *l;

        logout_cmd = g_key_file_get_string( kf, COMMAND_GROUP, "Logout", NULL );
        /* check for terminal setting on upgrade */
        if (fm_config->terminal == NULL)
        {
            fm_config->terminal = g_key_file_get_string(kf, COMMAND_GROUP,
                                                        "Terminal", NULL);
            if (fm_config->terminal != NULL) /* setting changed, save it */
                fm_config_save(fm_config, NULL);
        }
        /* this is heavy but fortunately it will be ran only once: on upgrade */
        fm = g_key_file_get_string(kf, COMMAND_GROUP, "FileManager", NULL);
        if (fm)
        {
            tmp = strchr(fm, ' '); /* chop params */
            if (tmp)
                *tmp = '\0';
            tmp = strrchr(fm, '/'); /* use only basename */
            if (tmp)
                tmp++;
            else
                tmp = fm;
            tmp = g_strdup_printf("%s.desktop", tmp); /* generate desktop id */
            g_free(fm);
            apps = g_app_info_get_all_for_type("inode/directory");
            for (l = apps; l; l = l->next) /* scan all known applications */
                if (strcmp(tmp, g_app_info_get_id(l->data)) == 0)
                    break;
            if (l != NULL) /* found */
                g_app_info_set_as_default_for_type(l->data, "inode/directory",
                                                   NULL);
            else
                g_warning("the %s is not valid desktop id of file manager", tmp);
            for (l = apps; l; l = l->next) /* free retrieved data */
                g_object_unref(l->data);
            g_list_free(apps);
            g_free(tmp);
            save_global_config();
        }
    }
    g_key_file_free( kf );
}

static void save_global_config()
{
    char* file = _user_config_file_name("config", NULL);
    FILE* f = fopen( file, "w" );
    if( f )
    {
        fprintf( f, "[" COMMAND_GROUP "]\n");
        if( logout_cmd )
            fprintf( f, "Logout=%s\n", logout_cmd );
        fclose( f );
    }
    g_free(file);
}

void free_global_config()
{
    g_free( logout_cmd );
}

/* this is dirty and should be removed later */
const char*
lxpanel_get_file_manager()
{
    GAppInfo *app = g_app_info_get_default_for_type("inode/directory", TRUE);
    static char *exec = NULL;
    const char *c, *x;

    if (!app)
        return "pcmanfm %s";
    c = g_app_info_get_commandline(app);
    x = strchr(c, ' '); /* skip all arguments */
    g_free(exec);
    if (x)
        exec = g_strndup(c, x - c);
    else
        exec = g_strdup(c);
    return exec;
}

/* vim: set sw=4 et sts=4 : */
