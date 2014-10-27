#ifndef APP_H
#define APP_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _PanelApp PanelApp;
typedef struct _PanelAppClass PanelAppClass;
typedef struct _PanelAppPrivate PanelAppPrivate;

#define LX_TYPE_PANEL_APP                  (panel_app_get_type())
#define PANEL_APP(obj)                   (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                        LX_TYPE_PANEL_APP, PanelApp))
#define PANEL_APP_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST((klass), \
                                        LX_TYPE_PANEL_APP, PanelAppClass))
#define LX_IS_PANEL_APP(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                        LX_TYPE_PANEL_APP))

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
};

int panel_app_command_line(GApplication* application, GApplicationCommandLine* commandline);

G_END_DECLS

#endif // APP_H