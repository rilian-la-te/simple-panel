/*
 * ACPI battery monitor plugin for SimplePanel
 *
 * Copyright (C) 2007 by Greg McNew <gmcnew@gmail.com>
 * Copyright (C) 2008 by Hong Jen Yee <pcman.tw@gmail.com>
 * Copyright (C) 2009 by Juergen Hoetzel <juergen@archlinux.org>
 * Copyright (C) 2014 by Andriy Grytsenko <andrej@rep.kiev.ua>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *
 * This plugin monitors battery usage on ACPI-enabled systems by reading the
 * battery information found in /sys/class/power_supply. The update interval is
 * user-configurable and defaults to 3 second.
 *
 * The battery's remaining life is estimated from its current charge and current
 * rate of discharge. The user may configure an alarm command to be run when
 * their estimated remaining battery life reaches a certain level.
 */

/* FIXME:
 *  Here are somethings need to be improvec:
 *  1. Replace pthread stuff with gthread counterparts for portability.
 *  3. Add an option to hide the plugin when AC power is used or there is no battery.
 *  4. Handle failure gracefully under systems other than Linux.
*/

#include <glib.h>
#include <glib/gi18n.h>
#include <pthread.h> /* used by pthread_create() and alarmThread */
#include <semaphore.h> /* used by update() and alarmProcess() for alarms */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <gdk/gdk.h>
#include <cairo.h>
#include <gtk/gtk.h>

#include "batt_sys.h"
#include "plugin.h" /* all other APIs including panel configuration */

#define BATT_KEY_HIDE "hide-if-no-battery"
#define BATT_KEY_NAME "battery-name"
#define BATT_KEY_PM_COMMAND "pm-command"
#define BATT_KEY_ALARM_COMMAND "alarm-command"
#define BATT_KEY_ALARM_TIME "alarm-time"
#define BATT_KEY_EXTENDED "show-extended-info"
#define BATT_KEY_USE_SYMBOLIC "use-symbolic-icons"
#define BATT_KEY_SHOW_PERCENTAGE "show-percentage"
#define BATT_KEY_SHOW_REMAINING "show-remaining"
#define BATT_KEY_BATTERY_NAME "battery-name"
/* The last MAX_SAMPLES samples are averaged when charge rates are evaluated.
   This helps prevent spikes in the "time left" values the user sees. */
#define MAX_SAMPLES 10

typedef struct {
    char *alarmCommand,
        *pm_command,
        *batt_name;
    GtkWidget *box;
    GtkWidget *button;
    GtkWidget *popup;
    GtkWidget* img;
    GIcon* icon;
    unsigned int alarmTime,
        numSamples,
        *rateSamples,
        rateSamplesSum,
        timer,
        state_elapsed_time,
        info_elapsed_time,
        wasCharging;
    gboolean symbolic,
        hide_if_no_battery,
        show_percentage,
        show_remaining;
    sem_t alarmProcessLock;
    battery* b;
    gboolean has_ac_adapter;
    gboolean show_extended_information;
    SimplePanel *panel;
    GSettings *settings;
} lx_battery;


typedef struct {
    char *command;
    sem_t *lock;
} Alarm;

static void destructor(gpointer data);
static void update_display(lx_battery *lx_b);
static void panel_edge_changed(SimplePanel* panel, GParamSpec* param, lx_battery* b);

/* alarmProcess takes the address of a dynamically allocated alarm struct (which
   it must free). It ensures that alarm commands do not run concurrently. */
static void * alarmProcess(void *arg) {
    Alarm *a = (Alarm *) arg;

    sem_wait(a->lock);
    if (system(a->command) != 0)
        g_warning("plugin batt: failed to execute alarm command \"%s\"", a->command);
    sem_post(a->lock);

    g_free(a);
    return NULL;
}


static void append(gchar **tooltip, gchar *fmt, ...)
{
    gchar *old = *tooltip;
    gchar *new;
    va_list va;

    va_start(va, fmt);
    new = g_strdup_vprintf(fmt, va);
    va_end(va);

    *tooltip = g_strconcat(old, new, NULL);

    g_free(old);
    g_free(new);
}


/* Make a tooltip string, and display remaining charge time if the battery
   is charging or remaining life if it's discharging */
static gchar* make_tooltip(lx_battery* lx_b, gboolean isCharging)
{
    gchar * tooltip;
    gchar * indent = "  ";
    battery *b = lx_b->b;

    if (b == NULL)
        return NULL;

    if (isCharging) {
        int hours = lx_b->b->seconds / 3600;
        int left_seconds = lx_b->b->seconds - 3600 * hours;
        int minutes = left_seconds / 60;
        tooltip = g_strdup_printf(
                _("Battery: %d%% charged, %d:%02d until full"),
                lx_b->b->percentage,
                hours,
                minutes );
    } else {
        /* if we have enough rate information for battery */
        if (lx_b->b->percentage != 100) {
            int hours = lx_b->b->seconds / 3600;
            int left_seconds = lx_b->b->seconds - 3600 * hours;
            int minutes = left_seconds / 60;
            tooltip = g_strdup_printf(
                    _("Battery: %d%% charged, %d:%02d left"),
                    lx_b->b->percentage,
                    hours,
                    minutes );
        } else {
            tooltip = g_strdup_printf(
                    _("Battery: %d%% charged"),
                    100 );
        }
    }

    if (!lx_b->show_extended_information) {
        return tooltip;
    }

    if (b->energy_full_design != -1)
        append(&tooltip, _("\n%sEnergy full design:\t\t%5d mWh"), indent, b->energy_full_design);
    if (b->energy_full != -1)
        append(&tooltip, _("\n%sEnergy full:\t\t\t%5d mWh"), indent, b->energy_full);
    if (b->energy_now != -1)
        append(&tooltip, _("\n%sEnergy now:\t\t\t%5d mWh"), indent, b->energy_now);
    if (b->power_now != -1)
        append(&tooltip, _("\n%sPower now:\t\t\t%5d mW"), indent, b->power_now);

    if (b->charge_full_design != -1)
        append(&tooltip, _("\n%sCharge full design:\t%5d mAh"), indent, b->charge_full_design);
    if (b->charge_full != -1)
        append(&tooltip, _("\n%sCharge full:\t\t\t%5d mAh"), indent, b->charge_full);
    if (b->charge_now != -1)
        append(&tooltip, _("\n%sCharge now:\t\t\t%5d mAh"), indent, b->charge_now);
    if (b->current_now != -1)
        append(&tooltip, _("\n%sCurrent now:\t\t\t%5d mA"), indent, b->current_now);

    if (b->voltage_now != -1)
        append(&tooltip, _("\n%sCurrent Voltage:\t\t%.3lf V"), indent, b->voltage_now / 1000.0);

    return tooltip;
}

static void set_tooltip_text(lx_battery* lx_b)
{
    if (lx_b->b == NULL)
        return;
    gboolean isCharging = battery_is_charging(lx_b->b);
    gchar *tooltip = make_tooltip(lx_b, isCharging);
    gtk_widget_set_tooltip_text(lx_b->button, tooltip);
    g_free(tooltip);
}

void update_display(lx_battery *lx_b) {
    battery *b = lx_b->b;
    /* unit: mW */
    int rate;
    gboolean isCharging;
    gchar *char_str = NULL;

    /* no battery is found */
    if( b == NULL )
    {
        gtk_widget_set_tooltip_text( lx_b->button, _("No batteries found") );
        if (lx_b->hide_if_no_battery)
        {
            gtk_widget_hide(gtk_widget_get_parent(lx_b->button));
        }
        lx_b->icon = g_themed_icon_new_with_default_fallbacks(lx_b->symbolic ? "battery-missing-symbolic" : "battery-missing-panel");
        goto update_done;
        char_str = g_strdup_printf("");
    }


    /* fixme: only one battery supported */

    rate = lx_b->b->current_now;
    isCharging = battery_is_charging ( b );

    /* Consider running the alarm command */
    if ( !isCharging && rate > 0 &&
        ( ( battery_get_remaining( b ) / 60 ) < (int)lx_b->alarmTime ) )
    {
        /* Shrug this should be done using glibs process functions */
        /* Alarms should not run concurrently; determine whether an alarm is
           already running */
        int alarmCanRun;
        sem_getvalue(&(lx_b->alarmProcessLock), &alarmCanRun);

        /* Run the alarm command if it isn't already running */
        if (alarmCanRun) {

            Alarm *a = (Alarm *) malloc(sizeof(Alarm));
            a->command = lx_b->alarmCommand;
            a->lock = &(lx_b->alarmProcessLock);

            /* Manage the alarm process in a new thread, which which will be
               responsible for freeing the alarm struct it's given */
            pthread_t alarmThread;
            pthread_create(&alarmThread, NULL, alarmProcess, a);
        }
    }

    set_tooltip_text(lx_b);
    if (isCharging)
    {
        if (lx_b->b->percentage == 100)
        {
            lx_b->icon = g_themed_icon_new_with_default_fallbacks(lx_b->symbolic ? "battery-full-charged-symbolic" : "battery-full-charged-panel");
        }
        else if (lx_b->b->percentage >= 80)
        {
            lx_b->icon = g_themed_icon_new_with_default_fallbacks(lx_b->symbolic ? "battery-full-charging-symbolic" : "battery-full-charging-panel");
        }
        else if (lx_b->b->percentage >= 50)
        {
            lx_b->icon = g_themed_icon_new_with_default_fallbacks(lx_b->symbolic ? "battery-good-charging-symbolic" : "battery-good-charging-panel");
        }
        else if (lx_b->b->percentage >= 30)
        {
            lx_b->icon = g_themed_icon_new_with_default_fallbacks(lx_b->symbolic ? "battery-low-charging-symbolic" : "battery-low-charging-panel");
        }
        else if (lx_b->b->percentage >= 10)
        {
            lx_b->icon = g_themed_icon_new_with_default_fallbacks(lx_b->symbolic ? "battery-caution-charging-symbolic" : "battery-caution-charging-panel");
        }
    }
    else
    {
        if (lx_b->b->percentage >= 80)
        {
            lx_b->icon = g_themed_icon_new_with_default_fallbacks(lx_b->symbolic ? "battery-full-symbolic" : "battery-full-panel");
        }
        else if (lx_b->b->percentage >= 50)
        {
            lx_b->icon = g_themed_icon_new_with_default_fallbacks(lx_b->symbolic ? "battery-good-symbolic" : "battery-good-panel");
        }
        else if (lx_b->b->percentage >= 30)
        {
            lx_b->icon = g_themed_icon_new_with_default_fallbacks(lx_b->symbolic ? "battery-low-symbolic" : "battery-low-panel");
        }
        else if (lx_b->b->percentage >= 10)
        {
            lx_b->icon = g_themed_icon_new_with_default_fallbacks(lx_b->symbolic ? "battery-caution-symbolic" : "battery-caution-panel");
        }
    }
    int hours = lx_b->b->seconds / 3600;
    int left_seconds = lx_b->b->seconds - 3600 * hours;
    int minutes = left_seconds / 60;
    if (lx_b->show_percentage && lx_b->show_remaining && lx_b->b->percentage != 100)
        char_str = g_strdup_printf(_("%d%% (%d:%02d remaining)"),lx_b->b->percentage,hours,minutes);
    else if (lx_b->show_percentage)
        char_str = g_strdup_printf("%d%%",lx_b->b->percentage);
    else if (lx_b->show_remaining && lx_b->b->percentage != 100)
        char_str = g_strdup_printf("%d:%02d",hours,minutes);
    else char_str = g_strdup_printf("");

update_done:
    gtk_button_set_label(GTK_BUTTON(lx_b->button),char_str);
    simple_panel_image_change_gicon(lx_b->img,lx_b->icon);
    g_free(char_str);
}

/* This callback is called every 3 seconds */
static int update_timeout(lx_battery *lx_b) {
    battery *bat;
    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;
    lx_b->state_elapsed_time++;
    lx_b->info_elapsed_time++;

    bat = battery_update( lx_b->b );
    if (bat == NULL)
    {
        battery_free(lx_b->b);

        /* maybe in the mean time a battery has been inserted. */
        lx_b->b = battery_get_by_name(lx_b->batt_name);
    }

    update_display( lx_b );

    return TRUE;
}

static GtkWidget * constructor(SimplePanel *panel, GSettings *settings)
{
    lx_battery *lx_b;
    GtkWidget *p;
    GVariant* target;

    lx_b = g_new0(lx_battery, 1);

    /* get available battery */
    lx_b->batt_name = g_settings_get_string(settings,BATT_KEY_BATTERY_NAME);

    lx_b->b = battery_get_by_name (lx_b->batt_name);

    p =lx_b->box= gtk_event_box_new();
    lxpanel_plugin_set_data(p, lx_b, destructor);
    gtk_widget_set_has_window(p, FALSE);

    lx_b->button = gtk_button_new();


    gtk_container_add(GTK_CONTAINER(lx_b->box), lx_b->button);


    gtk_widget_show(lx_b->button);

    sem_init(&(lx_b->alarmProcessLock), 0, 1);

    lx_b->alarmCommand = NULL;

    /* Set default values for integers */
    lx_b->alarmTime = 5;

    /* remember instance data */
    lx_b->panel = panel;
    lx_b->settings = settings;

    lx_b->show_extended_information = FALSE;

    lx_b->hide_if_no_battery = g_settings_get_boolean(settings,BATT_KEY_HIDE);
    lx_b->show_extended_information = g_settings_get_boolean(settings,BATT_KEY_EXTENDED);
    lx_b->symbolic = g_settings_get_boolean(settings,BATT_KEY_USE_SYMBOLIC);
    lx_b->alarmCommand = g_settings_get_string(settings,BATT_KEY_ALARM_COMMAND);
    lx_b->pm_command = g_settings_get_string(settings,BATT_KEY_PM_COMMAND);
    lx_b->alarmTime = g_settings_get_uint(settings,BATT_KEY_ALARM_TIME);
    lx_b->show_percentage = g_settings_get_boolean(settings,BATT_KEY_SHOW_PERCENTAGE);
    lx_b->show_remaining = g_settings_get_boolean(settings,BATT_KEY_SHOW_REMAINING);

    lx_b->icon = g_themed_icon_new_with_default_fallbacks(lx_b->symbolic
                                                          ?"battery-missing-symbolic"
                                                          :"battery-missing-panel");
    lx_b->img = simple_panel_image_new_for_gicon(lx_b->panel,lx_b->icon,-1);
    simple_panel_setup_button(lx_b->button,lx_b->img,NULL);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(lx_b->button),"app.launch-command");
    target = g_variant_new_string(lx_b->pm_command);
    gtk_actionable_set_action_target_value(GTK_ACTIONABLE(lx_b->button),target);
    g_signal_connect(lx_b->panel,"notify::edge",G_CALLBACK(panel_edge_changed),lx_b);
    update_display(lx_b);

    /* Start the update loop */
    lx_b->timer = g_timeout_add_seconds( 9, (GSourceFunc) update_timeout, (gpointer) lx_b);
    gtk_widget_show(p);

    return p;
}

static void panel_edge_changed(SimplePanel* panel, GParamSpec* param, lx_battery* b)
{
    int edge;
    g_object_get(panel,PANEL_PROP_EDGE,&edge,NULL);
    edge = (edge == GTK_POS_TOP || edge == GTK_POS_BOTTOM) ? GTK_POS_LEFT : GTK_POS_BOTTOM;
    gtk_button_set_image_position(GTK_BUTTON(b->button),edge);
}


static void
destructor(gpointer data)
{
    lx_battery *b = (lx_battery *)data;

    if (b->b != NULL)
        battery_free(b->b);
    if (b->img)
        gtk_widget_destroy(b->img);
    if (b->button)
        gtk_widget_destroy(b->button);
    if (b->popup)
        gtk_widget_destroy(b->popup);
    if (b->icon)
        g_object_unref(b->icon);
    g_free(b->alarmCommand);
    g_free(b->rateSamples);
    g_free(b->batt_name);
    g_free(b->pm_command);
    sem_destroy(&(b->alarmProcessLock));
    if (b->timer)
        g_source_remove(b->timer);
    g_free(b);

    return;

}

static gboolean applyConfig(gpointer user_data)
{
    lx_battery *b = lxpanel_plugin_get_data(user_data);
    GVariant* target;

    battery_free(b->b);
    b->b = battery_get_by_name(b->batt_name);
    /* ensure visibility if requested */
    if (!b->hide_if_no_battery)
        gtk_widget_show(user_data);
    else if (b->b == NULL)
        gtk_widget_hide(user_data);

    /* update tooltip */
    set_tooltip_text(b);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(b->button),"app.launch-command");
    target = g_variant_new_string(b->pm_command);
    gtk_actionable_set_action_target_value(GTK_ACTIONABLE(b->button),target);
    /* update settings */
    g_settings_set_string(b->settings, BATT_KEY_NAME, b->batt_name);
    g_settings_set_boolean(b->settings, BATT_KEY_HIDE, b->hide_if_no_battery);
    g_settings_set_string(b->settings, BATT_KEY_ALARM_COMMAND, b->alarmCommand);
    g_settings_set_uint(b->settings, BATT_KEY_ALARM_TIME, b->alarmTime);
    g_settings_set_string(b->settings, BATT_KEY_PM_COMMAND, b->pm_command);
    g_settings_set_boolean(b->settings, BATT_KEY_USE_SYMBOLIC, b->symbolic);
    g_settings_set_boolean(b->settings, BATT_KEY_SHOW_PERCENTAGE, b->show_percentage);
    g_settings_set_boolean(b->settings, BATT_KEY_SHOW_REMAINING, b->show_remaining);
    g_settings_set_boolean(b->settings, BATT_KEY_EXTENDED,
                         b->show_extended_information);
    update_display(b);
    return FALSE;
}


static GtkWidget *config(SimplePanel *panel, GtkWidget *p) {
    lx_battery *b = lxpanel_plugin_get_data(p);
    return lxpanel_generic_config_dlg(_("Battery Monitor"),
            panel, applyConfig, p,
            _("Battery name"), &b->batt_name, CONF_TYPE_STR,
            _("Hide if there is no battery"), &b->hide_if_no_battery, CONF_TYPE_BOOL,
            _("Alarm command"), &b->alarmCommand, CONF_TYPE_STR,
            _("Alarm time (minutes left)"), &b->alarmTime, CONF_TYPE_INT,
            _("Use symbolic icons"), &b->symbolic, CONF_TYPE_BOOL,
            _("Settings command"), &b->pm_command, CONF_TYPE_STR,
            _("Show remaining time on panel"), &b->show_remaining, CONF_TYPE_BOOL,
            _("Show percentage on panel"), &b->show_percentage, CONF_TYPE_BOOL,
            _("Show Extended Information"), &b->show_extended_information, CONF_TYPE_BOOL,
            NULL);
}


FM_DEFINE_MODULE(lxpanel_gtk, batt);

/* Plugin descriptor. */
SimplePanelPluginInit fm_module_init_lxpanel_gtk = {
    .name        = N_("Battery Monitor"),
    .description = N_("Display battery status using ACPI"),
    .new_instance = constructor,
    .config      = config,
    .has_config  = TRUE,
};


/* vim: set sw=4 sts=4 : */
