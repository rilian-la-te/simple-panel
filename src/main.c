#include "app.h"

int main (int argc, char *argv[], char* env)
{
    PanelApp *app;
    gint status;

    app = g_object_new (LX_TYPE_PANEL_APP,
                        "application-id", "org.simple.panel",
                        "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                        NULL);
    status = g_application_run (G_APPLICATION (app), argc, argv);

    g_object_unref (app);

    return status;
}
