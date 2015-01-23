#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <libfm/fm.h>

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
    GtkWidget * p, *w;

    w = icon_tasklist_new();

    g_return_val_if_fail(w != NULL, NULL);
    /* Save construction pointers */
    sp->panel = panel;
    sp->settings = settings;

    gtk_widget_show(w);

    /* Allocate top level widget and set into Plugin widget pointer. */
    p = gtk_event_box_new();
    lxpanel_plugin_set_data(p, sp, g_free);
    gtk_widget_set_has_window(p,FALSE);
    sp->tasklist = w;
    gtk_container_add(GTK_CONTAINER(p),w);

    /* Apply the configuration and show the widget. */
    return p;
}

FM_DEFINE_MODULE(lxpanel_gtk, icontasklist)

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
