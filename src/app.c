/*
 * Copyright (c) Konstantin Pugin.
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
#include <config.h>
#endif

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
#include "app-actions.h"

/* The same for new plugins type - they will be not unloaded by FmModule */
#define REGISTER_STATIC_MODULE(pc) do { \
    extern SimplePanelPluginInit lxpanel_static_plugin_##pc; \
    lxpanel_register_plugin_type(#pc, &lxpanel_static_plugin_##pc); } while (0)

G_DEFINE_TYPE_WITH_PRIVATE(PanelApp, panel_app, GTK_TYPE_APPLICATION);

gchar *cprofile = "default";
gchar *ccommand = NULL;
static gboolean is_started;

static const gchar* panel_commands = "run menu config exit ";

const GActionEntry app_action_entries[] =
{
    {"about", activate_about, NULL, NULL, NULL},
    {"run", activate_run, NULL, NULL, NULL}
};
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
static void init_plugin_class_list(void);

static void panel_app_startup(GApplication* app)
{
    G_APPLICATION_CLASS (panel_app_parent_class)->startup (app);
    setlocale(LC_CTYPE, "");
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );

    /* Add our own icons to the search path of icon theme */
    gtk_icon_theme_append_search_path( gtk_icon_theme_get_default(), PACKAGE_DATA_DIR "/images" );
    /* init LibFM */
    fm_gtk_init(NULL);

    /* prepare modules data */
    _prepare_modules();

    /* add actions for all panels */
    g_action_map_add_action_entries(G_ACTION_MAP(app),app_action_entries,G_N_ELEMENTS(app_action_entries),NULL);
    GSimpleAction* act = g_simple_action_new("quit",NULL);
    g_signal_connect(act,"activate",G_CALLBACK(activate_exit),app);
    g_action_map_add_action(G_ACTION_MAP(app),G_ACTION(act));
}

static void panel_app_shutdown(GApplication* app)
{
        /* destroy all panels */
    free_global_config();

    _unload_modules();
    fm_gtk_finalize();
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
        init_plugin_class_list();
        if( G_UNLIKELY( ! start_all_panels((PanelApp*)app) ) )
            g_warning( "Config files are not found.\n" );
        else
        {
            all_panels = gtk_application_get_windows(GTK_APPLICATION(app));
            for( l = all_panels; l; l = l->next )
            {
                apply_styling(l->data);
            }
            g_free(file);
            is_started=TRUE;
        }
    }
    if (ccommand!=NULL)
    {
        if (!g_strcmp0(ccommand,"menu"))
        {
#ifndef DISABLE_MENU
            GList* l;
            GList* all_panels = gtk_application_get_windows(GTK_APPLICATION(app));
            for( l = all_panels; l; l = l->next )
            {
                SimplePanel* p = (SimplePanel*)l->data;
                GList *plugins, *pl;

                plugins = gtk_container_get_children(GTK_CONTAINER(p->priv->box));
                for (pl = plugins; pl; pl = pl->next)
                {
                    const SimplePanelPluginInit *init = PLUGIN_CLASS(pl->data);
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
            SimplePanel * p = ((all_panels != NULL) ? all_panels->data : NULL);
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
        gchar* checker = g_strdup_printf("%s ",command);
        if (g_strrstr(panel_commands,checker))
        {
            ccommand = g_strdup(command);
        }
        else
        {
            g_application_command_line_printerr (commandline,
                                 _("%s: invalid command - %s. Doing nothing. Valid commands: %s\n"),
                                 g_get_application_name(),command,panel_commands);
            if (ccommand)
            {
                g_free(ccommand);
                ccommand = NULL;
            }
        }
        g_free(checker);
    }
    g_application_activate (application);
    return 0;
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
            SimplePanel* panel = panel_new(GTK_APPLICATION(app),panel_config, name );
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

/* Initialize the static plugins. */
static void init_plugin_class_list(void)
{
#ifdef STATIC_SEPARATOR
    REGISTER_STATIC_MODULE(separator);
#endif

#ifdef STATIC_DCLOCK
    REGISTER_STATIC_MODULE(dclock);
#endif

#ifdef STATIC_DIRMENU
    REGISTER_STATIC_MODULE(dirmenu);
#endif

#ifndef DISABLE_MENU
#ifdef STATIC_MENU
    REGISTER_STATIC_MODULE(menu);
#endif
#endif

#ifdef STATIC_SPACE
    REGISTER_STATIC_MODULE(space);
#endif
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
