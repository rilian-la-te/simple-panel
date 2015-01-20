/**
 * Copyright (c) 2012-2014 Piotr Sipika; see the AUTHORS file for more.
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
 * 
 * See the COPYRIGHT file for more information.
 */

#include "location.h"
#include "weatherwidget.h"
#include "yahooutil.h"
#include "logutil.h"


#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "plugin.h"
/* Need to maintain count for bookkeeping */
static gint g_iCount = 0;

typedef struct
{
  gint iMyId_;
  GtkWidget *pWeather_;
  GSettings *pConfig_;
} WeatherPluginPrivate;


/**
 * Weather Plugin destructor.
 *
 * @param pData Pointer to the plugin data (private).
 */
static void
weather_destructor(gpointer pData)
{
  WeatherPluginPrivate * pPriv = (WeatherPluginPrivate *) pData;

  LXW_LOG(LXW_DEBUG, "weather_destructor(%d): %d", pPriv->iMyId_, g_iCount);

  g_free(pPriv);

  --g_iCount;

  if (g_iCount == 0)
    {
      cleanupYahooUtil();

      cleanupLogUtil();
    }
}

/**
 * Weather Plugin constructor
 *
 * @param pPlugin Pointer to the PluginClass wrapper instance.
 * @param setting Pointer to the configuration settings for this plugin.
 *
 * @return Pointer to a new weather widget.
 */
static GtkWidget *
weather_constructor(SimplePanel *pPanel, GSettings *pConfig)
{
  WeatherPluginPrivate * pPriv = g_new0(WeatherPluginPrivate, 1);

  pPriv->pConfig_ = pConfig;

  /* There is one more now... */
  ++g_iCount;

  pPriv->iMyId_ = g_iCount;

  if (g_iCount == 1)
    {
      initializeLogUtil("syslog");
      
      setMaxLogLevel(LXW_ERROR);

      initializeYahooUtil();
    }

  LXW_LOG(LXW_DEBUG, "weather_constructor()");
  
  GtkWidget * pWidg = gtk_weather_new(FALSE);

  pPriv->pWeather_ = pWidg;

  GtkWidget * pEventBox = gtk_event_box_new();

  lxpanel_plugin_set_data(pEventBox, pPriv, weather_destructor);
  gtk_container_add(GTK_CONTAINER(pEventBox), pWidg);

  gtk_widget_set_has_window(pEventBox, FALSE);

  gtk_widget_show_all(pEventBox);

  /* use config settings */
  LocationInfo * pLocation = g_new0(LocationInfo, 1);

  pLocation->pcAlias_ = g_settings_get_string(pConfig, "alias");
  pLocation->pcCity_ = g_settings_get_string(pConfig, "city");
  pLocation->pcState_ = g_settings_get_string(pConfig, "state");
  pLocation->pcCountry_ = g_settings_get_string(pConfig, "country");
  pLocation->pcWOEID_ = g_settings_get_string(pConfig, "woeid");
  gchar* tmp = g_settings_get_string(pConfig, "units");
  pLocation->cUnits_ = tmp[0];
  g_free(tmp);
  pLocation->uiInterval_ = g_settings_get_uint(pConfig, "interval");
  pLocation->bEnabled_ = g_settings_get_boolean(pConfig, "enabled");
  if (pLocation->pcAlias_ && pLocation->pcWOEID_)
    {
      GValue locationValue = G_VALUE_INIT;

      g_value_init(&locationValue, G_TYPE_POINTER);

      /* location is copied by the widget */
      g_value_set_pointer(&locationValue, pLocation);

      g_object_set_property(G_OBJECT(pWidg),
                            "location",
                            &locationValue);
    }

  freeLocation(pLocation);

  return pEventBox;
}

/**
 * Weather Plugin callback to save configuration
 *
 * @param pWidget Pointer to this widget.
 */
void weather_save_configuration(GtkWidget * pWeather, LocationInfo * pLocation)
{
  GtkWidget * pWidget = gtk_widget_get_parent(pWeather);
  WeatherPluginPrivate * pPriv = NULL;

  if (pWidget)
    {
      pPriv = (WeatherPluginPrivate *) lxpanel_plugin_get_data(pWidget);
    }
  if (pPriv == NULL)
    {
      LXW_LOG(LXW_ERROR, "Weather: weather_save_configuration() for invalid widget");
      return;
    }

  LXW_LOG(LXW_DEBUG, "weather_save_configuration(%d)", pPriv->iMyId_);

  if (pLocation)
    {
      /* save configuration */
      g_settings_set_string(pPriv->pConfig_, "alias", pLocation->pcAlias_);
      g_settings_set_string(pPriv->pConfig_, "city", pLocation->pcCity_);
      g_settings_set_string(pPriv->pConfig_, "state", pLocation->pcState_);
      g_settings_set_string(pPriv->pConfig_, "country", pLocation->pcCountry_);
      g_settings_set_string(pPriv->pConfig_, "woeid", pLocation->pcWOEID_);

      char units[2] = {0};
      if (snprintf(units, 2, "%c", pLocation->cUnits_) > 0)
        {
          g_settings_set_string(pPriv->pConfig_, "units", units);
        }

      g_settings_set_uint(pPriv->pConfig_, "interval", (int) pLocation->uiInterval_);
      g_settings_set_boolean(pPriv->pConfig_, "enabled", (int) pLocation->bEnabled_);
    }

}

/**
 * Weather Plugin configuration change callback.
 *
 * @param pPanel  Pointer to the panel instance.
 * @param pPlugin Pointer to the PluginClass wrapper instance.
 */
static void
weather_configuration_changed(SimplePanel *pPanel, GtkWidget *pWidget)
{
  LXW_LOG(LXW_DEBUG, "weather_configuration_changed()");

  if (pPanel && pWidget)
    {
      LXW_LOG(LXW_DEBUG, 
             "   orientation: %s, width: %d, height: %d, icon size: %d\n", 
              (panel_get_orientation(pPanel) == GTK_ORIENTATION_HORIZONTAL)?"HORIZONTAL":
              (panel_get_orientation(pPanel) == GTK_ORIENTATION_VERTICAL)?"VERTICAL":"NONE",
              pPanel->width, panel_get_height(pPanel), 
              panel_get_icon_size(pPanel));
    }
}

/**
 * Weather Plugin configuration dialog callback.
 *
 * @param pPanel  Pointer to the panel instance.
 * @param pWidget Pointer to the Plugin widget instance.
 * @param pParent Pointer to the GtkWindow parent.
 *
 * @return Instance of the widget.
 */
static GtkWidget *
weather_configure(SimplePanel *pPanel G_GNUC_UNUSED, GtkWidget *pWidget)
{
  LXW_LOG(LXW_DEBUG, "weather_configure()");

  WeatherPluginPrivate * pPriv = (WeatherPluginPrivate *) lxpanel_plugin_get_data(pWidget);

  GtkWidget * pDialog = gtk_weather_create_preferences_dialog(GTK_WIDGET(pPriv->pWeather_));

  return pDialog;
}

FM_DEFINE_MODULE(lxpanel_gtk, weather)

/**
 * Definition of the weather plugin module
 */
SimplePanelPluginInit fm_module_init_lxpanel_gtk =
  {
    .name = N_("Weather Plugin"),
    .description = N_("Show weather conditions for a location."),

    // API functions
    .new_instance = weather_constructor,
    .config = weather_configure,
    .reconfigure = weather_configuration_changed,
    .has_config = TRUE
  };
