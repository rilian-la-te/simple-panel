#ifndef APP_H
#define APP_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _PanelApp PanelApp;
typedef struct _PanelAppClass PanelAppClass;
typedef struct _PanelAppPrivate PanelAppPrivate;

#define SIMPLE_TYPE_PANEL_APP                  (panel_app_get_type())
#define PANEL_APP(obj)                   (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                        SIMPLE_TYPE_PANEL_APP, PanelApp))
#define PANEL_APP_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST((klass), \
                                        SIMPLE_TYPE_PANEL_APP, PanelAppClass))
#define SIMPLE_IS_PANEL_APP(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                        SIMPLE_TYPE_PANEL_APP))

extern GType panel_app_get_type          (void) G_GNUC_CONST;

struct _PanelApp
{
    GtkApplication app;
    PanelAppPrivate *priv;
};

struct _PanelAppClass
{
    GtkApplicationClass parent_class;
};

struct _PanelAppPrivate
{
    GtkApplicationClass parent_class;
    gboolean is_dark;
    gboolean use_css;
    GSettings* config;
    GSettingsBackend* config_file;
    gchar* logout_cmd;
    gchar* shutdown_cmd;
    gchar* custom_css;
    gchar* terminal_cmd;
    gchar* profile;
    GtkWidget* pref_dialog;
};

G_END_DECLS

#endif // APP_H
