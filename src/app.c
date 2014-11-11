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

#include "app.h"
#include "panel.h"
#include "css.h"
#include "private.h"
#include "app-actions.h"
#include "app-misc.h"
#include "panel-enum-types.h"
#include "misc.h"

/* The same for new plugins type - they will be not unloaded by FmModule */
#define REGISTER_STATIC_MODULE(pc) do { \
    extern SimplePanelPluginInit lxpanel_static_plugin_##pc; \
    lxpanel_register_plugin_type(#pc, &lxpanel_static_plugin_##pc); } while (0)

enum
{
    PROP_NONE,
    PROP_WIDGET_STYLE,
    PROP_CSS,
    PROP_LOGOUT,
    PROP_SHUTDOWN,
    PROP_TERMINAL,
    PROP_PROFILE,
    PROP_COUNT
};

G_DEFINE_TYPE_WITH_PRIVATE(PanelApp, panel_app, GTK_TYPE_APPLICATION)

#define PANEL_APP_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SIMPLE_TYPE_PANEL_APP, PanelAppPrivate))

static gchar *ccommand = NULL;
static gboolean is_started;

static const gchar* panel_commands = "run menu config exit ";

const GActionEntry app_action_entries[] =
{
    {"about", activate_about, NULL, NULL, NULL},
    {"run", activate_run, NULL, NULL, NULL},
    {"logout", activate_logout, NULL, NULL, NULL},
    {"shutdown", activate_shutdown, NULL, NULL, NULL},
    {"quit", activate_exit, NULL, NULL, NULL},
};
static const GOptionEntry entries[] =
{
  { "version", 'v', 0, G_OPTION_ARG_NONE, NULL, N_("Print version and exit"), NULL },
  { "profile", 'p', 0, G_OPTION_ARG_STRING, NULL, N_("Use specified profile"), N_("profile") },
  { "command", 'c', 0, G_OPTION_ARG_STRING, NULL, N_("Run command on already opened panel"), N_("cmd") },
  { NULL }
};

static gboolean start_all_panels(PanelApp* app);
static void _ensure_user_config_dirs(PanelApp *app);
static void init_plugin_class_list(void);

static void panel_app_startup(GApplication* app)
{
    G_APPLICATION_CLASS (panel_app_parent_class)->startup (app);
    PANEL_APP(app)->priv = PANEL_APP_GET_PRIVATE(app);
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
    g_action_map_add_action_entries(G_ACTION_MAP(app),app_action_entries,G_N_ELEMENTS(app_action_entries),app);

    /*load config*/
    _ensure_user_config_dirs(PANEL_APP(app));
    PANEL_APP(app)->priv->config = load_global_config_gsettings(PANEL_APP(app),&(PANEL_APP(app)->priv->config_file));
    gdk_window_set_events(gdk_get_default_root_window(), GDK_STRUCTURE_MASK |
            GDK_SUBSTRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK);
    init_plugin_class_list();
    if( G_UNLIKELY( ! start_all_panels(PANEL_APP(app)) ) )
        g_warning( "Config files are not found.\n" );
    else
    {
        apply_styling(PANEL_APP(app));
    }
}

static void panel_app_shutdown(GApplication* app)
{
        /* destroy all panels */
    PANEL_APP(app)->priv = PANEL_APP_GET_PRIVATE(app);
    g_object_unref(PANEL_APP(app)->priv->config);
    g_object_unref(PANEL_APP(app)->priv->config_file);
    _unload_modules();
    fm_gtk_finalize();
    G_APPLICATION_CLASS (panel_app_parent_class)->shutdown (app);
}

void panel_app_activate(GApplication* app)
{
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
        g_object_set(G_OBJECT(application),"profile",&profile_name,NULL);
    }
    if (g_variant_dict_lookup (options, "command", "&s", &command))
    {
        if (g_strrstr(panel_commands,command))
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
    }
    g_application_activate(application);
    return 0;
}

static void panel_app_set_property(GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    PanelApp* app;
    g_return_if_fail (SIMPLE_IS_PANEL_APP (object));

    app = PANEL_APP (object);
    app->priv = PANEL_APP_GET_PRIVATE(app);

    switch (prop_id)
    {
    case PROP_WIDGET_STYLE:
        app->priv->widgets_style = g_value_get_enum(value);
        apply_styling(app);
        break;
    case PROP_LOGOUT:
        if (app->priv->logout_cmd)
            g_free(app->priv->logout_cmd);
        app->priv->logout_cmd = g_strdup(g_value_get_string(value));
        break;
    case PROP_SHUTDOWN:
        if (app->priv->shutdown_cmd)
            g_free(app->priv->shutdown_cmd);
        app->priv->shutdown_cmd = g_strdup(g_value_get_string(value));
        break;
    case PROP_PROFILE:
        if (app->priv->profile)
            g_free(app->priv->profile);
        app->priv->profile = g_strdup(g_value_get_string(value));
        break;
    case PROP_CSS:
        if (app->priv->custom_css)
            g_free(app->priv->custom_css);
        app->priv->custom_css = g_strdup(g_value_get_string(value));
        apply_styling(app);
        break;
    case PROP_TERMINAL:
        app->priv->terminal_cmd = g_strdup(g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void panel_app_get_property(GObject      *object,
                               guint         prop_id,
                               GValue *value,
                               GParamSpec   *pspec)
{
    PanelApp* app;
    g_return_if_fail (SIMPLE_IS_PANEL_APP (object));

    app = PANEL_APP (object);
    app->priv = PANEL_APP_GET_PRIVATE(app);

    switch (prop_id)
    {
    case PROP_WIDGET_STYLE:
        g_value_set_enum(value,app->priv->widgets_style);
        break;
    case PROP_LOGOUT:
        g_value_set_string(value,app->priv->logout_cmd);
        break;
    case PROP_SHUTDOWN:
        g_value_set_string(value,app->priv->shutdown_cmd);
        break;
    case PROP_PROFILE:
        g_value_set_string(value,app->priv->profile);
        break;
    case PROP_CSS:
        g_value_set_string(value,app->priv->custom_css);
        break;
    case PROP_TERMINAL:
        g_value_set_string(value,app->priv->terminal_cmd);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static gboolean start_all_panels(PanelApp* app)
{
    char *panel_dir;
    const gchar * const * dir;

    /* try user panels */
    panel_dir = _user_config_file_name(app,"panels", NULL);
    start_panels_from_dir(GTK_APPLICATION(app),panel_dir);
    g_free(panel_dir);
    if (gtk_application_get_windows(GTK_APPLICATION(app)) != NULL)
        return TRUE;
    /* else try XDG fallbacks */
    dir = g_get_system_config_dirs();
    if (dir) while (dir[0])
    {
        panel_dir = _system_config_file_name(app,dir[0], "panels");
        start_panels_from_dir(GTK_APPLICATION(app),panel_dir);
        g_free(panel_dir);
        if (gtk_application_get_windows(GTK_APPLICATION(app)) != NULL)
            return TRUE;
        dir++;
    }
    return gtk_application_get_windows(GTK_APPLICATION(app)) != NULL;
}

static void _ensure_user_config_dirs(PanelApp* app)
{
    app->priv = PANEL_APP_GET_PRIVATE(app);
    char *dir = g_build_filename(g_get_user_config_dir(), "simple-panel", app->priv->profile,
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

void panel_app_dispose(GObject* object)
{
    PanelApp* app = PANEL_APP(object);
    if (app->priv->custom_css)
        g_free(app->priv->custom_css);
    if (app->priv->logout_cmd)
        g_free(app->priv->logout_cmd);
    if (app->priv->terminal_cmd)
        g_free(app->priv->terminal_cmd);
    if (app->priv->shutdown_cmd)
        g_free(app->priv->shutdown_cmd);
    g_free(app->priv->profile);
}

void panel_app_class_init(PanelAppClass *klass)
{
    GObjectClass* gobject_class;
    GApplicationClass* app_class;
    gobject_class = G_OBJECT_CLASS(klass);
    app_class = G_APPLICATION_CLASS(klass);
    gobject_class->set_property = panel_app_set_property;
    gobject_class->get_property = panel_app_get_property;
    gobject_class->dispose = panel_app_dispose;
    app_class->startup = panel_app_startup;
    app_class->shutdown = panel_app_shutdown;
    app_class->handle_local_options = panel_app_handle_local_options;
    app_class->command_line = panel_app_command_line;
    app_class->activate = panel_app_activate;
    g_object_class_install_property (
                gobject_class,
                PROP_PROFILE,
                g_param_spec_string (
                    "profile",
                    "Profile",
                    "Config profile of this panel application",
                    "default",
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property (
                gobject_class,
                PROP_CSS,
                g_param_spec_string (
                    "css",
                    "CSS File",
                    "Custom CSS Style of this panel",
                    "",
                    G_PARAM_READWRITE));
    g_object_class_install_property (
                gobject_class,
                PROP_LOGOUT,
                g_param_spec_string (
                    "logout-command",
                    "Logout command",
                    "Command for logout used by this panel",
                    "lxsession-logout",
                    G_PARAM_READWRITE));
    g_object_class_install_property (
                gobject_class,
                PROP_SHUTDOWN,
                g_param_spec_string (
                    "shutdown-command",
                    "Shutdown command",
                    "Command for shutdown used by this panel",
                    "lxsession-logout",
                    G_PARAM_READWRITE));
    g_object_class_install_property (
                gobject_class,
                PROP_TERMINAL,
                g_param_spec_string (
                    "terminal-command",
                    "Terminal",
                    "Terminal emulator used by this panel",
                    "sakura",
                    G_PARAM_READWRITE));
    g_object_class_install_property(
                gobject_class,
                PROP_WIDGET_STYLE,
                g_param_spec_enum(
                    "widget-style",
                    "Widget style",
                    "Widget style used by application. May be normal, dark and custom",
                    PANEL_WIDGETS_STYLE,
                    PANEL_WIDGETS_NORMAL,
                    G_PARAM_READWRITE));
}
