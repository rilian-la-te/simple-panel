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

#include "dbg.h"
#include "css.h"

#define DEFAULT_TIP_FORMAT    "%A %x"
#define DEFAULT_CLOCK_FORMAT  "%R"

#define DCLOCK_KEY_CLOCK_FORMAT     "clock-format"
#define DCLOCK_KEY_TOOLTIP_FORMAT     "tooltip-format"
#define DCLOCK_KEY_CENTER_TEXT     "center-text"
#define DCLOCK_KEY_ACTION     "click-action"
#define DCLOCK_KEY_ICON_ONLY     "icon-only"
#define DCLOCK_KEY_BOLD_FONT     "bold-font"

/* Private context for digital clock plugin. */
typedef struct {
    GtkWidget * plugin;				/* Back pointer to plugin */
    SimplePanel * panel;
#ifdef GSETTINGS_PLUGIN_TEST
    GSettings* settings;
#else
    config_setting_t *settings;
#endif
    GtkWidget * clock_label;			/* Label containing clock value */
    GtkWidget * clock_icon;			/* Icon when icon_only */
    GtkWidget * calendar_window;		/* Calendar window, if it is being displayed */
    char * clock_format;			/* Format string for clock value */
    char * tooltip_format;			/* Format string for tooltip value */
    char * action;				/* Command to execute on a click */
    gboolean bold;				/* True if bold font */
    gboolean icon_only;				/* True if icon only (no clock value) */
    gboolean center_text;
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

/* Handler for "map" signal on popup window. */
static void dclock_popup_map(GtkWidget * widget, DClockPlugin * dc)
{
    lxpanel_plugin_adjust_popup_position(widget, dc->plugin);
}

static void dclock_style_updated(GtkWidget* widget, DClockPlugin* pl)
{
    gchar* css = css_generate_flat_button(widget,pl->panel);
    css_apply_with_class(GTK_WIDGET(pl->clock_label),css,"-panel-flat-button",FALSE);
    css_apply_with_class(GTK_WIDGET(pl->clock_icon),css,"-panel-flat-button",FALSE);
    g_free (css);
}

/* Display a window containing the standard calendar widget. */
static GtkWidget * dclock_create_calendar(DClockPlugin * dc)
{
    /* Create a new window. */
    GtkWidget * win = gtk_window_new(GTK_WINDOW_POPUP);
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

    /* Connect signals. */
    g_signal_connect(G_OBJECT(win), "map", G_CALLBACK(dclock_popup_map), dc);

    /* Return the widget. */
    return win;
}

/* Handler for "button-press-event" event from main widget. */
static gboolean dclock_button_press_event(GtkWidget * widget, GdkEventButton * evt, SimplePanel * panel)
{
    DClockPlugin * dc = lxpanel_plugin_get_data(widget);

    /* If an action is set, execute it. */
    if (dc->action != NULL || (strlen(dc->action)>0))
        fm_launch_command_simple(NULL, NULL, 0, dc->action, NULL);

    /* If no action is set, toggle the presentation of the calendar. */
    else
    {
        if (dc->calendar_window == NULL)
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
    return TRUE;
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
    if (( ! dc->icon_only)
    && ((dc->prev_clock_value == NULL) || (strcmp(dc->prev_clock_value, clock_value) != 0)))
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
            lxpanel_draw_label_text(dc->panel, dc->clock_label, utf8, dc->bold, 1, TRUE);
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
            if (((dc->icon_only) || (strcmp(dc->prev_clock_value, clock_value) == 0))
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
#ifdef GSETTINGS_PLUGIN_TEST
static GtkWidget *dclock_constructor(SimplePanel *panel, GSettings *settings)
#else
static GtkWidget *dclock_constructor(SimplePanel *panel, config_setting_t *settings)
#endif
{
    /* Allocate and initialize plugin context and set into Plugin private data pointer. */
    DClockPlugin * dc = g_new0(DClockPlugin, 1);
    GtkWidget * p;
    const char *str;
    int tmp_int;

#ifdef GSETTINGS_PLUGIN_TEST
    dc->clock_format = g_settings_get_string(settings,DCLOCK_KEY_CLOCK_FORMAT);
    dc->tooltip_format = g_settings_get_string(settings,DCLOCK_KEY_TOOLTIP_FORMAT);
    dc->action = g_settings_get_string(settings,DCLOCK_KEY_ACTION);
    dc->center_text = g_settings_get_boolean(settings,DCLOCK_KEY_CENTER_TEXT);
    dc->icon_only = g_settings_get_boolean(settings,DCLOCK_KEY_ICON_ONLY);
    dc->bold = g_settings_get_boolean(settings,DCLOCK_KEY_BOLD_FONT);
#else
    /* Load parameters from the configuration file. */
    if (config_setting_lookup_string(settings, "ClockFmt", &str))
        dc->clock_format = g_strdup(str);
    if (config_setting_lookup_string(settings, "TooltipFmt", &str))
        dc->tooltip_format = g_strdup(str);
    if (config_setting_lookup_string(settings, "Action", &str))
        dc->action = g_strdup(str);
    if (config_setting_lookup_int(settings, "BoldFont", &tmp_int))
        dc->bold = tmp_int != 0;
    if (config_setting_lookup_int(settings, "IconOnly", &tmp_int))
        dc->icon_only = tmp_int != 0;
    if (config_setting_lookup_int(settings, "CenterText", &tmp_int))
        dc->center_text = tmp_int != 0;
#endif

    /* Save construction pointers */
    dc->panel = panel;
    dc->settings = settings;

    /* Allocate top level widget and set into Plugin widget pointer. */
    dc->plugin = p = gtk_event_box_new();
    lxpanel_plugin_set_data(p, dc, dclock_destructor);

    /* Allocate a horizontal box as the child of the top level. */
    GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(p), hbox);
    gtk_widget_show(hbox);

    /* Create a label and an image as children of the horizontal box.
     * Only one of these is visible at a time, controlled by user preference. */
    dc->clock_label = gtk_label_new(NULL);
    gtk_container_add(GTK_CONTAINER(hbox), dc->clock_label);
    dc->clock_icon = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(hbox), dc->clock_icon);

    /* Initialize the clock display. */
    if (dc->clock_format == NULL)
        dc->clock_format = g_strdup(_(DEFAULT_CLOCK_FORMAT));
    if (dc->tooltip_format == NULL)
        dc->tooltip_format = g_strdup(_(DEFAULT_TIP_FORMAT));
    dclock_apply_configuration(p);
    g_signal_connect(dc->panel, "style-updated", G_CALLBACK(dclock_style_updated),(gpointer) dc);

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
    g_free(dc->action);
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

    /* Set up the icon or the label as the displayable widget. */
    if (dc->icon_only)
    {
        if(lxpanel_image_set_icon_theme(dc->panel, dc->clock_icon, "clock") != FALSE) {
            lxpanel_image_set_from_file(dc->panel, dc->clock_icon, PACKAGE_DATA_DIR "/images/clock.png");
        }
        gtk_widget_show(dc->clock_icon);
        gtk_widget_hide(dc->clock_label);
    }
    else
    {
        gtk_widget_show(dc->clock_label);
        gtk_widget_hide(dc->clock_icon);
    }

    if (dc->center_text)
    {
        gtk_label_set_justify(GTK_LABEL(dc->clock_label), GTK_JUSTIFY_CENTER);
    }
    else
    {
        gtk_label_set_justify(GTK_LABEL(dc->clock_label), GTK_JUSTIFY_LEFT);
    }

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

    /* Save configuration */
#ifdef GSETTINGS_PLUGIN_TEST
    g_settings_set_string(dc->settings, DCLOCK_KEY_CLOCK_FORMAT, dc->clock_format);
    g_settings_set_string(dc->settings, DCLOCK_KEY_TOOLTIP_FORMAT, dc->tooltip_format);
    g_settings_set_string(dc->settings, DCLOCK_KEY_ACTION, dc->action);
    g_settings_set_int(dc->settings, DCLOCK_KEY_BOLD_FONT, dc->bold);
    g_settings_set_int(dc->settings, DCLOCK_KEY_ICON_ONLY, dc->icon_only);
    g_settings_set_int(dc->settings, DCLOCK_KEY_CENTER_TEXT, dc->center_text);
#else
    config_group_set_string(dc->settings, "ClockFmt", dc->clock_format);
    config_group_set_string(dc->settings, "TooltipFmt", dc->tooltip_format);
    config_group_set_string(dc->settings, "Action", dc->action);
    config_group_set_int(dc->settings, "BoldFont", dc->bold);
    config_group_set_int(dc->settings, "IconOnly", dc->icon_only);
    config_group_set_int(dc->settings, "CenterText", dc->center_text);
#endif
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
        _("Action when clicked (default: display calendar)"), &dc->action, CONF_TYPE_STR,
        _("Bold font"), &dc->bold, CONF_TYPE_BOOL,
        _("Tooltip only"), &dc->icon_only, CONF_TYPE_BOOL,
        _("Center text"), &dc->center_text, CONF_TYPE_BOOL,
        NULL);
}

/* Callback when panel configuration changes. */
static void dclock_reconfigure(SimplePanel *panel, GtkWidget *p)
{
    dclock_apply_configuration(p);
}

/* Plugin descriptor. */
SimplePanelPluginInit lxpanel_static_plugin_dclock = {
    .name = N_("Digital Clock"),
    .description = N_("Display digital clock and tooltip"),

    .new_instance = dclock_constructor,
    .config = dclock_configure,
    .has_config = TRUE,
    .reconfigure = dclock_reconfigure,
    .button_press_event = dclock_button_press_event
};
