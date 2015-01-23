/**
 * Copyright (c) 2008-2014 LxDE Developers, see the file AUTHORS for details.
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

#include <gtk/gtk.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <alsa/asoundlib.h>
#include <poll.h>
#include "plugin.h"

#define VOLUMEALSA_PROP_CHANNEL "channel-id"
#define VOLUMEALSA_PROP_CARD_ID "card-id"
#define VOLUMEALSA_PROP_USE_SYMBOLIC "use-symbolic-icons"
#define VOLUMEALSA_PROP_MIXER "mixer"

typedef struct {

    /* Graphics. */
    GtkWidget * plugin;				/* Back pointer to the widget */
    SimplePanel * panel;				/* Back pointer to panel */
    GSettings * settings;		/* Plugin settings */
    GtkWidget * tray_icon;			/* Displayed image */
    GtkWidget * popup_window;			/* Top level window for popup */
    GtkWidget * volume_scale;			/* Scale for volume */
    gboolean mute;			/* mute state */
    gchar* channel,*card_id, *mixer_command;
    gboolean symbolic;
    gboolean alsa_is_init;
    guint volume_scale_handler;			/* Handler for vscale widget */
    guint mute_check_handler;			/* Handler for mute_check widget */

    /* ALSA interface. */
    snd_mixer_t * mixer;			/* The mixer */
    snd_mixer_selem_id_t * sid;			/* The element ID */
    snd_mixer_elem_t * master_element;		/* The Master element */
    guint mixer_evt_idle;			/* Timer to handle restarting poll */
    guint restart_idle;

    /* unloading and error handling */
    GIOChannel **channels;                      /* Channels that we listen to */
    guint *watches;                             /* Watcher IDs for channels */
    guint num_channels;                         /* Number of channels */
    GIcon* icon;

} VolumeALSAPlugin;

static gboolean asound_restart(gpointer vol_gpointer);
static gboolean asound_initialize(VolumeALSAPlugin * vol);
static void asound_deinitialize(VolumeALSAPlugin * vol);
static void volumealsa_update_display(VolumeALSAPlugin * vol);
static void volumealsa_destructor(gpointer user_data);
static void alsa_init(VolumeALSAPlugin* vol);
static void volumealsa_toggle_muted(VolumeALSAPlugin * vol);

/*** ALSA ***/

static gboolean asound_find_element(VolumeALSAPlugin * vol, const char * ename)
{
    for (
      vol->master_element = snd_mixer_first_elem(vol->mixer);
      vol->master_element != NULL;
      vol->master_element = snd_mixer_elem_next(vol->master_element))
    {
        snd_mixer_selem_get_id(vol->master_element, vol->sid);
        if ((snd_mixer_selem_is_active(vol->master_element))
        && (strcmp(ename, snd_mixer_selem_id_get_name(vol->sid)) == 0))
            return TRUE;
    }
    return FALSE;
}

/* NOTE by PCMan:
 * This is magic! Since ALSA uses its own machanism to handle this part.
 * After polling of mixer fds, it requires that we should call
 * snd_mixer_handle_events to clear all pending mixer events.
 * However, when using the glib IO channels approach, we don't have
 * poll() and snd_mixer_poll_descriptors_revents(). Due to the design of
 * glib, on_mixer_event() will be called for every fd whose status was
 * changed. So, after each poll(), it's called for several times,
 * not just once. Therefore, we cannot call snd_mixer_handle_events()
 * directly in the event handler. Otherwise, it will get called for
 * several times, which might clear unprocessed pending events in the queue.
 * So, here we call it once in the event callback for the first fd.
 * Then, we don't call it for the following fds. After all fds with changed
 * status are handled, we remove this restriction in an idle handler.
 * The next time the event callback is involked for the first fs, we can
 * call snd_mixer_handle_events() again. Racing shouldn't happen here
 * because the idle handler has the same priority as the io channel callback.
 * So, io callbacks for future pending events should be in the next gmain
 * iteration, and won't be affected.
 */

static gboolean asound_reset_mixer_evt_idle(VolumeALSAPlugin * vol)
{
    if (!g_source_is_destroyed(g_main_current_source()))
        vol->mixer_evt_idle = 0;
    return FALSE;
}

/* Handler for I/O event on ALSA channel. */
static gboolean asound_mixer_event(GIOChannel * channel, GIOCondition cond, gpointer vol_gpointer)
{
    VolumeALSAPlugin * vol = (VolumeALSAPlugin *) vol_gpointer;
    int res = 0;

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    if (vol->mixer_evt_idle == 0)
    {
        vol->mixer_evt_idle = g_idle_add_full(G_PRIORITY_DEFAULT, (GSourceFunc) asound_reset_mixer_evt_idle, vol, NULL);
        res = snd_mixer_handle_events(vol->mixer);
    }

    if (cond & G_IO_IN)
    {
        /* the status of mixer is changed. update of display is needed. */
        volumealsa_update_display(vol);
    }

    if ((cond & G_IO_HUP) || (res < 0))
    {
        /* This means there're some problems with alsa. */
        g_warning("volumealsa: ALSA (or pulseaudio) had a problem: "
                "volumealsa: snd_mixer_handle_events() = %d,"
                " cond 0x%x (IN: 0x%x, HUP: 0x%x).", res, cond,
                G_IO_IN, G_IO_HUP);
        gtk_widget_set_tooltip_text(vol->plugin, "ALSA (or pulseaudio) had a problem."
                " Please check the lxpanel logs.");

        if (vol->restart_idle == 0)
            vol->restart_idle = g_timeout_add_seconds(1, asound_restart, vol);

        return FALSE;
    }

    return TRUE;
}

static gboolean asound_restart(gpointer vol_gpointer)
{
    VolumeALSAPlugin * vol = vol_gpointer;

    if (g_source_is_destroyed(g_main_current_source()))
        return FALSE;

    asound_deinitialize(vol);

    if (!asound_initialize(vol)) {
        g_warning("volumealsa: Re-initialization failed.");
        return TRUE; // try again in a second
    }

    g_warning("volumealsa: Restarted ALSA interface...");

    vol->restart_idle = 0;
    return FALSE;
}

/* Initialize the ALSA interface. */
static gboolean asound_initialize(VolumeALSAPlugin * vol)
{
    /* Access the "default" device. */
    snd_mixer_selem_id_alloca(&vol->sid);
    snd_mixer_open(&vol->mixer, 0);
    snd_mixer_attach(vol->mixer, vol->card_id);
    snd_mixer_selem_register(vol->mixer, NULL, NULL);
    snd_mixer_load(vol->mixer);

    /* Find Master element, or Front element, or PCM element, or LineOut element.
     * If one of these succeeds, master_element is valid. */
    if (! asound_find_element(vol,vol->channel))
        return FALSE;

    /* Set the playback volume range as we wish it. */
    snd_mixer_selem_set_playback_volume_range(vol->master_element, 0, 100);

    /* Listen to events from ALSA. */
    int n_fds = snd_mixer_poll_descriptors_count(vol->mixer);
    struct pollfd * fds = g_new0(struct pollfd, n_fds);

    vol->channels = g_new0(GIOChannel *, n_fds);
    vol->watches = g_new0(guint, n_fds);
    vol->num_channels = n_fds;

    snd_mixer_poll_descriptors(vol->mixer, fds, n_fds);
    int i;
    for (i = 0; i < n_fds; ++i)
    {
        GIOChannel* channel = g_io_channel_unix_new(fds[i].fd);
        vol->watches[i] = g_io_add_watch(channel, G_IO_IN | G_IO_HUP, asound_mixer_event, vol);
        vol->channels[i] = channel;
    }
    g_free(fds);
    return TRUE;
}

static void asound_deinitialize(VolumeALSAPlugin * vol)
{
    guint i;

    if (vol->mixer_evt_idle != 0) {
        g_source_remove(vol->mixer_evt_idle);
        vol->mixer_evt_idle = 0;
    }

    for (i = 0; i < vol->num_channels; i++) {
        g_source_remove(vol->watches[i]);
        g_io_channel_shutdown(vol->channels[i], FALSE, NULL);
        g_io_channel_unref(vol->channels[i]);
    }
    g_free(vol->channels);
    g_free(vol->watches);
    vol->channels = NULL;
    vol->watches = NULL;
    vol->num_channels = 0;

    snd_mixer_close(vol->mixer);
    vol->master_element = NULL;
    /* FIXME: unalloc vol->sid */
}

/* Get the presence of the mute control from the sound system. */
static gboolean asound_has_mute(VolumeALSAPlugin * vol)
{
    return ((vol->master_element != NULL) ? snd_mixer_selem_has_playback_switch(vol->master_element) : FALSE);
}

/* Get the condition of the mute control from the sound system. */
static gboolean asound_is_muted(VolumeALSAPlugin * vol)
{
    /* The switch is on if sound is not muted, and off if the sound is muted.
     * Initialize so that the sound appears unmuted if the control does not exist. */
    int value = 1;
    if (vol->master_element != NULL)
        snd_mixer_selem_get_playback_switch(vol->master_element, 0, &value);
    return (value == 0);
}

/* Get the volume from the sound system.
 * This implementation returns the average of the Front Left and Front Right channels. */
static int asound_get_volume(VolumeALSAPlugin * vol)
{
    long aleft = 0;
    long aright = 0;
    if (vol->master_element != NULL)
    {
        snd_mixer_selem_get_playback_volume(vol->master_element, SND_MIXER_SCHN_FRONT_LEFT, &aleft);
        snd_mixer_selem_get_playback_volume(vol->master_element, SND_MIXER_SCHN_FRONT_RIGHT, &aright);
    }
    return (aleft + aright) >> 1;
}

/* Set the volume to the sound system.
 * This implementation sets the Front Left and Front Right channels to the specified value. */
static void asound_set_volume(VolumeALSAPlugin * vol, int volume)
{
    if (vol->master_element != NULL)
    {
        snd_mixer_selem_set_playback_volume(vol->master_element, SND_MIXER_SCHN_FRONT_LEFT, volume);
        snd_mixer_selem_set_playback_volume(vol->master_element, SND_MIXER_SCHN_FRONT_RIGHT, volume);
    }
}

/*** Graphics ***/

static void volumealsa_lookup_current_icon(VolumeALSAPlugin * vol, int level)
{
    if (vol->mute || level == 0)
    {
        vol->icon = g_themed_icon_new_with_default_fallbacks(vol->symbolic ? "audio-volume-muted-symbolic" : "audio-volume-muted-panel");
    }
    else if (level >= 66)
    {
        vol->icon = g_themed_icon_new_with_default_fallbacks(vol->symbolic ? "audio-volume-high-symbolic" : "audio-volume-high-panel");
    }
    else if (level >= 33)
    {
        vol->icon = g_themed_icon_new_with_default_fallbacks(vol->symbolic ? "audio-volume-medium-symbolic" : "audio-volume-medium-panel");
    }
    else if (level > 0)
    {
        vol->icon = g_themed_icon_new_with_default_fallbacks(vol->symbolic ? "audio-volume-low-symbolic" : "audio-volume-low-panel");
    }
}

/* Do a full redraw of the display. */
static void volumealsa_update_current_icon(VolumeALSAPlugin * vol,int level)
{

    volumealsa_lookup_current_icon(vol,level);

    /* Change icon, fallback to default icon if theme doesn't exsit */
    simple_panel_image_change_gicon(vol->tray_icon,vol->icon,vol->panel);
    /* Display current level in tooltip. */
    char * tooltip = g_strdup_printf("%s: %d", vol->channel, level);
    gtk_widget_set_tooltip_text(vol->plugin, tooltip);
    g_free(tooltip);
}


/*
 * Here we just update volume's vertical scale and mute check button.
 * The rest will be updated by signal handelrs.
 */
static void volumealsa_update_display(VolumeALSAPlugin * vol)
{
    /* Volume. */
    if (vol->volume_scale != NULL)
    {
        gtk_range_set_value(GTK_RANGE(vol->volume_scale), asound_get_volume(vol));
    }
}

static void on_button_toggled(GtkToggleButton* b, VolumeALSAPlugin* vol)
{
    if (vol->alsa_is_init)
        if (gtk_toggle_button_get_active(b))
            gtk_widget_show_all(vol->popup_window);
        else
            gtk_widget_hide(vol->popup_window);
    else
    {
        alsa_init(vol);
        gtk_toggle_button_set_active(b,FALSE);
    }

}

/* Handler for "button-press-event" signal on main widget. */
static gboolean volumealsa_button_press_event(GtkWidget * widget, GdkEventButton * event, SimplePanel * panel)
{
    VolumeALSAPlugin * vol = lxpanel_plugin_get_data(widget);
    /* Middle-click.  Toggle the mute status. */
    if (event->button == 2)
    {
        vol->mute = !vol->mute;
        volumealsa_toggle_muted(vol);
    }
    return TRUE;
}

/* Handler for "focus-out" signal on popup window. */
static gboolean volumealsa_popup_focus_out(GtkWidget * widget, GdkEvent * event, VolumeALSAPlugin * vol)
{
    /* Hide the widget. */
    gtk_widget_hide(vol->popup_window);
    GtkWidget* b = gtk_bin_get_child(GTK_BIN(vol->plugin));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b),FALSE);
    return FALSE;
}

/* Handler for "map" signal on popup window. */
static void volumealsa_popup_map(GtkWidget * widget, VolumeALSAPlugin * vol)
{
    lxpanel_plugin_adjust_popup_position(widget, vol->plugin);
}

/* Handler for "value_changed" signal on popup window vertical scale. */
static void volumealsa_popup_scale_changed(GtkRange * range, VolumeALSAPlugin * vol)
{
    int level = gtk_range_get_value(GTK_RANGE(vol->volume_scale));
    /* Reflect the value of the control to the sound system. */
    asound_set_volume(vol, level);

    /*
     * Redraw the controls.
     * Scale and check button do not need to be updated, as these are always
     * in sync with user's actions.
     */
    volumealsa_update_current_icon(vol, level);
}

/* Handler for "scroll-event" signal on popup window vertical scale. */
static void volumealsa_popup_scale_scrolled(GtkScale * scale, GdkEventScroll * evt, VolumeALSAPlugin * vol)
{
    /* Get the state of the vertical scale. */
    gdouble val = gtk_range_get_value(GTK_RANGE(vol->volume_scale));

    /* Dispatch on scroll direction to update the value. */
    if (evt->direction == GDK_SCROLL_UP)
        val += 2;
    else if (evt->direction == GDK_SCROLL_DOWN)
        val -= 2;
    else if (evt->direction = GDK_SCROLL_SMOOTH)
    {
      val -= evt->delta_y * 2;
      val = CLAMP (val, 0,100);
    }

    /* Reset the state of the vertical scale.  This provokes a "value_changed" event. */
    gtk_range_set_value(GTK_RANGE(vol->volume_scale), CLAMP((int)val, 0, 100));
}

/* Handler for "toggled" signal on popup window mute checkbox. */
static void volumealsa_toggle_muted(VolumeALSAPlugin * vol)
{
    /* Get the state of the mute toggle. */
    int level = gtk_range_get_value(GTK_RANGE(vol->volume_scale));
    /* Reflect the mute toggle to the sound system. */
    if (vol->master_element != NULL)
    {
        int chn;
        for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++)
            snd_mixer_selem_set_playback_switch(vol->master_element, chn, ((vol->mute) ? 0 : 1));
    }

    /*
     * Redraw the controls.
     * Scale and check button do not need to be updated, as these are always
     * in sync with user's actions.
     */
    volumealsa_update_current_icon(vol, level);
}

static void volumealsa_set_menu_command(GtkWidget* p, GMenu* menu)
{
    VolumeALSAPlugin * vol = lxpanel_plugin_get_data(p);
    gchar* action = g_strdup_printf(LAUNCH_COMMAND_ACTION,vol->mixer_command);
    g_menu_prepend(menu,_("Sound settings..."),action);
    g_free(action);
}

/* Build the window that appears when the top level widget is clicked. */
static void volumealsa_build_popup_window(GtkWidget *p)
{
    VolumeALSAPlugin * vol = lxpanel_plugin_get_data(p);

    /* Create a new window. */
    vol->popup_window = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_decorated(GTK_WINDOW(vol->popup_window), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(vol->popup_window), 0);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(vol->popup_window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(vol->popup_window), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(vol->popup_window),-1,145);
    gtk_window_set_type_hint(GTK_WINDOW(vol->popup_window), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
    gtk_widget_add_events(vol->popup_window,GDK_FOCUS_CHANGE_MASK);
    gtk_window_set_attached_to(GTK_WINDOW(vol->popup_window),vol->plugin);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(vol->popup_window), "focus-out-event", G_CALLBACK(volumealsa_popup_focus_out), vol);
    g_signal_connect(G_OBJECT(vol->popup_window), "map", G_CALLBACK(volumealsa_popup_map), vol);

    /* Create a scrolled window as the child of the top level window. */
    GtkWidget * scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER(scrolledwindow), 0);
    gtk_widget_show(scrolledwindow);
    gtk_container_add(GTK_CONTAINER(vol->popup_window), scrolledwindow);
    gtk_widget_set_can_focus(scrolledwindow, FALSE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_SHADOW_NONE);

    /* Create a viewport as the child of the scrolled window. */
    GtkWidget * viewport = gtk_viewport_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), viewport);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_IN);
    gtk_widget_show(viewport);

    /* Create a vertical box as the child of the frame. */
    GtkWidget * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(viewport), box);

    /* Create a vertical scale as the child of the vertical box. */
    vol->volume_scale = gtk_scale_new(GTK_ORIENTATION_VERTICAL,GTK_ADJUSTMENT(gtk_adjustment_new(100, 0, 100, 0, 0, 0)));
    gtk_scale_set_draw_value(GTK_SCALE(vol->volume_scale), FALSE);
    gtk_range_set_inverted(GTK_RANGE(vol->volume_scale), TRUE);
    gtk_box_pack_start(GTK_BOX(box), vol->volume_scale, TRUE, TRUE, 0);

    /* Value-changed and scroll-event signals. */
    vol->volume_scale_handler = g_signal_connect(vol->volume_scale, "value-changed", G_CALLBACK(volumealsa_popup_scale_changed), vol);
    g_signal_connect(vol->volume_scale, "scroll-event", G_CALLBACK(volumealsa_popup_scale_scrolled), vol);
}

static void alsa_init(VolumeALSAPlugin* vol)
{
    if (asound_initialize(vol))
    {
        vol->alsa_is_init = TRUE;
        volumealsa_build_popup_window(vol->plugin);
        /* Connect signals. */
        g_signal_connect(G_OBJECT(vol->plugin), "scroll-event", G_CALLBACK(volumealsa_popup_scale_scrolled), vol );
        /* Update the display, show the widget, and return. */
        volumealsa_update_display(vol);
        int level = gtk_range_get_value(GTK_RANGE(vol->volume_scale));
        volumealsa_update_current_icon(vol,level);
    }
    else
    {
        gtk_widget_set_tooltip_text(vol->plugin, _("ALSA is not connected."));
        vol->alsa_is_init = FALSE;
    }

}

static void alsa_deinit(VolumeALSAPlugin* vol)
{
    if (vol->alsa_is_init)
    {
        if (vol->popup_window)
            gtk_widget_destroy(vol->popup_window);
        g_signal_handlers_disconnect_by_func(vol->plugin,volumealsa_popup_scale_scrolled,vol);
        asound_deinitialize(vol);
        vol->icon = g_icon_new_for_string(vol->symbolic ? "dialog-error-symbolic" : "dialog-error",NULL);
        simple_panel_image_change_gicon(vol->tray_icon,vol->icon,vol->panel);
        gtk_widget_set_tooltip_text(vol->plugin, _("ALSA is not connected."));
        vol->alsa_is_init = FALSE;
    }
}

static GtkWidget* get_background_widget(GtkWidget* plugin)
{
    VolumeALSAPlugin *b = lxpanel_plugin_get_data(plugin);
    return gtk_bin_get_child(GTK_BIN(b->plugin));
}

/* Plugin constructor. */
static GtkWidget *volumealsa_constructor(SimplePanel *panel, GSettings *settings)
{
    /* Allocate and initialize plugin context and set into Plugin private data pointer. */
    VolumeALSAPlugin * vol = g_new0(VolumeALSAPlugin, 1);
    GtkWidget *p, *b;
    vol->icon == NULL;
    vol->settings = settings;
    vol->panel = panel;
    vol->symbolic = g_settings_get_boolean(settings,VOLUMEALSA_PROP_USE_SYMBOLIC);
    vol->channel = g_settings_get_string(settings,VOLUMEALSA_PROP_CHANNEL);
    vol->card_id = g_settings_get_string(settings,VOLUMEALSA_PROP_CARD_ID);
    vol->mixer_command = g_settings_get_string(settings,VOLUMEALSA_PROP_MIXER);

    /* Allocate top level widget and set into Plugin widget pointer. */
    vol->plugin = p = gtk_event_box_new();
    lxpanel_plugin_set_data(p, vol, volumealsa_destructor);
    b = gtk_toggle_button_new();
    gtk_container_add(GTK_CONTAINER(vol->plugin),b);
    gtk_widget_add_events(GTK_WIDGET(b), GDK_SCROLL_MASK);
    g_signal_connect(b,"toggled",G_CALLBACK(on_button_toggled),vol);
    vol->icon = g_themed_icon_new_with_default_fallbacks(vol->symbolic ? "dialog-error-symbolic" : "dialog-error");
    vol->tray_icon = simple_panel_image_new_for_gicon(panel,vol->icon,-1);
    simple_panel_setup_button(b,vol->tray_icon,NULL);
    /* Initialize ALSA.  If that fails, present missing icon. */
    alsa_init(vol);

    gtk_widget_show_all(p);
    return p;
}

/* Plugin destructor. */
static void volumealsa_destructor(gpointer user_data)
{
    VolumeALSAPlugin * vol = (VolumeALSAPlugin *) user_data;

    asound_deinitialize(vol);

    /* If the dialog box is open, dismiss it. */
    if (vol->popup_window != NULL)
        gtk_widget_destroy(vol->popup_window);

    if (vol->restart_idle)
        g_source_remove(vol->restart_idle);
    if (vol->icon != NULL);
        g_object_unref(vol->icon);
    g_free(vol->card_id);
    g_free(vol->channel);
    g_free(vol->mixer_command);
    /* Deallocate all memory. */
    g_free(vol);
}

/* Callback when the configuration dialog is to be shown. */

static gboolean apply_config(gpointer user_data)
{
    GtkWidget *p = user_data;
    VolumeALSAPlugin* m = lxpanel_plugin_get_data(p);
    g_settings_set_string(m->settings, VOLUMEALSA_PROP_CARD_ID, m->card_id);
    g_settings_set_string(m->settings, VOLUMEALSA_PROP_CHANNEL, m->channel);
    g_settings_set_string(m->settings, VOLUMEALSA_PROP_MIXER, m->mixer_command);
    g_settings_set_boolean(m->settings,VOLUMEALSA_PROP_USE_SYMBOLIC,m->symbolic);
    alsa_deinit(m);
    alsa_init(m);
    return FALSE;
}

static GtkWidget *volumealsa_configure(SimplePanel *panel, GtkWidget *p)
{
    VolumeALSAPlugin * vol = lxpanel_plugin_get_data(p);
    return lxpanel_generic_config_dlg(_("Volume Control"), panel, apply_config, p,
                                      _("Use symbolic icons"), &vol->symbolic, CONF_TYPE_BOOL,
                                      _("ALSA Card"), &vol->card_id, CONF_TYPE_STR,
                                      _("ALSA Channel"), &vol->channel, CONF_TYPE_STR,
                                      _("Mixer launch command"), &vol->mixer_command, CONF_TYPE_STR,
                                      NULL);
}

/* Callback when panel configuration changes. */
static void volumealsa_panel_configuration_changed(SimplePanel *panel, GtkWidget *p)
{
    /* Do a full redraw. */
    volumealsa_update_display(lxpanel_plugin_get_data(p));
}

FM_DEFINE_MODULE(lxpanel_gtk, volumealsa)

/* Plugin descriptor. */
/* FIXME: make GSettings for select a channel*/
SimplePanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Volume Control"),
    .description = N_("Display and control volume for ALSA"),

    .new_instance = volumealsa_constructor,
    .update_context_menu = volumealsa_set_menu_command,
    .config = volumealsa_configure,
    .reconfigure = volumealsa_panel_configuration_changed,
    .button_press_event = volumealsa_button_press_event,
    .has_config = TRUE
};

/* vim: set sw=4 et sts=4 : */
