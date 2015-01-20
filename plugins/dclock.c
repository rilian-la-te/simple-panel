/**
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

#include "plugin.h"

#include <libfm/fm-gtk.h>

#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>

#include "misc.h"
#include "css.h"

#define DEFAULT_TIP_FORMAT    "%A %x"
#define DEFAULT_CLOCK_FORMAT  "%R"

#define DCLOCK_KEY_CLOCK_FORMAT     "clock-format"
#define DCLOCK_KEY_TOOLTIP_FORMAT     "tooltip-format"
#define DCLOCK_KEY_CENTER_TEXT     "center-text"
#define DCLOCK_KEY_ICON_ONLY     "icon-only"
#define DCLOCK_KEY_BOLD_FONT     "bold-font"

/* Private context for digital clock plugin. */
typedef struct {
    GtkWidget * plugin;				/* Back pointer to plugin */
    SimplePanel * panel;
    GSettings* settings;
    GtkWidget* btn;
    GtkWidget * calendar_window;		/* Calendar window, if it is being displayed */
    char * clock_format;			/* Format string for clock value */
    char * tooltip_format;			/* Format string for tooltip value */
    gboolean bold;				/* True if bold font */
    guint timer;				/* Timer for periodic update */
    enum {
	AWAITING_FIRST_CHANGE,			/* Experimenting to determine interval, waiting for first change */
	AWAITING_SECOND_CHANGE,			/* Experimenting to determine interval, waiting for second change */
	ONE_SECOND_INTERVAL,			/* Determined that one second interval is necessary */
	ONE_MINUTE_INTERVAL			/* Determined that one minute interval is sufficient */
    } expiration_interval;
    int experiment_count;			/* Count of experiments that have been done to determine interval */
    char * prev_clock_value;			/* Previous value of clock */
    char * prev_tooltip_value;			/* Previous value of tooltip */
} DClockPlugin;

static gboolean dclock_update_display(DClockPlugin * dc);
static void dclock_destructor(gpointer user_data);
static gboolean dclock_apply_configuration(gpointer user_data);

inline gchar* dclock_gen_css(gboolean is_bold){
    return g_strdup_printf(".-simple-panel-font-weight{\n"
                    " font-weight: %s;\n"
                    "}",is_bold ? "bold" : "normal");
}

/* Display a window containing the standard calendar widget. */
static GtkWidget * dclock_create_calendar(DClockPlugin * dc)
{
    /* Create a new window. */
    GtkWidget * win = gtk_window_new(GTK_WINDOW_POPUP);
    gint x, y;
//    gtk_style_context_remove_class(gtk_widget_get_style_context(win),GTK_STYLE_CLASS_BACKGROUND);
    gtk_window_set_default_size(GTK_WINDOW(win), 180, 180);
    gtk_container_set_border_width(GTK_CONTAINER(win), 5);
    /* Create a vertical box as a child of the window. */
    GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(box));

    /* Create a standard calendar widget as a child of the vertical box. */
    GtkWidget * calendar = gtk_calendar_new();
    gtk_calendar_set_display_options(
        GTK_CALENDAR(calendar),
        GTK_CALENDAR_SHOW_WEEK_NUMBERS | GTK_CALENDAR_SHOW_DAY_NAMES | GTK_CALENDAR_SHOW_HEADING);
    gtk_box_pack_start(GTK_BOX(box), calendar, TRUE, TRUE, 0);


    /* Preset the widget position right now to not move it across the screen */
    gtk_widget_show_all(box);
    lxpanel_plugin_popup_set_position_helper(dc->panel, dc->plugin, win, &x, &y);
    gtk_window_move(GTK_WINDOW(win), x, y);

    /* Return the widget. */
    return win;
}

static gboolean dclock_toggled(GtkToggleButton* btn, DClockPlugin* dc)
{
    if (gtk_toggle_button_get_active(btn))
    {
        dc->calendar_window = dclock_create_calendar(dc);
        gtk_widget_show_all(dc->calendar_window);
    }
    else
    {
        gtk_widget_destroy(dc->calendar_window);
        dc->calendar_window = NULL;
    }
}

/* Set the timer. */
static void dclock_timer_set(DClockPlugin * dc, struct timeval *current_time)
{
    int milliseconds = 1000;

    /* Get current time to millisecond resolution. */
    if (gettimeofday(current_time, NULL) >= 0)
    {
        /* Compute number of milliseconds until next second boundary. */
        milliseconds = 1000 - (current_time->tv_usec / 1000);

        /* If the expiration interval is the minute boundary,
         * add number of milliseconds after that until next minute boundary. */
        if (dc->expiration_interval == ONE_MINUTE_INTERVAL)
        {
            time_t seconds = 60 - (current_time->tv_sec - (current_time->tv_sec / 60) * 60);
            milliseconds += seconds * 1000;
        }
    }

    /* Be defensive, and set the timer. */
    if (milliseconds <= 0)
        milliseconds = 1000;
    dc->timer = g_timeout_add(milliseconds, (GSourceFunc) dclock_update_display, (gpointer) dc);
}

/* Periodic timer callback.
 * Also used during initialization and configuration change to do a redraw. */
static gboolean dclock_update_display(DClockPlugin * dc)
{
    /* Determine the current time. */
    struct timeval now;
    struct tm * current_time;

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    dclock_timer_set(dc, &now);
    current_time = localtime(&now.tv_sec);

    /* Determine the content of the clock label and tooltip. */
    char clock_value[64];
    char tooltip_value[64];
    clock_value[0] = '\0';
    if (dc->clock_format != NULL)
        strftime(clock_value, sizeof(clock_value), dc->clock_format, current_time);
    tooltip_value[0] = '\0';
    if (dc->tooltip_format != NULL)
        strftime(tooltip_value, sizeof(tooltip_value), dc->tooltip_format, current_time);

    /* When we write the clock value, it causes the panel to do a full relayout.
     * Since this function may be called too often while the timing experiment is underway,
     * we take the trouble to check if the string actually changed first. */
    if (((dc->prev_clock_value == NULL) || (strcmp(dc->prev_clock_value, clock_value) != 0)))
    {
        /* Convert "\n" escapes in the user's format string to newline characters. */
        char * newlines_converted = NULL;
        if (strstr(clock_value, "\\n") != NULL)
        {
            newlines_converted = g_strdup(clock_value);	/* Just to get enough space for the converted result */
            char * p;
            char * q;
            for (p = clock_value, q = newlines_converted; *p != '\0'; p += 1)
            {
                if ((p[0] == '\\') && (p[1] == 'n'))
                {
                    *q++ = '\n';
                    p += 1;
                }
                else
                    *q++ = *p;
            }
            *q = '\0';
        }

        gchar * utf8 = g_locale_to_utf8(((newlines_converted != NULL) ? newlines_converted : clock_value), -1, NULL, NULL, NULL);
        if (utf8 != NULL)
        {
            gtk_button_set_label(GTK_BUTTON(dc->btn),utf8);
            g_free(utf8);
        }
        g_free(newlines_converted);
    }

    /* Determine the content of the tooltip. */
    gchar * utf8 = g_locale_to_utf8(tooltip_value, -1, NULL, NULL, NULL);
    if (utf8 != NULL)
    {
        gtk_widget_set_tooltip_text(dc->plugin, utf8);
        g_free(utf8);
    }

    /* Conduct an experiment to see how often the value changes.
     * Use this to decide whether we update the value every second or every minute.
     * We need to account for the possibility that the experiment is being run when we cross a minute boundary. */
    if (dc->expiration_interval < ONE_SECOND_INTERVAL)
    {
        if (dc->prev_clock_value == NULL)
        {
            /* Initiate the experiment. */
            dc->prev_clock_value = g_strdup(clock_value);
            dc->prev_tooltip_value = g_strdup(tooltip_value);
        }
        else
        {
            if (((strcmp(dc->prev_clock_value, clock_value) == 0))
            && (strcmp(dc->prev_tooltip_value, tooltip_value) == 0))
            {
                dc->experiment_count += 1;
                if (dc->experiment_count > 3)
                {
                    /* No change within 3 seconds.  Assume change no more often than once per minute. */
                    dc->expiration_interval = ONE_MINUTE_INTERVAL;
                    g_free(dc->prev_clock_value);
                    g_free(dc->prev_tooltip_value);
                    dc->prev_clock_value = NULL;
                    dc->prev_tooltip_value = NULL;
                }
            }
            else if (dc->expiration_interval == AWAITING_FIRST_CHANGE)
            {
                /* We have a change at the beginning of the experiment, but we do not know when the next change might occur.
                 * Continue the experiment for 3 more seconds. */
                dc->expiration_interval = AWAITING_SECOND_CHANGE;
                dc->experiment_count = 0;
                g_free(dc->prev_clock_value);
                g_free(dc->prev_tooltip_value);
                dc->prev_clock_value = g_strdup(clock_value);
                dc->prev_tooltip_value = g_strdup(tooltip_value);
            }
            else
            {
                /* We have a second change.  End the experiment. */
                dc->expiration_interval = ((dc->experiment_count > 3) ? ONE_MINUTE_INTERVAL : ONE_SECOND_INTERVAL);
                g_free(dc->prev_clock_value);
                g_free(dc->prev_tooltip_value);
                dc->prev_clock_value = NULL;
                dc->prev_tooltip_value = NULL;
            }
        }
    }

    /* Reset the timer and return. */
    return FALSE;
}

/* Plugin constructor. */
static GtkWidget *dclock_constructor(SimplePanel *panel, GSettings *settings)
{
    /* Allocate and initialize plugin context and set into Plugin private data pointer. */
    DClockPlugin * dc = g_new0(DClockPlugin, 1);
    GtkWidget * p, *w;

    dc->clock_format = g_settings_get_string(settings,DCLOCK_KEY_CLOCK_FORMAT);
    dc->tooltip_format = g_settings_get_string(settings,DCLOCK_KEY_TOOLTIP_FORMAT);
    dc->bold = g_settings_get_boolean(settings,DCLOCK_KEY_BOLD_FONT);

    /* Save construction pointers */
    dc->panel = panel;
    dc->settings = settings;

    /* Allocate top level widget and set into Plugin widget pointer. */
    dc->plugin = p = gtk_event_box_new();
    lxpanel_plugin_set_data(p, dc, dclock_destructor);


    dc->btn = w = gtk_toggle_button_new();
    g_signal_connect(w, "toggled", G_CALLBACK(dclock_toggled),dc);
    gtk_container_add(GTK_CONTAINER(p), w);
    gtk_widget_show(w);

    /* Create a label and an image as children of the horizontal box.
     * Only one of these is visible at a time, controlled by user preference. */
    simple_panel_setup_button(w,NULL,NULL);

    /* Initialize the clock display. */
    if (dc->clock_format == NULL)
        dc->clock_format = g_strdup(_(DEFAULT_CLOCK_FORMAT));
    if (dc->tooltip_format == NULL)
        dc->tooltip_format = g_strdup(_(DEFAULT_TIP_FORMAT));
    dclock_apply_configuration(p);

    /* Show the widget and return. */
    return p;
}

/* Plugin destructor. */
static void dclock_destructor(gpointer user_data)
{
    DClockPlugin * dc = user_data;

    /* Remove the timer. */
    if (dc->timer != 0)
        g_source_remove(dc->timer);

    /* Ensure that the calendar is dismissed. */
    if (dc->calendar_window != NULL)
        gtk_widget_destroy(dc->calendar_window);

    /* Deallocate all memory. */
    g_free(dc->clock_format);
    g_free(dc->tooltip_format);
    g_free(dc->prev_clock_value);
    g_free(dc->prev_tooltip_value);
    g_free(dc);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static gboolean dclock_apply_configuration(gpointer user_data)
{
    GtkWidget * p = user_data;
    DClockPlugin * dc = lxpanel_plugin_get_data(p);
    struct timeval now;

    /* stop the updater now */
    if (dc->timer)
        g_source_remove(dc->timer);

    /* Rerun the experiment to determine update interval and update the display. */
    g_free(dc->prev_clock_value);
    g_free(dc->prev_tooltip_value);
    dc->expiration_interval = AWAITING_FIRST_CHANGE;
    dc->experiment_count = 0;
    dc->prev_clock_value = NULL;
    dc->prev_tooltip_value = NULL;
    dclock_timer_set(dc, &now);

    /* Hide the calendar. */
    if (dc->calendar_window != NULL)
    {
        gtk_widget_destroy(dc->calendar_window);
        dc->calendar_window = NULL;
    }

    gchar* css = dclock_gen_css(dc->bold);
    css_apply_with_class(dc->btn,css,"-simple-panel-font-weight",FALSE);
    g_free(css);

    /* Save configuration */
    g_settings_set_string(dc->settings, DCLOCK_KEY_CLOCK_FORMAT, dc->clock_format);
    g_settings_set_string(dc->settings, DCLOCK_KEY_TOOLTIP_FORMAT, dc->tooltip_format);
    g_settings_set_boolean(dc->settings, DCLOCK_KEY_BOLD_FONT, dc->bold);
    return FALSE;
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *dclock_configure(SimplePanel *panel, GtkWidget *p)
{
    DClockPlugin * dc = lxpanel_plugin_get_data(p);
    return lxpanel_generic_config_dlg(_("Digital Clock"), panel,
        dclock_apply_configuration, p,
        _("Clock Format"), &dc->clock_format, CONF_TYPE_STR,
        _("Tooltip Format"), &dc->tooltip_format, CONF_TYPE_STR,
        _("Format codes: man 3 strftime; %n for line break"), NULL, CONF_TYPE_TRIM,
        _("Bold font"), &dc->bold, CONF_TYPE_BOOL,
        NULL);
}

/* Plugin descriptor. */
SimplePanelPluginInit lxpanel_static_plugin_dclock = {
    .name = N_("Digital Clock"),
    .description = N_("Display digital clock and tooltip"),

    .new_instance = dclock_constructor,
    .config = dclock_configure,
    .has_config = TRUE,
};
