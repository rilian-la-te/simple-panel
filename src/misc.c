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
    FmIcon *icon;
    guint theme_changed_handler;
    GdkPixbuf* pixbuf;
    GdkPixbuf* hilight;
    gulong hicolor;
    gint size; /* desired size */
} ImgData;

static GQuark img_data_id = 0;

static void on_theme_changed(GtkIconTheme* theme, GtkWidget* img);
static void _gtk_image_set_from_file_scaled(GtkWidget *img, ImgData *data);
static GtkWidget *_gtk_image_new_for_icon(FmIcon *icon, gint size);

pair allign_pair[] = {
    { PANEL_ALLIGN_NONE, "none" },
    { PANEL_ALLIGN_LEFT, "left" },
    { PANEL_ALLIGN_RIGHT, "right" },
    { PANEL_ALLIGN_CENTER, "center"},
    { 0, NULL },
};

pair edge_pair[] = {
    { GTK_POS_LEFT, "left" },
    { GTK_POS_RIGHT, "right" },
    { GTK_POS_TOP, "top" },
    { GTK_POS_BOTTOM, "bottom" },
    { 0, NULL },
};

pair strut_pair[] = {
    { STRUT_FILL, "none" },
    { STRUT_DYNAMIC, "request" },
    { STRUT_PIXEL, "pixel" },
    { STRUT_PERCENT, "percent" },
    { 0, NULL },
};

pair background_pair[] = {
    { PANEL_BACKGROUND_GTK, "gtk-normal" },
    { PANEL_BACKGROUND_DARK, "gtk-dark" },
    { PANEL_BACKGROUND_GNOME, "gtk-gnome-panel" },
    { PANEL_BACKGROUND_CUSTOM_COLOR, "color" },
    { PANEL_BACKGROUND_CUSTOM_IMAGE, "image" },
    { 0, NULL },
};

pair bool_pair[] = {
    { 0, "0" },
    { 1, "1" },
    { 0, NULL },
};

int
str2num(pair *p, const gchar *str, int defval)
{
    ENTER;
    for (;p && p->str; p++) {
        if (!g_ascii_strcasecmp(str, p->str))
            RET(p->num);
    }
    RET(defval);
}

const gchar *
num2str(pair *p, int num, const gchar *defval)
{
    ENTER;
    for (;p && p->str; p++) {
        if (num == p->num)
            RET(p->str);
    }
    RET(defval);
}

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
    if (wtype == STRUT_PERCENT) {
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


void _calculate_position(LXPanel *panel)
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
        screen = gtk_widget_get_screen(GTK_WIDGET(panel));
        g_assert(np->monitor >= 0 && np->monitor < gdk_screen_get_n_monitors(screen));
        gdk_screen_get_monitor_geometry(screen,np->monitor,&marea);
    }

    if (np->edge == GTK_POS_TOP || np->edge == GTK_POS_BOTTOM) {
        np->aw = np->width;
        np->ax = marea.x;
        calculate_width(marea.width, np->widthtype, np->allign, np->margin,
              &np->aw, &np->ax);
        np->ah = ((( ! np->autohide) || (np->visible)) ? np->height : np->height_when_hidden);
        np->ay = marea.y + ((np->edge == GTK_POS_TOP) ? 0 : (marea.height - np->ah));

    } else {
        np->ah = np->width;
        np->ay = marea.y;
        calculate_width(marea.height, np->widthtype, np->allign, np->margin,
              &np->ah, &np->ay);
        np->aw = ((( ! np->autohide) || (np->visible)) ? np->height : np->height_when_hidden);
        np->ax = marea.x + ((np->edge == GTK_POS_LEFT) ? 0 : (marea.width - np->aw));
    }
    //g_debug("%s - x=%d y=%d w=%d h=%d\n", __FUNCTION__, np->ax, np->ay, np->aw, np->ah);
    RET();
}

void calculate_position(Panel *np)
{
    _calculate_position(np->topgwin);
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
    g_object_unref(data->icon);
    if (data->theme_changed_handler != 0)
        g_signal_handler_disconnect(gtk_icon_theme_get_default(), data->theme_changed_handler);
    if (data->pixbuf != NULL)
        g_object_unref(data->pixbuf);
    if (data->hilight != NULL)
        g_object_unref(data->hilight);
    g_free(data);
}

/* Handler for "changed" signal in _gtk_image_new_from_file_scaled. */
static void on_theme_changed(GtkIconTheme * theme, GtkWidget * img)
{
    ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(img), img_data_id);
    _gtk_image_set_from_file_scaled(img, data);
}

/* consumes reference on icon */
static void _lxpanel_button_set_icon(GtkWidget* btn, FmIcon* icon, gint size)
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

        if (icon != data->icon || size != data->size) /* something was changed */
        {
            g_object_unref(data->icon);
            data->icon = icon;
            data->size = size;
            _gtk_image_set_from_file_scaled(img, data);
        }
        else
            g_object_unref(icon);
    }
    else
        g_object_unref(icon);
}

void lxpanel_button_set_icon(GtkWidget* btn, const gchar *name, gint size)
{
    _lxpanel_button_set_icon(btn, fm_icon_from_name(name), size);
}

void lxpanel_button_update_icon(GtkWidget* btn, FmIcon *icon, gint size)
{
    _lxpanel_button_set_icon(btn, g_object_ref(icon), size);
}

static void _gtk_image_set_from_file_scaled(GtkWidget * img, ImgData * data)
{
    if (data->pixbuf != NULL)
    {
        g_object_unref(data->pixbuf);
        data->pixbuf = NULL;
    }

    /* if there is a cached hilighted version of this pixbuf, free it */
    if (data->hilight != NULL)
    {
        g_object_unref(data->hilight);
        data->hilight = NULL;
    }

    if (G_LIKELY(G_IS_THEMED_ICON(data->icon)))
        data->pixbuf = fm_pixbuf_from_icon_with_fallback(data->icon, data->size,
                                                         "application-x-executable");
    else
    {
        char *file = g_icon_to_string(fm_icon_get_gicon(data->icon));
        data->pixbuf = gdk_pixbuf_new_from_file_at_scale(file, -1, data->size, TRUE, NULL);
        g_free(file);
    }

    if (data->pixbuf != NULL)
    {
        /* Set the pixbuf into the image widget. */
        gtk_image_set_from_pixbuf((GtkImage *)img, data->pixbuf);
    }
    else
    {
        /* No pixbuf available.  Set the "missing image" icon. */
        gtk_image_set_from_icon_name(GTK_IMAGE(img), "image-missing", GTK_ICON_SIZE_BUTTON);
    }
}

/* consumes reference on icon */
static GtkWidget *_gtk_image_new_for_icon(FmIcon *icon, gint size)
{
    GtkWidget * img = gtk_image_new();
    ImgData * data = g_new0(ImgData, 1);

    data->icon = icon;
    data->size = size;
    if (img_data_id == 0)
        img_data_id = g_quark_from_static_string("ImgData");
    g_object_set_qdata_full(G_OBJECT(img), img_data_id, data, (GDestroyNotify) img_data_free);
    _gtk_image_set_from_file_scaled(img, data);
    if (G_IS_THEMED_ICON(data->icon))
    {
        /* This image is loaded from icon theme.  Update the image if the icon theme is changed. */
        data->theme_changed_handler = g_signal_connect(gtk_icon_theme_get_default(), "changed", G_CALLBACK(on_theme_changed), img);
    }
    return img;
}

/* parameters width and keep_ratio are unused, kept for backward compatibility */
GtkWidget * _gtk_image_new_from_file_scaled(const gchar * file, gint width, gint height, gboolean keep_ratio)
{
    return _gtk_image_new_for_icon(fm_icon_from_name(file), height);
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


guint32 gcolor2rgb24(GdkRGBA *color)
{
    guint32 i;

    ENTER;

    i = (gint)round(color->red * 0xFF) & 0xFF;
    i <<= 8;
    i |= (gint)round(color->green * 0xFF) & 0xFF;
    i <<= 8;
    i |= (gint)round(color->blue * 0xFF) & 0xFF;
    DBG("i=%x\n", i);
    RET(i);
}

///* Handler for "enter-notify-event" signal on image that has highlighting requested. */
static gboolean fb_button_enter(GtkImage * widget, GdkEventCrossing * event)
{
    if (gtk_image_get_storage_type(widget) == GTK_IMAGE_PIXBUF)
    {
        ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(widget), img_data_id);
        if (data != NULL)
        {
            if (data->hilight == NULL)
            {
                GdkPixbuf * dark = data->pixbuf;
                int height = gdk_pixbuf_get_height(dark);
                int rowstride = gdk_pixbuf_get_rowstride(dark);
                gulong hicolor = data->hicolor;

                GdkPixbuf * light = gdk_pixbuf_add_alpha(dark, FALSE, 0, 0, 0);
                if (light != NULL)
                {
                    guchar extra[3];
                    int i;
                    for (i = 2; i >= 0; i--, hicolor >>= 8)
                        extra[i] = hicolor & 0xFF;

                    guchar * src = gdk_pixbuf_get_pixels(light);
                    guchar * up;
                    for (up = src + height * rowstride; src < up; src += 4)
                    {
                        if (src[3] != 0)
                        {
                            for (i = 0; i < 3; i++)
                            {
                            int value = src[i] + extra[i];
                            if (value > 255) value = 255;
                            src[i] = value;
                            }
                        }
                    }
                data->hilight = light;
                }
            }

        if (data->hilight != NULL)
            gtk_image_set_from_pixbuf(widget, data->hilight);
        }
    }
    return TRUE;
}

///* Handler for "leave-notify-event" signal on image that has highlighting requested. */
static gboolean fb_button_leave(GtkImage * widget, GdkEventCrossing * event, gpointer user_data)
{
    if (gtk_image_get_storage_type(widget) == GTK_IMAGE_PIXBUF)
    {
        ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(widget), img_data_id);
        if ((data != NULL) && (data->pixbuf != NULL))
            gtk_image_set_from_pixbuf(widget, data->pixbuf);
    }
    return TRUE;
}


/* consumes reference on icon */
static GtkWidget *_lxpanel_button_new_for_icon(FmIcon *icon,
                                               gint size, gulong highlight_color,
                                               const gchar *label)
{
    GtkWidget * event_box = gtk_event_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(event_box), 0);
    gtk_widget_set_can_focus(event_box, FALSE);

    GtkWidget * image = _gtk_image_new_for_icon(icon, size);
    if (highlight_color != 0)
    {
        ImgData * data = (ImgData *) g_object_get_qdata(G_OBJECT(image), img_data_id);
        data->hicolor = highlight_color;

        gtk_widget_add_events(event_box, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
        g_signal_connect_swapped(G_OBJECT(event_box), "enter-notify-event", G_CALLBACK(fb_button_enter), image);
        g_signal_connect_swapped(G_OBJECT(event_box), "leave-notify-event", G_CALLBACK(fb_button_leave), image);
    }

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
    return event_box;
}

GtkWidget *lxpanel_button_new_for_icon(LXPanel *panel, const gchar *name, GdkRGBA *color, const gchar *label)
{
    gulong highlight_color = color ? gcolor2rgb24(color) : PANEL_ICON_HIGHLIGHT;
    return _lxpanel_button_new_for_icon(fm_icon_from_name(name),
                                        panel->priv->icon_size, highlight_color, label);
}

GtkWidget *lxpanel_button_new_for_fm_icon(LXPanel *panel, FmIcon *icon, GdkRGBA *color, const gchar *label)
{
    gulong highlight_color = color ? gcolor2rgb24(color) : PANEL_ICON_HIGHLIGHT;
    return _lxpanel_button_new_for_icon(g_object_ref(icon),
                                        panel->priv->icon_size, highlight_color, label);
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
 This function is used to re-create a new box with different
 orientation from the old one, add all children of the old one to
 the new one, and then destroy the old box.
 It's mainly used when we need to change the orientation of the panel or
 any plugin with a layout box. Since GtkHBox cannot be changed to GtkVBox,
 recreating a new box to replace the old one is required.
*/
/* for compatibility with old plugins */
GtkWidget* recreate_box( GtkBox* oldbox, GtkOrientation orientation )
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(oldbox), orientation);
    return GTK_WIDGET(oldbox);
}

/* for compatibility with old plugins */
void show_error( GtkWindow* parent_win, const char* msg )
{
    fm_show_error(parent_win, NULL, msg);
}

/* old plugins compatibility mode, use fm_pixbuf_from_icon_with_fallback() instead */
GdkPixbuf * lxpanel_load_icon(const char * name, int width, int height, gboolean use_fallback)
{
    FmIcon * fm_icon;
    GdkPixbuf * icon = NULL;

    fm_icon = fm_icon_from_name(name ? name : "application-x-executable");
    /* well, we don't use parameter width and not really use cache here */
    icon = fm_pixbuf_from_icon_with_fallback(fm_icon, height,
                            use_fallback ? "application-x-executable" : NULL);
    g_object_unref(fm_icon);
    return icon;
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

/* vim: set sw=4 et sts=4 : */
