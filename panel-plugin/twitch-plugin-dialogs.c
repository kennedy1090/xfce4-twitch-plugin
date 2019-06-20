/*  $Id$
 *
 *  Copyright (C) 2012 John Doo <john@foo.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include "twitch-plugin.h"
#include "twitch-plugin-dialogs.h"

/* the website url */
#define PLUGIN_WEBSITE ""

inline void refresh(TwitchPlugin *twitch) {
  twitch_plugin_apply_settings(twitch);
  twitch_init(twitch->api);
}

static void
twitch_plugin_configure_response (GtkWidget    *dialog,
                           gint          response,
                           TwitchPlugin *twitch)
{
  gboolean result;
  DialogSettings *settings = twitch->settings;

  if (response == GTK_RESPONSE_HELP)
    {
      /* show help */
      result = g_spawn_command_line_async ("exo-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);

      if (G_UNLIKELY (result == FALSE))
        g_warning (_("Unable to open the following url: %s"), PLUGIN_WEBSITE);
    }
  else
    {
      if(response == GTK_RESPONSE_APPLY) {
        twitch_free_user(&twitch->api->user);

        twitch->api->user.name = g_strdup(gtk_entry_get_text(GTK_ENTRY(settings->username)));
        if(twitch->api->client_id) g_free(twitch->api->client_id);
        twitch->api->client_id = g_strdup(gtk_entry_get_text(GTK_ENTRY(settings->client_id)));
        if(twitch->api->access_token) g_free(twitch->api->access_token);
        twitch->api->access_token = g_strdup(gtk_entry_get_text(GTK_ENTRY(settings->access_token)));
        twitch->update_status_rate = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(settings->update_status));
        twitch->update_users_rate = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(settings->update_users)) * 60;

        if(g_str_has_prefix(twitch->api->access_token, "oauth:")) {
          twitch->api->access_token += 6;
        }

        gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(settings->color_picker), &twitch->color);
      }
      /* remove the dialog data from the plugin */
      g_object_set_data (G_OBJECT (twitch->plugin), "dialog", NULL);

      /* unlock the panel menu */
      xfce_panel_plugin_unblock_menu (twitch->plugin);

      /* save the plugin */
      twitch_plugin_save (twitch->plugin, twitch);

      /* destroy the properties dialog */
      gtk_widget_destroy (dialog);

      if(response == GTK_RESPONSE_APPLY) refresh(twitch);
    }
}



void
twitch_plugin_configure (XfcePanelPlugin *plugin,
                  TwitchPlugin    *twitch)
{
  GtkWidget *dialog, *grid;
  GdkRGBA default_color;
  DialogSettings *settings = g_new0(DialogSettings, 1);
  twitch->settings = settings;

  /* block the plugin menu */
  xfce_panel_plugin_block_menu (plugin);

  /* create the dialog */
  dialog = xfce_titled_dialog_new_with_buttons (_("Twitch Plugin"),
                                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                "gtk-help", GTK_RESPONSE_HELP,
                                                "_Close", GTK_RESPONSE_CLOSE,
                                                "_Apply", GTK_RESPONSE_APPLY,
                                                NULL);

  /* center dialog on the screen */
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  /* set dialog icon */
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "xfce4-settings");

  /* add widgets */
  grid = gtk_grid_new();
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid, FALSE, FALSE, 0);
  settings->username = gtk_entry_new();
  settings->client_id = gtk_entry_new();
  settings->access_token = gtk_entry_new();
  settings->color_picker = gtk_color_button_new();
  settings->update_status = gtk_spin_button_new_with_range(0, 60*5, 10);
  settings->update_users = gtk_spin_button_new_with_range(0, 60, 1);
  
  //populate with currently loaded values
  gtk_entry_set_text(GTK_ENTRY(settings->username), twitch->api->user.name);
  gtk_entry_set_text(GTK_ENTRY(settings->client_id), twitch->api->client_id);
  gtk_entry_set_text(GTK_ENTRY(settings->access_token), twitch->api->access_token);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(settings->update_status), twitch->update_status_rate);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(settings->update_users), twitch->update_users_rate/60);
  //set briefly to TWITCH_PURPLE to try to save it in the color picker custom colors
  gdk_rgba_parse(&default_color, TWITCH_PURPLE);
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(settings->color_picker), &default_color);
  gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(settings->color_picker), &twitch->color);

  gtk_grid_attach(GTK_GRID(grid), gtk_label_new(_("Twitch username: ")), 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), settings->username, 1, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), gtk_label_new(_("Client ID: ")), 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), settings->client_id, 1, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), gtk_label_new(_("OR")), 0, 2, 2, 1);
  gtk_grid_attach(GTK_GRID(grid), gtk_label_new(_("Access Token: ")), 0, 3, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), settings->access_token, 1, 3, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), gtk_label_new(_("Icon color: ")), 0, 4, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), settings->color_picker, 1, 4, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), gtk_label_new(_("Update rate (sec):")), 0, 5, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), settings->update_status, 1, 5, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), gtk_label_new(_("Update following (min):")), 0, 6, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), settings->update_users, 1, 6, 1, 1);

  /* link the dialog to the plugin, so we can destroy it when the plugin
   * is closed, but the dialog is still open */
  g_object_set_data (G_OBJECT (plugin), "dialog", dialog);

  /* connect the reponse signal to the dialog */
  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK(twitch_plugin_configure_response), twitch);

  /* show the entire dialog */
  gtk_widget_show_all(dialog);
}



void
twitch_plugin_about (XfcePanelPlugin *plugin)
{
  /* about dialog code. you can use the GtkAboutDialog
   * or the XfceAboutInfo widget */
  GdkPixbuf *icon;

  const gchar *auth[] =
    {
      "Xfce Dev <xfce4-dev@xfce.org>",
      NULL
    };

  icon = xfce_panel_pixbuf_from_source ("twitch-purple", NULL, 32);
  gtk_show_about_dialog (NULL,
                         "logo",         icon,
                         "license",      xfce_get_license_text (XFCE_LICENSE_TEXT_GPL),
                         "version",      PACKAGE_VERSION,
                         "program-name", PACKAGE_NAME,
                         "comments",     _("Twitch following plugin"),
                         "website",      PLUGIN_WEBSITE,
                         "copyright",    _("Copyright \xc2\xa9 2019-2019 Will Kennedy\n"),
                         "authors",      auth,
                         NULL);

  if (icon)
    g_object_unref (G_OBJECT (icon));
}
