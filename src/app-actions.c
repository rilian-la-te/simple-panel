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

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#include "app-actions.h"
#include "private.h"
#include "app-misc.h"
#include "css.h"
#include "misc.h"

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

GSettings* load_global_config_gsettings(PanelApp* app, GSettingsBackend** config)
{
    char* file = NULL;
    gchar* user_file = NULL;
    gboolean loaded = FALSE;
    GSettings *settings;
    GSettingsBackend* b;
    const gchar * const * dir = g_get_system_config_dirs();

    /* try to load system config file first */
    if (dir) while (dir[0] && !loaded)
    {
        g_free(file);
        file = _system_config_file_name(app,dir[0], "config");
        if (g_file_test(file, G_FILE_TEST_EXISTS))
            loaded = TRUE;
        dir++;
    }
    /* now try to load user config file */
    user_file = _user_config_file_name(app,"config", NULL);
    if (!g_file_test(file, G_FILE_TEST_EXISTS) && loaded)
    {
        GFile* src = g_file_new_for_path(file);
        GFile* dest = g_file_new_for_path(file);
        g_file_copy(src,dest,G_FILE_COPY_BACKUP,NULL,NULL,NULL,NULL);
        g_object_unref(src);
        g_object_unref(dest);
    }
    g_free(file);

    b = g_keyfile_settings_backend_new(user_file,"/org/simple/panel/","global");
    settings = g_settings_new_with_backend_and_path("org.simple.panel",b,"/org/simple/panel/");
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(app),settings,"logout-command");
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(app),settings,"shutdown-command");
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(app),settings,"terminal-command");
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(app),settings,"css");
    simple_panel_add_gsettings_as_action(G_ACTION_MAP(app),settings,"widget-style");
    g_free(user_file);
    *config=b;
    return settings;
}

void apply_styling(PanelApp* app)
{
    gboolean is_dark = (app->priv->widgets_style%2 == 1);
    if (gtk_settings_get_default() != NULL)
        gtk_settings_set_long_property(gtk_settings_get_default(),"gtk-application-prefer-dark-theme",is_dark,NULL);
    if (app->priv->widgets_style >= PANEL_WIDGETS_CSS)
    {
        gchar* msg;
        msg=css_apply_from_file_to_app(app->priv->custom_css);
        if (msg!=NULL)
        {
            g_warning("Cannot apply custom style: %s\n",msg);
            g_free(msg);
        }
    }
}
