/**
 * Copyright (c) 2006, 2008, 2014 LxDE Developers,
 * 	see the file AUTHORS for details.
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

#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "plugin.h"

#include "dbg.h"

#include "volume-impl.h"

#include "volume_xpm.h"

int mixer_fd;

GtkSpinButton *vol_spin;
static gdouble vol_before_mute;
static gdouble curr_volume;
static GtkWidget *curr_image;
static gboolean skip_botton1_event;

typedef struct {
    SimplePanel *panel;
    GtkWidget *dlg;
} volume_t;

static void
volume_destructor(gpointer user_data)
{
    volume_t *vol = (volume_t *) user_data;

    ENTER;
    if (vol->dlg)
        gtk_widget_destroy(vol->dlg);
    if (mixer_fd)
        close(mixer_fd);
    g_free(vol);
    RET();
}

static void update_icon (GtkWidget *p, volume_t *vol)
{
    GIcon* icon;
	GtkWidget *image;

	if (curr_volume <= 0) {
        icon = g_themed_icon_new_with_default_fallbacks("audio-volume-muted-panel");
	}
	else if (curr_volume > 0 && curr_volume <= 50) {
        icon = g_themed_icon_new_with_default_fallbacks("audio-volume-low-panel");
	}
	else if (curr_volume > 50 && curr_volume <= 75) {
        icon = g_themed_icon_new_with_default_fallbacks("audio-volume-medium-panel");
    }
	else /* curr_volume > 75 */ {
        icon = g_themed_icon_new_with_default_fallbacks("audio-volume-high-panel");
    }

	if (icon) {
		if (curr_image) {
			gtk_widget_destroy(curr_image);
			curr_image = NULL;
		}
        image = simple_panel_image_new_for_gicon(vol->panel,icon,-1);
        g_object_unref(icon);
		gtk_container_add(GTK_CONTAINER(p), image);

		curr_image = image;
	}
	gtk_widget_show_all(p);
	return;
}

static void on_volume_focus (GtkWidget* dlg, GdkEventFocus *event, GtkWidget *p)
{
	volume_t *vol = lxpanel_plugin_get_data(p);

	if (! vol_spin) return;
	GtkAdjustment *vol_adjustment = gtk_spin_button_get_adjustment (vol_spin);
	if (! vol_adjustment) return;
	curr_volume = gtk_adjustment_get_value (vol_adjustment);

	update_icon(p, vol);

	/* FIXME: use smarter method */
	gtk_widget_destroy( dlg );
	vol->dlg = NULL;
}

static void on_mouse_scroll (GtkWidget* widget, GdkEventScroll* evt, volume_t *vol)
{
	if ( ! vol->dlg ) {

		vol->dlg = create_volume_window();
		g_signal_connect( vol->dlg, "delete-event",
				G_CALLBACK(on_volume_focus), widget );

	}
	else {
		if (! vol_spin) return;
		GtkAdjustment *vol_adjustment =
			gtk_spin_button_get_adjustment (vol_spin);
		if (! vol_adjustment) return;

		curr_volume = gtk_adjustment_get_value (vol_adjustment);

		if (evt->direction == GDK_SCROLL_UP) {
			curr_volume += 2;
		}
		else /*if (evt->direction == GDK_SCROLL_DOWN)*/ {
			curr_volume -= 2;
		}
		update_icon(widget, vol);
		gtk_adjustment_set_value (vol_adjustment, curr_volume);
		gtk_spin_button_set_adjustment(vol_spin, vol_adjustment);
		skip_botton1_event = TRUE;
	}
}

static gboolean on_button_press (GtkWidget* widget, GdkEventButton* evt, SimplePanel* p)
{
	volume_t *vol = lxpanel_plugin_get_data(widget);

	/* for scroll correction */
	if (skip_botton1_event) {
		gtk_widget_destroy(vol->dlg);
		vol->dlg = NULL;
		skip_botton1_event = FALSE;
	}

	switch ( evt->button ) {
	case 1: {	/*  Left click */
		if ( ! vol->dlg ) {
			vol->dlg = create_volume_window();

			/* setting background to default */
			gtk_widget_set_style(vol->dlg, panel_get_defstyle(p));

			g_signal_connect( vol->dlg, "focus-out-event",
					G_CALLBACK(on_volume_focus), widget );

			gtk_window_present( GTK_WINDOW(vol->dlg) );
		}
		else {
			/* update icon */
			if (! vol_spin) return FALSE;
			GtkAdjustment *vol_adjustment =
				gtk_spin_button_get_adjustment (vol_spin);
			if (! vol_adjustment) return FALSE;
			curr_volume = gtk_adjustment_get_value (vol_adjustment);
			update_icon(widget, vol);

			gtk_widget_destroy(vol->dlg);
			vol->dlg = NULL;
		}
		break;
	}

	case 2:	{	/* middle mouse button */
		if ( ! vol->dlg ) {
			vol->dlg = create_volume_window();
		}

		if (! vol_spin) return FALSE;
		GtkAdjustment *vol_adjustment =
			gtk_spin_button_get_adjustment (vol_spin);
		if (! vol_adjustment) return FALSE;

		curr_volume = gtk_adjustment_get_value (vol_adjustment);

		if (curr_volume > 0) {
			/* turning to mute */
			vol_before_mute = curr_volume;
			curr_volume = 0;
		}
		else {
			curr_volume = vol_before_mute;
		}

		gtk_adjustment_set_value (vol_adjustment, curr_volume);
		gtk_spin_button_set_adjustment(vol_spin, vol_adjustment);

		update_icon(widget, vol);

		gtk_widget_destroy( vol->dlg );
		vol->dlg = NULL;
		break;
	}
	default:	/* never here */
		break;
	}
	return FALSE;
}

static GtkWidget *volume_constructor(SimplePanel *panel, GSettings *settings)
{
    volume_t *vol;
    GtkWidget *p;
    GtkAdjustment *vol_adjustment;

    vol_before_mute = 1;
    curr_volume = 0;
    curr_image = NULL;
    skip_botton1_event = FALSE;

    ENTER;
    /* check if OSS mixer device could be open */
    mixer_fd = open ("/dev/mixer", O_RDWR, 0);
    if (mixer_fd < 0) {
        RET(NULL);
    }
    /* try to obtain current volume */
    p = create_volume_window(); /* use pointer */
    if (! vol_spin)
        goto _error;
    vol_adjustment = gtk_spin_button_get_adjustment (vol_spin);
    if (! vol_adjustment)
    {
_error:
        gtk_widget_destroy(p);
        RET(NULL);
    }
    curr_volume = gtk_adjustment_get_value (vol_adjustment);

    vol = g_new0(volume_t, 1);
    vol->dlg = p; /* it was reused */

    p = gtk_event_box_new();
    lxpanel_plugin_set_data(p, vol, volume_destructor);
    vol->panel = panel;
    int icon_size;
    g_object_get(panel,PANEL_PROP_ICON_SIZE, &icon_size,NULL);

    g_signal_connect(p, "scroll-event", G_CALLBACK(on_mouse_scroll), vol);
    gtk_widget_set_size_request(p, icon_size, icon_size);

    update_icon(p, vol);
    gtk_widget_destroy( vol->dlg );
    vol->dlg = NULL;

    /* FIXME: display current level in tooltip. ex: "Volume Control: 80%"  */
    gtk_widget_set_tooltip_text(p, _("Volume control"));

    RET(p);
}

FM_DEFINE_MODULE(lxpanel_gtk, volume)

SimplePanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Volume Control"),
    .description = "Display and control volume",

    .new_instance = volume_constructor,
    .button_press_event = on_button_press,
    .has_config = FALSE
};
