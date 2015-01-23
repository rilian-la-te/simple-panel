#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>


#include "tasklist.h"
#include "plugin.h"


typedef struct {
    SimplePanel *panel; /* The panel and settings are required to apply config */
    GSettings* settings;
    GtkWidget* tasklist;
} IconTasklistPlugin;

static GtkWidget *icontasklist_constructor(SimplePanel *panel, GSettings *settings)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    IconTasklistPlugin * sp = g_new0(IconTasklistPlugin, 1);
    GtkWidget * p;

    /* Save construction pointers */
    sp->panel = panel;
    sp->settings = settings;

    /* Allocate top level widget and set into Plugin widget pointer. */
    p = gtk_event_box_new();
    lxpanel_plugin_set_data(p, sp, g_free);
    gtk_widget_set_has_window(p,FALSE);
    sp->tasklist = icon_tasklist_new();
    gtk_container_add(GTK_CONTAINER(p),sp->tasklist);

    /* Apply the configuration and show the widget. */
    gtk_widget_show_all(p);
    return p;
}

SimplePanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Application Task Bar with Pinned Tasks"),
    .description = N_("Bar with buttons to launch application and show all opened windows"),

    .expand_available = TRUE,
    .expand_default = TRUE,

//    .init = icontasklist_init,
    .new_instance = icontasklist_constructor,
//    .config = launchtaskbar_configure,
    .has_config = FALSE
};
