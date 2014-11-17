#ifndef HAVE_NS_STATUSICON_H
#define HAVE_NS_STATUSICON_H

#include "netstat.h"
#include <gtk/gtk.h>

struct statusicon {
        GtkWidget *main;
        GtkWidget *icon;
};

struct statusicon *create_statusicon(SimplePanel *panel, GtkWidget *box,
        const char *tooltips, const char* icon_name);
void statusicon_destroy(struct statusicon *icon);
void update_statusicon(struct statusicon *widget, const char *filename);
void set_statusicon_tooltips(struct statusicon *widget, const char *tooltips);
void set_statusicon_visible(struct statusicon *widget, gboolean b);

#endif
