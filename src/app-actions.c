/**
 * Copyright (c) 2006-2014 LxDE Developers, see the file AUTHORS for details.
 * Copyright (c) 2014 Konstantin Pugin.
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
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <glib.h>
#include <libfm/fm-gtk.h>

#include "app-actions.h"
#include "private.h"
#include "app-misc.h"
#include "css.h"

static void get_about_dialog()
{
    GtkWidget *about;
    const gchar* authors[] = {
        "Hong Jen Yee (PCMan) <pcman.tw@gmail.com>",
        "Jim Huang <jserv.tw@gmail.com>",
        "Greg McNew <gmcnew@gmail.com> (battery plugin)",
        "Fred Chien <cfsghost@gmail.com>",
        "Daniel Kesler <kesler.daniel@gmail.com>",
        "Juergen Hoetzel <juergen@archlinux.org>",
        "Marty Jack <martyj19@comcast.net>",
        "Martin Bagge <brother@bsnet.se>",
        "Andriy Grytsenko <andrej@rep.kiev.ua>",
        "Giuseppe Penone <giuspen@gmail.com>",
        "Piotr Sipika <piotr.sipika@gmail.com>",
        NULL
    };
    /* TRANSLATORS: Replace this string with your names, one name per line. */
    gchar *translators = _( "translator-credits" );

    about = gtk_about_dialog_new();
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about), VERSION);
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about), _("SimplePanel"));
    if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), "start-here"))
    {
         gtk_about_dialog_set_logo( GTK_ABOUT_DIALOG(about),
                                    gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "start-here", 48, 0, NULL));
         gtk_window_set_icon(GTK_WINDOW(about),
                             gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "start-here", 48, 0, NULL));
    }
    else
    {
        gtk_about_dialog_set_logo(  GTK_ABOUT_DIALOG(about),
                                    gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/images/my-computer.png", NULL));
        gtk_window_set_icon(GTK_WINDOW(about),
                            gdk_pixbuf_new_from_file(PACKAGE_DATA_DIR "/images/my-computer.png", NULL));
    }
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about), _("Copyright (C) 2008-2014"));
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about), _( " Simple desktop panel."));
    gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(about), "This program is free software; you can redistribute it and/or\nmodify it under the terms of the GNU General Public License\nas published by the Free Software Foundation; either version 2\nof the License, or (at your option) any later version.\n\nThis program is distributed in the hope that it will be useful,\nbut WITHOUT ANY WARRANTY; without even the implied warranty of\nMERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\nGNU General Public License for more details.\n\nYou should have received a copy of the GNU General Public License\nalong with this program; if not, write to the Free Software\nFoundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.");
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about), "http://lxde.org/");
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about), authors);
    gtk_about_dialog_set_translator_credits(GTK_ABOUT_DIALOG(about), translators);
    gtk_dialog_run(GTK_DIALOG(about));
    gtk_widget_destroy(about);
}

void activate_about(GSimpleAction* simple, GVariant* param, gpointer data)
{
    get_about_dialog();
}

void activate_run(GSimpleAction* simple, GVariant* param, gpointer data)
{
    gtk_run();
}

void activate_exit(GSimpleAction* simple, GVariant* param, gpointer data)
{
    g_application_quit(G_APPLICATION((PanelApp*)data));
}

void activate_logout(GSimpleAction* simple, GVariant* param, gpointer data)
{
    PanelApp* app = (PanelApp*)data;
    const char* l_logout_cmd = app->priv->logout_cmd;
    /* If LXSession is running, _LXSESSION_PID will be set */
    if( ! l_logout_cmd && getenv("_LXSESSION_PID") )
        l_logout_cmd = "lxsession-logout";

    if( l_logout_cmd )
        fm_launch_command_simple(NULL, NULL, 0, l_logout_cmd, NULL);
    else
        fm_show_error(NULL, NULL, _("Logout command is not set"));
}

void activate_shutdown(GSimpleAction* simple, GVariant* param, gpointer data)
{
    PanelApp* app = (PanelApp*)data;
    const char* l_logout_cmd = app->priv->shutdown_cmd;
    /* If LXSession is running, _LXSESSION_PID will be set */
    if( ! l_logout_cmd && getenv("_LXSESSION_PID") )
        l_logout_cmd = "lxsession-logout";

    if( l_logout_cmd )
        fm_launch_command_simple(NULL, NULL, 0, l_logout_cmd, NULL);
    else
        fm_show_error(NULL, NULL, _("Shutdown command is not set"));
}

#define COMMAND_GROUP "Command"
#define APPEARANCE_GROUP "Appearance"

void load_global_config(PanelApp* app)
{
    GKeyFile* kf = g_key_file_new();
    char* file = NULL;
    gboolean loaded = FALSE;
    const gchar * const * dir = g_get_system_config_dirs();

    /* try to load system config file first */
    if (dir) while (dir[0] && !loaded)
    {
        g_free(file);
        file = _system_config_file_name(app,dir[0], "config");
        if (g_key_file_load_from_file(kf, file, 0, NULL))
            loaded = TRUE;
        dir++;
    }
    /* now try to load user config file */
    g_free(file);
    file = _user_config_file_name(app,"config", NULL);
    if (g_key_file_load_from_file(kf, file, 0, NULL))
        loaded = TRUE;
    g_free(file);

    if( loaded )
    {
        g_object_set(G_OBJECT(app),
                     "logout-command",g_key_file_get_string( kf, COMMAND_GROUP, "Logout", NULL ),
                     "shutdown-command",g_key_file_get_string( kf, COMMAND_GROUP, "Shutdown", NULL ),
                     "widget-style",g_key_file_get_integer(kf, APPEARANCE_GROUP, "Type", NULL),
                     "css", g_key_file_get_string(kf, APPEARANCE_GROUP, "CSS", NULL),
                     NULL);
        /* check for terminal setting on upgrade */
        if (fm_config->terminal == NULL)
        {
            fm_config->terminal = g_key_file_get_string(kf, COMMAND_GROUP,
                                                        "Terminal", NULL);
            if (fm_config->terminal != NULL) /* setting changed, save it */
                fm_config_save(fm_config, NULL);
        }
        save_global_config(app);
    }
    g_key_file_free( kf );
}

void save_global_config(PanelApp* app)
{
    char* file = _user_config_file_name(app,"config", NULL);
    FILE* f = fopen( file, "w" );
    char *logout_cmd,*shutdown_cmd, *css;
    int type;
    g_object_get(G_OBJECT(app),
                 "logout-command",&logout_cmd,
                 "shutdown-command",&shutdown_cmd,
                 "widget-style",&type,
                 "css", &css,
                 NULL);
    if( f )
    {
        fprintf( f, "[" COMMAND_GROUP "]\n");
        fprintf( f, "Logout=%s\n", logout_cmd);
        fprintf( f, "Shutdown=%s\n", shutdown_cmd);
        fprintf( f, "[" APPEARANCE_GROUP "]\n");
        fprintf( f, "Type=%d\n", type);
        fprintf( f, "CSS=%s\n", css);
        fclose( f );
        if (logout_cmd != NULL)
            g_free(logout_cmd);
        if (shutdown_cmd != NULL)
            g_free(shutdown_cmd);
        if (css != NULL)
            g_free(css);
    }
    g_free(file);
}

void apply_styling(PanelApp* app)
{
    gboolean is_dark = (app->priv->widgets_style%2 == 1);
    if (gtk_settings_get_default() != NULL)
        gtk_settings_set_long_property(gtk_settings_get_default(),"gtk-application-prefer-dark-theme",is_dark,NULL);
    if (app->priv->widgets_style >= PANEL_WIDGETS_CSS)
    {
        GList* l;
        gchar* msg;
        for( l = gtk_application_get_windows(GTK_APPLICATION(app)); l; l = l->next )
        {
            msg=css_apply_from_file(GTK_WIDGET(l->data),app->priv->custom_css);
            if (msg!=NULL)
            {
                g_warning("Cannot apply custom style: %s\n",msg);
                g_free(msg);
            }
        }
    }
}
