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

#include "app.h"
#include "panel.h"
#include "css.h"
#include "private.h"
#include "app-actions.h"
#include "app-misc.h"
#include "panel-enum-types.h"
#include "misc.h"
#include "menu-maker.h"

enum
{
    PROP_NONE,
    PROP_DARK,
    PROP_USE_CSS,
    PROP_CSS,
    PROP_LOGOUT,
    PROP_SHUTDOWN,
    PROP_PROFILE,
    PROP_COUNT
};

G_DEFINE_TYPE_WITH_PRIVATE(PanelApp, panel_app, GTK_TYPE_APPLICATION)

#define PANEL_APP_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SIMPLE_TYPE_PANEL_APP, PanelAppPrivate))

static gboolean is_started;

static void activate_preferences (GSimpleAction* simple, GVariant* param, gpointer data);
static const GActionEntry app_action_entries[] =
{
    {"preferences", activate_preferences, NULL, NULL, NULL},
    {"panel-preferences", activate_panel_preferences, "s", NULL, NULL},
    {"about", activate_about, NULL, NULL, NULL},
    {"menu", activate_menu, NULL, NULL, NULL},
    {"run", activate_run, NULL, NULL, NULL},
    {"logout", activate_logout, NULL, NULL, NULL},
    {"shutdown", activate_shutdown, NULL, NULL, NULL},
    {"quit", activate_exit, NULL, NULL, NULL},
};


const GActionEntry menu_action_entries[] =
{
    {"launch-id", activate_menu_launch_id, "s", NULL, NULL},
    {"launch-uri", activate_menu_launch_uri, "s", NULL, NULL},
    {"launch-command", activate_menu_launch_command, "s", NULL, NULL},
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

static void custom_css_file_helper(GtkFileChooser * file_chooser, GtkApplication * p)
{
    char * file = g_strdup(gtk_file_chooser_get_filename(file_chooser));
    if (file != NULL)
    {
        g_action_group_activate_action(G_ACTION_GROUP(p),"css",g_variant_new_string(file));
        g_free(file);
    }
}

static void activate_preferences (GSimpleAction* simple, GVariant* param, gpointer data)
{
    PanelApp* app = (PanelApp*)data;
    PANEL_APP(app)->priv = PANEL_APP_GET_PRIVATE(app);
    GtkWidget* w;
    if (app->priv->pref_dialog!= NULL)
    {
        gtk_window_present(GTK_WINDOW(app->priv->pref_dialog));
        return;
    }
    GtkBuilder* builder = gtk_builder_new();
    gtk_builder_add_from_resource(builder,"/org/simple/panel/app/pref.ui",NULL);
    app->priv->pref_dialog = (GtkWidget*)gtk_builder_get_object( builder, "app-pref" );
    gtk_application_add_window(GTK_APPLICATION(app),GTK_WINDOW(app->priv->pref_dialog));
    g_object_add_weak_pointer( G_OBJECT(app->priv->pref_dialog), (gpointer) &app->priv->pref_dialog );
    gtk_window_set_position( GTK_WINDOW(app->priv->pref_dialog), GTK_WIN_POS_CENTER );
    panel_apply_icon(GTK_WINDOW(app->priv->pref_dialog));

    w = (GtkWidget*)gtk_builder_get_object( builder, "logout" );
    g_settings_bind(app->priv->config,"logout-command",w,"text",G_SETTINGS_BIND_DEFAULT);
    w = (GtkWidget*)gtk_builder_get_object( builder, "shutdown" );
    g_settings_bind(app->priv->config,"shutdown-command",w,"text",G_SETTINGS_BIND_DEFAULT);
    w = (GtkWidget*)gtk_builder_get_object( builder, "css-chooser" );
    g_settings_bind(app->priv->config,"is-custom",w,"sensitive",G_SETTINGS_BIND_GET);
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(w), app->priv->custom_css != NULL ? app->priv->custom_css: NULL);
    g_signal_connect( w, "file-set", G_CALLBACK (custom_css_file_helper), GTK_APPLICATION(app));
    gtk_window_present(GTK_WINDOW(app->priv->pref_dialog));
}


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
    g_action_map_add_action_entries(G_ACTION_MAP(app),menu_action_entries,G_N_ELEMENTS(menu_action_entries),app);
}

static void panel_app_shutdown(GApplication* gapp)
{
        /* destroy all panels */
    PanelApp* app = PANEL_APP(gapp);
    app->priv = PANEL_APP_GET_PRIVATE(app);
    g_object_unref(app->priv->config);
    g_object_unref(app->priv->config_file);
    _unload_modules();
    fm_gtk_finalize();
    if (app->priv->custom_css)
        g_free(app->priv->custom_css);
    if (app->priv->logout_cmd)
        g_free(app->priv->logout_cmd);
    if (app->priv->shutdown_cmd)
        g_free(app->priv->shutdown_cmd);
    g_free(app->priv->profile);
    G_APPLICATION_CLASS (panel_app_parent_class)->shutdown (gapp);
}

void panel_app_activate(GApplication* app)
{
    static gboolean is_started;

    if (!is_started)
    {
        /*load config*/
        _ensure_user_config_dirs(PANEL_APP(app));
        PANEL_APP(app)->priv->config = load_global_config_gsettings(PANEL_APP(app),&(PANEL_APP(app)->priv->config_file));
        gdk_window_set_events(gdk_get_default_root_window(), GDK_STRUCTURE_MASK |
                GDK_SUBSTRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK);
        init_plugin_class_list();
        if( G_UNLIKELY( ! start_all_panels(PANEL_APP(app)) ) )
        {
            g_warning( "Config files are not found.\n" );
            g_application_quit(app);
        }
        else
        {
            is_started = TRUE;
            apply_styling(PANEL_APP(app));
        }
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
    const gchar* profile_name = NULL;
    GVariantDict *options;
    const gchar* ccommand = NULL;
    options = g_application_command_line_get_options_dict(commandline);
    if (g_variant_dict_lookup (options, "profile", "&s", &profile_name))
        g_object_set(G_OBJECT(application),"profile",profile_name,NULL);
    if (g_variant_dict_lookup (options, "command", "&s", &ccommand))
    {

        gchar* name;
        GVariant* param;
        GError* err = NULL;
        g_action_parse_detailed_name(ccommand,&name,&param,&err);
        if(err)
        {
            g_warning("%s\n",err->message);
            g_clear_error(&err);
        }
        else if (g_action_map_lookup_action(G_ACTION_MAP(application),name))
            g_action_group_activate_action(G_ACTION_GROUP(application),name,param);
        else
        {
            gchar ** listv = g_action_group_list_actions(G_ACTION_GROUP(application));
            gchar* list = g_strjoinv(" ",listv);
            g_strfreev(listv);
            g_application_command_line_printerr (commandline,
                                                 _("%s: invalid command - %s. Doing nothing.\nValid commands: %s\n"),
                                                 g_get_application_name(),ccommand,list);
            g_free(list);
        }
        if (name)
            g_free(name);
        if (param)
            g_variant_unref(param);
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
    case PROP_DARK:
        app->priv->is_dark = g_value_get_boolean(value);
        apply_styling(app);
        break;
    case PROP_USE_CSS:
        app->priv->use_css = g_value_get_boolean(value);
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
    case PROP_DARK:
        g_value_set_boolean(value,app->priv->is_dark);
        break;
    case PROP_USE_CSS:
        g_value_set_boolean(value,app->priv->use_css);
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
                    NULL,
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
    g_object_class_install_property(
                gobject_class,
                PROP_DARK,
                g_param_spec_boolean(
                    "is-dark",
                    "Dark theme",
                    "TRUE if panel uses dark theme",
                    FALSE,
                    G_PARAM_READWRITE));
    g_object_class_install_property(
                gobject_class,
                PROP_USE_CSS,
                g_param_spec_boolean(
                    "is-custom",
                    "Custom CSS",
                    "TRUE if custom CSS file is used",
                    FALSE,
                    G_PARAM_READWRITE));
}
