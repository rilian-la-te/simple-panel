/*
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


#include <glib/gi18n.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <libfm/fm-gtk.h>

#define __LXPANEL_INTERNALS__

#include "app.h"
#include "panel.h"
#include "css.h"
#include "private.h"
#define VERSION "0.7.1"
G_DEFINE_TYPE_WITH_PRIVATE(PanelApp, panel_app, GTK_TYPE_APPLICATION);

gchar *cprofile = "default";
gchar *ccommand = NULL;
static gchar cversion[] = VERSION;
static gboolean is_started;

static const gchar* panel_commands = "run menu config restart exit";

static const GOptionEntry entries[] =
{
  { "version", 'v', 0, G_OPTION_ARG_NONE, NULL, N_("Print version and exit"), NULL },
  { "profile", 'p', 0, G_OPTION_ARG_STRING, NULL, N_("Use specified profile"), N_("profile") },
  { "command", 'c', 0, G_OPTION_ARG_STRING, NULL, N_("Run command on already opened panel"), N_("cmd") },
  { NULL }
};

static gboolean start_all_panels(PanelApp* app);
static void _ensure_user_config_dirs(void);
void load_global_config();
void free_global_config();

static void panel_app_startup(GApplication* app)
{
    GtkCssProvider* provider;
    GError* error;
    gboolean status;
    G_APPLICATION_CLASS (panel_app_parent_class)->startup (app);
    setlocale(LC_CTYPE, "");
#ifdef ENABLE_NLS
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE )
#endif

    /* Add our own icons to the search path of icon theme */
    gtk_icon_theme_append_search_path( gtk_icon_theme_get_default(), PACKAGE_DATA_DIR "/images" );
    /* init LibFM */
    fm_gtk_init(NULL);

    /* prepare modules data */
    _prepare_modules();
//TODO: CSS File.
    /* Add a gtkrc file to be parsed too. */

}

static void panel_app_shutdown(GApplication* app)
{
        /* destroy all panels */
    free_global_config();

    _unload_modules();
    fm_gtk_finalize();
//    if (!is_restarting)
//        return 0;
//    if (strchr(argv[0], G_DIR_SEPARATOR))
//        execve(argv[0], argv, env);
//    else
//        execve(g_find_program_in_path(argv[0]), argv, env);
//    return 1;
    G_APPLICATION_CLASS (panel_app_parent_class)->shutdown (app);
}

void panel_app_activate(GApplication* app)
{
    gchar *file, *msg;
    GList* l;
    GList* all_panels;
    if (!is_started)
    {
        _ensure_user_config_dirs();
        load_global_config();
        gdk_window_set_events(gdk_get_default_root_window(), GDK_STRUCTURE_MASK |
                GDK_SUBSTRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK);
        if( G_UNLIKELY( ! start_all_panels((PanelApp*)app) ) )
            g_warning( "Config files are not found.\n" );
        else
        {
            all_panels = gtk_application_get_windows(GTK_APPLICATION(app));
            file = _user_config_file_name("panel.css", NULL);
            for( l = all_panels; l; l = l->next )
            {
                msg=css_apply_from_file(GTK_WIDGET(l->data),file);
                if (msg!=NULL)
                {
                    g_warning("Cannot apply custom style: %s\n",msg);
                    g_free(msg);
                }
            }
            g_free(file);
            is_started=TRUE;
        }
    }
    else if (ccommand!=NULL)
    {
        if (!g_strcmp0(ccommand,"menu"))
        {
#ifndef DISABLE_MENU
            GList* l;
            GList* all_panels = gtk_application_get_windows(GTK_APPLICATION(app));
            for( l = all_panels; l; l = l->next )
            {
                LXPanel* p = (LXPanel*)l->data;
                GList *plugins, *pl;

                plugins = gtk_container_get_children(GTK_CONTAINER(p->priv->box));
                for (pl = plugins; pl; pl = pl->next)
                {
                    const LXPanelPluginInit *init = PLUGIN_CLASS(pl->data);
                    if (init->show_system_menu)
                    /* queue to show system menu */
                        init->show_system_menu(pl->data);
                }
                g_list_free(plugins);
            }
#endif
        }
        else if (!g_strcmp0(ccommand,"run"))
            gtk_run();
        else if (!g_strcmp0(ccommand,"config"))
        {
            GList* all_panels = gtk_application_get_windows(GTK_APPLICATION(app));
            LXPanel * p = ((all_panels != NULL) ? all_panels->data : NULL);
            if (p != NULL)
                panel_configure(p, 0);
        }
//        else if (!g_strcmp0(ccommand,"restart"))
//            restart();
        else if (!g_strcmp0(ccommand,"exit"))
            g_application_quit(G_APPLICATION(app));
    }
}

static gint panel_app_handle_local_options(GApplication *application, GVariantDict *options)
{
    if (g_variant_dict_contains (options, "version"))
    {
        g_print (_("%s - Version %s\n"), g_get_application_name (), VERSION);
        return 0;
    }
    return -1;
}

//static void
//usage()
//{
//    g_print(_("lxpanel %s - lightweight GTK2+ panel for UNIX desktops\n"), VERSION);
//    g_print(_("Command line options:\n"));
//    g_print(_(" --help      -- print this help and exit\n"));
//    g_print(_(" --version   -- print version and exit\n"));
////    g_print(_(" --log <number> -- set log level 0-5. 0 - none 5 - chatty\n"));
////    g_print(_(" --configure -- launch configuration utility\n"));
//    g_print(_(" --profile name -- use specified profile\n"));
//    g_print("\n");
//    g_print(_(" -h  -- same as --help\n"));
//    g_print(_(" -p  -- same as --profile\n"));
//    g_print(_(" -v  -- same as --version\n"));
// //   g_print(_(" -C  -- same as --configure\n"));
//    g_print(_("\nVisit http://lxde.org/ for detail.\n\n"));
//}
int panel_app_command_line(GApplication* application,GApplicationCommandLine* commandline)
{
    static gchar* profile_name = NULL;
    static gchar* command = NULL;
    GVariantDict *options;
    options = g_application_command_line_get_options_dict(commandline);
    if (g_variant_dict_lookup (options, "profile", "&s", &profile_name))
    {
        cprofile = g_strdup(profile_name);
    }
    if (g_variant_dict_lookup (options, "command", "&s", &command))
    {
        if (g_str_match_string(command,panel_commands,FALSE))
        {
            ccommand = g_strdup(command);
        }
        else
        {
            g_application_command_line_printerr (commandline,
                                 _("%s: invalid command. Doing nothing."),
                                 command);
        }
    }
    g_application_activate (application);
}

static void _start_panels_from_dir(PanelApp* app,const char *panel_dir)
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
            LXPanel* panel = panel_new(app,panel_config, name );
            if( panel )
                gtk_application_add_window(GTK_APPLICATION(app),GTK_WINDOW(panel));
        }
        g_free( panel_config );
    }
    g_dir_close( dir );
}

static gboolean start_all_panels(PanelApp* app)
{
    char *panel_dir;
    const gchar * const * dir;

    /* try user panels */
    panel_dir = _user_config_file_name("panels", NULL);
    _start_panels_from_dir(app,panel_dir);
    g_free(panel_dir);
    if (gtk_application_get_windows(GTK_APPLICATION(app)) != NULL)
        return TRUE;
    /* else try XDG fallbacks */
    dir = g_get_system_config_dirs();
    if (dir) while (dir[0])
    {
        panel_dir = _system_config_file_name(dir[0], "panels");
        _start_panels_from_dir(app,panel_dir);
        g_free(panel_dir);
        if (gtk_application_get_windows(GTK_APPLICATION(app)) != NULL)
            return TRUE;
        dir++;
    }
    /* last try at old fallback for compatibility reasons */
    panel_dir = _old_system_config_file_name("panels");
    _start_panels_from_dir(app,panel_dir);
    g_free(panel_dir);
    return gtk_application_get_windows(GTK_APPLICATION(app)) != NULL;
}

static void _ensure_user_config_dirs(void)
{
    char *dir = g_build_filename(g_get_user_config_dir(), "lxpanel", cprofile,
                                 "panels", NULL);

    /* make sure the private profile and panels dir exists */
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);
}

void panel_app_init(PanelApp *self)
{
    g_application_add_main_option_entries (G_APPLICATION (self), entries);
}


void panel_app_class_init(PanelAppClass *klass)
{
    GApplicationClass* app_class;
    app_class = G_APPLICATION_CLASS(klass);
    app_class->startup = panel_app_startup;
    app_class->shutdown = panel_app_shutdown;
    app_class->handle_local_options = panel_app_handle_local_options;
    app_class->command_line = panel_app_command_line;
    app_class->activate = panel_app_activate;

}
