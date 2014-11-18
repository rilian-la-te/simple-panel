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
#include <math.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <libfm/fm-gtk.h>

#include "misc.h"
#include "private.h"
#include "css.h"

#include "dbg.h"

/* data used by themed images buttons */
typedef struct {
    GIcon *icon;
    guint icon_changed_handler;
    GdkRGBA hicolor;
    gint size; /* desired size */
    SimplePanel *panel;
} ImgData;

static GQuark img_data_id = 0;

static void on_theme_changed(GtkWidget* img, GObject* object);
static void _gtk_image_set_from_file_gicon(GtkWidget *img, ImgData *data);
static GtkWidget *gtk_image_new_for_gicon(SimplePanel* p, GIcon *icon, gint size);

int buf_gets( char* buf, int len, char **fp )
{
    char* p;
    int i = 0;
    if( !fp || !(p = *fp) || !**fp )
    {
        buf[0] = '\0';
        return 0;
    }

    do
    {
        if( G_LIKELY( i < len ) )
        {
            buf[i] = *p;
            ++i;
        }
        if( G_UNLIKELY(*p == '\n') )
        {
            ++p;
            break;
        }
    }while( *(++p) );
    buf[i] = '\0';
    *fp = p;
    return i;
}

extern int
lxpanel_put_line(FILE* fp, const char* format, ...)
{
    static int indent = 0;
    int i, ret;
    va_list args;

    if( strchr(format, '}') )
        --indent;

    for( i = 0; i < indent; ++i )
        fputs( "    ", fp );

    va_start (args, format);
    ret = vfprintf (fp, format, args);
    va_end (args);

    if( strchr(format, '{') )
        ++indent;
    fputc( '\n', fp );  /* add line break */
    return (ret + 1);
}

extern  int
lxpanel_get_line(char**fp, line *s)
{
    gchar *tmp, *tmp2;

    s->type = LINE_NONE;
    if (!fp)
        RET(s->type);
    while (buf_gets(s->str, s->len, fp)) {

        g_strstrip(s->str);

        if (s->str[0] == '#' || s->str[0] == 0) {
            continue;
        }
        if (!g_ascii_strcasecmp(s->str, "}")) {
            s->type = LINE_BLOCK_END;
            break;
        }

        s->t[0] = s->str;
        for (tmp = s->str; isalnum(*tmp); tmp++);
        for (tmp2 = tmp; isspace(*tmp2); tmp2++);
        if (*tmp2 == '=') {
            for (++tmp2; isspace(*tmp2); tmp2++);
            s->t[1] = tmp2;
            *tmp = 0;
            s->type = LINE_VAR;
        } else if  (*tmp2 == '{') {
            *tmp = 0;
            s->type = LINE_BLOCK_START;
        } else {
            g_warning( "parser: unknown token: '%c'", *tmp2);
        }
        break;
    }
    return s->type;
}

static void
calculate_width(int scrw, int wtype, int allign, int margin,
      int *panw, int *x)
{
    if (wtype == PANEL_SIZE_PERCENT) {
        /* sanity check */
        if (*panw > 100)
            *panw = 100;
        else if (*panw < 0)
            *panw = 1;
        *panw = ((gfloat) scrw * (gfloat) *panw) / 100.0;
    }
    if (allign != PANEL_ALLIGN_CENTER) {
        if (margin > scrw) {
            g_warning( "margin is bigger then edge size %d > %d. Ignoring margin",
                  margin, scrw);
            margin = 0;
        }
	*panw = MIN(scrw - margin, *panw);
    }
    if (allign == PANEL_ALLIGN_LEFT)
        *x += margin;
    else if (allign == PANEL_ALLIGN_RIGHT) {
        *x += scrw - *panw - margin;
        if (*x < 0)
            *x = 0;
    } else if (allign == PANEL_ALLIGN_CENTER)
        *x += (scrw - *panw) / 2;
    RET();
}


void _calculate_position(SimplePanel *panel, GdkRectangle* rect)
{
    Panel *np = panel->priv;
    GdkScreen *screen;
    GdkRectangle marea;

    ENTER;
    /* FIXME: Why this doesn't work? */
    /* if you are still going to use this, be sure to update it to take into
       account multiple monitors */
    if (0)  {
//        if (np->curdesk < np->wa_len/4) {
//        marea.x = np->workarea[np->curdesk*4 + 0];
//        marea.y = np->workarea[np->curdesk*4 + 1];
//        marea.width  = np->workarea[np->curdesk*4 + 2];
//        marea.height = np->workarea[np->curdesk*4 + 3];
    } else {
        screen = gdk_screen_get_default();
        if (np->monitor < 0) /* all monitors */
        {
            marea.x = 0;
            marea.y = 0;
            marea.width = gdk_screen_get_width(screen);
            marea.height = gdk_screen_get_height(screen);
        }
        else if (np->monitor < gdk_screen_get_n_monitors(screen))
            gdk_screen_get_monitor_geometry(screen,np->monitor,&marea);
        else
        {
            marea.x = 0;
            marea.y = 0;
            marea.width = 0;
            marea.height = 0;
        }
    }

    if (np->edge == PANEL_EDGE_TOP || np->edge == PANEL_EDGE_BOTTOM) {
        rect->width = np->width;
        rect->x = marea.x;
        calculate_width(marea.width, np->widthtype, np->allign, np->margin,
              &rect->width, &rect->x);
        rect->height = ((( ! np->autohide) || (np->visible)) ? np->height : np->height_when_hidden);
        rect->y = marea.y + ((np->edge == PANEL_EDGE_TOP) ? 0 : (marea.height - rect->height));

    } else {
        rect->height = np->height;
        rect->y = marea.y;
        calculate_width(marea.height, np->widthtype, np->allign, np->margin,
              &rect->height, &rect->y);
        rect->width = ((( ! np->autohide) || (np->visible)) ? np->height : np->height_when_hidden);
        rect->x = marea.x + ((np->edge == PANEL_EDGE_LEFT) ? 0 : (marea.width - rect->width));
    }
    RET();
}

void calculate_position(SimplePanel *np)
{
    GdkRectangle rect;
    rect.width = np->priv->aw;
    rect.height = np->priv->ah;
    _calculate_position(np, &rect);
    np->priv->aw = rect.width;
    np->priv->ah = rect.height;
    np->priv->ax = rect.x;
    np->priv->ay = rect.y;
}

gchar *
expand_tilda(const gchar *file)
{
    ENTER;
    RET((file[0] == '~') ?
        g_strdup_printf("%s%s", getenv("HOME"), file+1)
        : g_strdup(file));

}

/*
 * SuxPanel version 0.1
 * Copyright (c) 2003 Leandro Pereira <leandro@linuxmag.com.br>
 *
 * This program may be distributed under the terms of GNU General
 * Public License version 2. You should have received a copy of the
 * license with this program; if not, please consult http://www.fsf.org/.
 *
 * This program comes with no warranty. Use at your own risk.
 *
 */

/* DestroyNotify handler for image data in _gtk_image_new_from_file_scaled. */
static void img_data_free(ImgData * data)
{
    if (data->icon != NULL)
        g_object_unref(data->icon);
    if (data->panel != NULL)
    {
        g_object_remove_weak_pointer(G_OBJECT(data->panel), (gpointer *)&data->panel);
        g_signal_handler_disconnect(data->panel, data->icon_changed_handler);
    }
    g_free(data);
}

/* Handler for "changed" signal in _gtk_image_new_from_file_scaled. */
static void on_theme_changed(GtkWidget * img, GObject *object)
{
    ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(img), img_data_id);
    _gtk_image_set_from_file_gicon(img, data);
}

/* consumes reference on icon */
static void _simple_panel_button_set_icon(GtkWidget* btn, GIcon* icon, gint size)
{
    /* Locate the image within the button. */
    GtkWidget * child = gtk_bin_get_child(GTK_BIN(btn));
    GtkWidget * img = NULL;
    if (GTK_IS_IMAGE(child))
        img = child;
    else if (GTK_IS_BOX(child))
    {
        GList * children = gtk_container_get_children(GTK_CONTAINER(child));
        img = GTK_WIDGET(GTK_IMAGE(children->data));
        g_list_free(children);
    }

    if (img != NULL)
    {
        ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(img), img_data_id);
        if (size <= 0)
            size = data->size;
        if (icon != data->icon || size != data->size) /* something was changed */
        {
            if (data->icon != NULL)
                g_object_unref(data->icon);
            data->icon = icon;
            data->size = size;
            _gtk_image_set_from_file_gicon(img, data);
        }
        else
            g_object_unref(icon);
    }
    else
        g_object_unref(icon);
}

void simple_panel_button_set_icon(GtkWidget* btn, const gchar *name, gint size)
{
    _simple_panel_button_set_icon(btn, g_icon_new_for_string(name,NULL), size);
}

GtkWidget* simple_panel_image_new_for_icon(SimplePanel * p,const gchar *name, gint height)
{
    return gtk_image_new_for_gicon(p,g_icon_new_for_string(name,NULL),height);
}

GtkWidget* simple_panel_image_new_for_gicon(SimplePanel * p,GIcon *icon, gint height)
{
    return gtk_image_new_for_gicon(p,icon,height);
}

static void _gtk_image_set_from_file_gicon(GtkWidget *img, ImgData *data)
{
    if (data->icon)
    {
        gtk_image_set_from_gicon(GTK_IMAGE(img),data->icon,GTK_ICON_SIZE_INVALID);
        gtk_image_set_pixel_size(GTK_IMAGE(img),data->panel->priv->icon_size);
    }
}

gboolean simple_panel_image_change_icon(GtkWidget *img, const gchar *name)
{
    return simple_panel_image_change_gicon(img,g_icon_new_for_string(name,NULL));
}

gboolean simple_panel_image_change_gicon(GtkWidget *img, GIcon *icon)
{
    ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(img), img_data_id);

    g_return_val_if_fail(data != NULL && icon != NULL, FALSE);
    if(data == NULL || icon == NULL)
        return FALSE;
    if (data->icon != NULL)
        g_object_unref(data->icon);
    data->icon=icon;
    _gtk_image_set_from_file_gicon(img, data);
    return TRUE;
}

static GtkWidget* gtk_image_new_for_gicon(SimplePanel* p,GIcon * icon, gint size)
{
    g_return_val_if_fail(icon != NULL, NULL);
    GtkWidget * img = gtk_image_new();
    ImgData * data = g_new0(ImgData, 1);
    data->icon = icon;
    data->size = size;
    if (img_data_id == 0)
        img_data_id = g_quark_from_static_string("ImgData");
    g_object_set_qdata_full(G_OBJECT(img), img_data_id, data, (GDestroyNotify) img_data_free);
    if (p && size < 0)
    {
        data->panel = p;
        data->icon_changed_handler = g_signal_connect_swapped(p, "notify::"PANEL_PROP_ICON_SIZE,
                                                G_CALLBACK(on_theme_changed), img);
        /* it is in fact not required if image is panel child but let be safe */
        g_object_add_weak_pointer(G_OBJECT(p), (gpointer *)&data->panel);
    }
    _gtk_image_set_from_file_gicon(img, data);
    return img;
}

/* consumes reference on icon */
static GtkWidget *_simple_panel_button_new_for_icon(SimplePanel* panel,GIcon *icon,
                                               gint size, const GdkRGBA* color,
                                               const gchar *label)
{
    GtkWidget * event_box = gtk_button_new();
    css_apply_with_class(event_box,NULL,GTK_STYLE_CLASS_BUTTON,TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(event_box), 0);
    gtk_widget_set_can_focus(event_box, FALSE);
    gtk_widget_set_has_window(event_box,FALSE);
    GtkWidget * image = gtk_image_new_for_gicon(panel,icon, size);
    if (label == NULL)
        gtk_container_add(GTK_CONTAINER(event_box), image);
    else
    {
        GtkWidget * inner = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_container_set_border_width(GTK_CONTAINER(inner), 0);
        gtk_widget_set_can_focus(inner, FALSE);
        gtk_container_add(GTK_CONTAINER(event_box), inner);

        gtk_box_pack_start(GTK_BOX(inner), image, FALSE, FALSE, 0);

        GtkWidget * lbl = gtk_label_new(label);
        gtk_box_pack_end(GTK_BOX(inner), lbl, FALSE, FALSE, 0);
    }
    gtk_widget_show_all(event_box);
    gchar* css;
    ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(image), img_data_id);
    gchar* tmp = gdk_rgba_to_string(color);
    gdk_rgba_parse(&data->hicolor,tmp);
    g_free(tmp);
    css = css_generate_panel_icon_button(data->hicolor);
    css_apply_with_class(event_box,css,"-panel-icon-button",FALSE);
    g_free(css);
    return event_box;
}

GtkWidget *simple_panel_button_new_for_icon(SimplePanel *panel, const gchar *name, GdkRGBA *color, const gchar *label)
{
    GdkRGBA fallback = {1,1,1,0.15};
    return _simple_panel_button_new_for_icon(panel,g_icon_new_for_string(name,NULL),
                                        -1, (color != NULL) ? color : &fallback, label);
}

void
get_button_spacing(GtkRequisition *req, GtkContainer *parent, gchar *name)
{
    GtkWidget *b;
    ENTER;
    b = gtk_button_new();
    gtk_widget_set_name(GTK_WIDGET(b), name);
    gtk_widget_set_can_focus(b, FALSE);
    gtk_widget_set_can_default(b, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (b), 0);

    if (parent)
        gtk_container_add(parent, b);

    gtk_widget_show(b);
    gtk_widget_get_preferred_size(b, req, NULL);

    gtk_widget_destroy(b);
    RET();
}

char* translate_exec_to_cmd( const char* exec, const char* icon,
                             const char* title, const char* fpath )
{
    GString* cmd = g_string_sized_new( 256 );
    if (!exec)
	    return NULL;
    for( ; *exec; ++exec )
    {
        if( G_UNLIKELY(*exec == '%') )
        {
            ++exec;
            if( !*exec )
                break;
            switch( *exec )
            {
                case 'c':
                    if( title )
                    {
                        g_string_append( cmd, title );
                    }
                    break;
                case 'i':
                    if( icon )
                    {
                        g_string_append( cmd, "--icon " );
                        g_string_append( cmd, icon );
                    }
                    break;
                case 'k':
                    if( fpath )
                    {
                        char* uri = g_filename_to_uri( fpath, NULL, NULL );
                        g_string_append( cmd, uri );
                        g_free( uri );
                    }
                    break;
                case '%':
                    g_string_append_c( cmd, '%' );
                    break;
            }
        }
        else
            g_string_append_c( cmd, *exec );
    }
    return g_string_free( cmd, FALSE );
}

/*
 * Taken from pcmanfm:
 * Parse Exec command line of app desktop file, and translate
 * it into a real command which can be passed to g_spawn_command_line_async().
 * file_list is a null-terminated file list containing full
 * paths of the files passed to app.
 * returned char* should be freed when no longer needed.
 */
static char* translate_app_exec_to_command_line( const char* pexec,
                                                 GList* file_list )
{
    char* file;
    GList* l;
    gchar *tmp;
    GString* cmd = g_string_new("");
    gboolean add_files = FALSE;

    for( ; *pexec; ++pexec )
    {
        if( *pexec == '%' )
        {
            ++pexec;
            switch( *pexec )
            {
            case 'U':
                for( l = file_list; l; l = l->next )
                {
                    tmp = g_filename_to_uri( (char*)l->data, NULL, NULL );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    if (l->next)
                        g_string_append_c( cmd, ' ' );
                    g_free( file );
                }
                add_files = TRUE;
                break;
            case 'u':
                if( file_list && file_list->data )
                {
                    file = (char*)file_list->data;
                    tmp = g_filename_to_uri( file, NULL, NULL );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_free( file );
                    add_files = TRUE;
                }
                break;
            case 'F':
            case 'N':
                for( l = file_list; l; l = l->next )
                {
                    file = (char*)l->data;
                    tmp = g_shell_quote( file );
                    g_string_append( cmd, tmp );
                    if (l->next)
                        g_string_append_c( cmd, ' ' );
                    g_free( tmp );
                }
                add_files = TRUE;
                break;
            case 'f':
            case 'n':
                if( file_list && file_list->data )
                {
                    file = (char*)file_list->data;
                    tmp = g_shell_quote( file );
                    g_string_append( cmd, tmp );
                    g_free( tmp );
                    add_files = TRUE;
                }
                break;
            case 'D':
                for( l = file_list; l; l = l->next )
                {
                    tmp = g_path_get_dirname( (char*)l->data );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    if (l->next)
                        g_string_append_c( cmd, ' ' );
                    g_free( file );
                }
                add_files = TRUE;
                break;
            case 'd':
                if( file_list && file_list->data )
                {
                    tmp = g_path_get_dirname( (char*)file_list->data );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_free( tmp );
                    add_files = TRUE;
                }
                break;
            case 'c':
                #if 0
                g_string_append( cmd, vfs_app_desktop_get_disp_name( app ) );
                #endif
                break;
            case 'i':
                /* Add icon name */
                #if 0
                if( vfs_app_desktop_get_icon_name( app ) )
                {
                    g_string_append( cmd, "--icon " );
                    g_string_append( cmd, vfs_app_desktop_get_icon_name( app ) );
                }
                #endif
                break;
            case 'k':
                /* Location of the desktop file */
                break;
            case 'v':
                /* Device name */
                break;
            case '%':
                g_string_append_c ( cmd, '%' );
                break;
            case '\0':
                goto _finish;
                break;
            }
        }
        else  /* not % escaped part */
        {
            g_string_append_c ( cmd, *pexec );
        }
    }
_finish:
    if( ! add_files )
    {
        for( l = file_list; l; l = l->next )
        {
            g_string_append_c( cmd, ' ' );
            file = (char*)l->data;
            tmp = g_shell_quote( file );
            g_string_append( cmd, tmp );
            g_free( tmp );
        }
    }

    return g_string_free( cmd, FALSE );
}

gboolean spawn_command_async(GtkWindow *parent_window, gchar const* workdir,
        gchar const* cmd)
{
    GError* err = NULL;
    gchar** argv = NULL;

    g_info("lxpanel: spawning \"%s\"...", cmd);

    g_shell_parse_argv(cmd, NULL, &argv, &err);
    if (!err)
        g_spawn_async(workdir, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err);

    if (err)
    {
        g_warning("%s\n", err->message);
        fm_show_error(parent_window, NULL, err->message);
        g_error_free(err);
    }

    g_strfreev(argv);

    return !err;
}

/* FIXME: this should be replaced with fm_launch_file_simple() */
gboolean lxpanel_launch_app(const char* exec, GList* files, gboolean in_terminal, char const* in_workdir)
{
    GError *error = NULL;
    char* cmd;
    if( ! exec )
        return FALSE;
    cmd = translate_app_exec_to_command_line(exec, files);
    if( in_terminal )
    {
	char * escaped_cmd = g_shell_quote(cmd);
        char* term_cmd;
        const char* term = fm_config->terminal ? fm_config->terminal : "lxterminal";
        if( strstr(term, "%s") )
            term_cmd = g_strdup_printf(term, escaped_cmd);
        else
            term_cmd = g_strconcat( term, " -e ", escaped_cmd, NULL );
	g_free(escaped_cmd);
        if( cmd != exec )
            g_free(cmd);
        cmd = term_cmd;
    }

    spawn_command_async(NULL, in_workdir, cmd);

    g_free(cmd);

    return (error == NULL);
}

void start_panels_from_dir(GtkApplication* app,const char *panel_dir)
{
    GDir* dir = g_dir_open( panel_dir, 0, NULL );
    const gchar* name;

    if( ! dir )
    {
        return;
    }

    while((name = g_dir_read_name(dir)) != NULL)
    {
        char* panel_config = g_build_filename( panel_dir, name, NULL );
        if (strchr(panel_config, '~') == NULL)    /* Skip editor backup files in case user has hand edited in this directory */
        {
            SimplePanel* panel = panel_load(app,panel_config, name );
            if( panel )
                gtk_application_add_window(app,GTK_WINDOW(panel));
        }
        g_free( panel_config );
    }
    g_dir_close( dir );
}

void simple_panel_scale_button_set_range (GtkScaleButton* b, gint lower, gint upper)
{
    gtk_adjustment_set_lower(gtk_scale_button_get_adjustment(b),lower);
    gtk_adjustment_set_upper(gtk_scale_button_get_adjustment(b),upper);
    gtk_adjustment_set_step_increment(gtk_scale_button_get_adjustment(b),1);
    gtk_adjustment_set_page_increment(gtk_scale_button_get_adjustment(b),1);
}

void simple_panel_scale_button_set_value_labeled (GtkScaleButton* b, gint value)
{
    gtk_scale_button_set_value(b,value);
    gchar* str = g_strdup_printf("%d",value);
    gtk_button_set_label(GTK_BUTTON(b),str);
    g_free(str);
}

void simple_panel_add_prop_as_action(GActionMap* map,const char* prop)
{
    GAction* action;
    action = G_ACTION(g_property_action_new(prop,map,prop));
    g_action_map_add_action(map,action);
    g_object_unref(action);
}

void simple_panel_add_gsettings_as_action(GActionMap* map, GSettings* settings,const char* prop)
{
    GAction* action;
    g_settings_bind(settings,prop,G_OBJECT(map),prop,G_SETTINGS_BIND_DEFAULT);
    action = G_ACTION(g_settings_create_action(settings,prop));
    g_action_map_add_action(map,action);
    g_object_unref(action);
}
/* vim: set sw=4 et sts=4 : */
