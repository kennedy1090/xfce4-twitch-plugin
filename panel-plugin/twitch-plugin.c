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
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include "twitch-plugin.h"
#include "twitch-plugin-dialogs.h"

/* default settings */
#define DEFAULT_USERNAME ""
#define DEFAULT_ID ""
#define DEFAULT_TOKEN ""
#define UPDATE_STATUS_SECONDS 60
#define UPDATE_USERS_SECONDS 1800

#define IS_HORIZ(twitch) xfce_panel_plugin_get_orientation(twitch->plugin) == GTK_ORIENTATION_HORIZONTAL
#define SIZE(twitch) xfce_panel_plugin_get_size(twitch->plugin)
#define NROWS(twitch) xfce_panel_plugin_get_nrows(twitch->plugin)
#define ICON_SIZE(twitch) ((SIZE(twitch) - 2) / NROWS(twitch) - 2)
#define GET_H(twitch, i) (IS_HORIZ(twitch)? NROWS(twitch) + i / NROWS(twitch) : i % NROWS(twitch))
#define GET_V(twitch, i) (IS_HORIZ(twitch)? i % NROWS(twitch) : NROWS(twitch) + i / NROWS(twitch))



/* prototypes */
static void
twitch_plugin_construct (XfcePanelPlugin *plugin);

// static void
// create_grid_layout (TwitchPlugin *);

static void
add_button (TwitchPlugin *, TwitchUser *);

static void
remove_button (TwitchPlugin *, TwitchUser *);

static void 
check_button(gpointer, gpointer, gpointer);

static void
update_button(gpointer, gpointer, gpointer);

/* register the plugin */
XFCE_PANEL_PLUGIN_REGISTER(twitch_plugin_construct);



void
twitch_plugin_save (XfcePanelPlugin *plugin,
             TwitchPlugin    *twitch)
{
  XfceRc *rc;
  gchar  *file, *color;

  /* get the config file location */
  file = xfce_panel_plugin_save_location (plugin, TRUE);

  if (G_UNLIKELY (file == NULL))
    {
       DBG ("Failed to open config file");
       return;
    }

  /* open the config file, read/write */
  rc = xfce_rc_simple_open (file, FALSE);
  g_free (file);

  if (G_LIKELY (rc != NULL))
    {
      /* save the settings */
      DBG(".");
      if (twitch->api->user.name)
        xfce_rc_write_entry (rc, "twitch-username", twitch->api->user.name);

      if(twitch->api->client_id)
        xfce_rc_write_entry (rc, "twitch-client-id", twitch->api->client_id);

      if(twitch->api->access_token)
        xfce_rc_write_entry (rc, "twitch-access-token", twitch->api->access_token);

      color = gdk_rgba_to_string(&twitch->color);
      xfce_rc_write_entry(rc, "twitch-logo-color", color);
      g_free(color);
      /* close the rc file */
      xfce_rc_close (rc);
    }
}



static void
twitch_plugin_read (TwitchPlugin *twitch)
{
  XfceRc      *rc;
  gchar       *file;
  const gchar *value;

  /* get the plugin config file location */
  file = xfce_panel_plugin_save_location (twitch->plugin, TRUE);

  if (G_LIKELY (file != NULL))
    {
      /* open the config file, readonly */
      rc = xfce_rc_simple_open (file, TRUE);

      /* cleanup */
      g_free (file);

      if (G_LIKELY (rc != NULL))
        {
          /* read the settings */
          value = xfce_rc_read_entry (rc, "twitch-username", DEFAULT_USERNAME);
          twitch->api->user.name = g_strdup (value);

          value = xfce_rc_read_entry (rc, "twitch-client-id", DEFAULT_ID);
          twitch->api->client_id = g_strdup (value);

          value = xfce_rc_read_entry (rc, "twitch-access-token", DEFAULT_TOKEN);
          twitch->api->access_token = g_strdup (value);

          value = xfce_rc_read_entry (rc, "twitch-logo-color", TWITCH_PURPLE);
          gdk_rgba_parse (&twitch->color, value);
          /* cleanup */
          xfce_rc_close (rc);

          /* leave the function, everything went well */
          return;
        }
    }

  /* something went wrong, apply default values */
  DBG ("Applying default settings");

  twitch->api->user.name = g_strdup (DEFAULT_USERNAME);
  twitch->api->client_id = g_strdup (DEFAULT_ID);
  twitch->api->access_token = g_strdup (DEFAULT_TOKEN);
  gdk_rgba_parse (&twitch->color, TWITCH_PURPLE);
}



static TwitchPlugin *
twitch_plugin_new (XfcePanelPlugin *plugin)
{
  TwitchPlugin   *twitch;

  /* allocate memory for the plugin structure */
  twitch = g_slice_new0 (TwitchPlugin);

  /* pointer to plugin */
  twitch->plugin = plugin;

  twitch->api = g_slice_new0 (TwitchApi);
  twitch->buttons = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_object_unref);


  /* read the user settings */
  twitch_plugin_read (twitch);
  twitch->provider = gtk_css_provider_new();
  twitch_plugin_apply_settings(twitch);
  twitch->api->init_complete = twitch_init(twitch->api);

  /* create some panel widgets */
  twitch->ebox = gtk_event_box_new ();
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(twitch->ebox), FALSE);
  gtk_event_box_set_above_child(GTK_EVENT_BOX(twitch->ebox), TRUE);
  gtk_widget_show (twitch->ebox);

  twitch->hvbox = gtk_box_new(xfce_panel_plugin_get_orientation(plugin), 0);
  gtk_container_add(GTK_CONTAINER(twitch->ebox), twitch->hvbox);

  twitch->icon = gtk_image_new_from_icon_name("twitch-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);

  g_object_ref_sink(twitch->icon);
  gtk_style_context_add_provider(GTK_STYLE_CONTEXT(gtk_widget_get_style_context(twitch->icon)),
                          GTK_STYLE_PROVIDER(twitch->provider),
                          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  gtk_box_pack_start(GTK_BOX(twitch->hvbox), twitch->icon, FALSE, FALSE, 5);
  gtk_widget_show_all(twitch->hvbox);

  g_timeout_add_seconds(UPDATE_STATUS_SECONDS, twitch_plugin_update_status, twitch);
  g_timeout_add_seconds(UPDATE_USERS_SECONDS, twitch_plugin_update_users, twitch);

  return twitch;
}

gboolean twitch_plugin_update_users(gpointer userdata) {
  TwitchPlugin *twitch = userdata;
  twitch_update_users(twitch->api);
  return TRUE;
}

gboolean twitch_plugin_update_status(gpointer userdata) {
  TwitchPlugin *twitch = userdata;
  twitch_update_status(twitch->api);
  g_hash_table_foreach(twitch->api->following, check_button, twitch);
  return TRUE;
}

void
twitch_plugin_apply_settings (TwitchPlugin *twitch) {
  gchar *css, *color;
  color = gdk_rgba_to_string(&twitch->color);
  css = g_strdup_printf(
    ".xfce4-panel button { padding: 1px }   \
     .xfce4-panel image,icon { color: %s }",
    color
  );
  g_free(color);
  gtk_css_provider_load_from_data(twitch->provider, css, -1, NULL);
}



static void
twitch_plugin_free (XfcePanelPlugin *plugin,
             TwitchPlugin    *twitch)
{
  GtkWidget *dialog;

  /* check if the dialog is still open. if so, destroy it */
  dialog = g_object_get_data (G_OBJECT (plugin), "dialog");
  if (G_UNLIKELY (dialog != NULL))
    gtk_widget_destroy (dialog);

  /* destroy the panel widgets */
  gtk_widget_destroy (twitch->hvbox);

  /* cleanup the settings */
  twitch_free(twitch->api);

  g_object_unref(twitch->icon);
  g_object_unref(G_OBJECT(twitch->provider));
  /* free the plugin structure */
  g_slice_free (TwitchPlugin, twitch);
}


static void
twitch_plugin_orientation_changed (XfcePanelPlugin *plugin,
                            GtkOrientation   orientation,
                            TwitchPlugin    *twitch)
{
  // create_grid_layout(twitch);
}
static gboolean
twitch_plugin_size_changed (XfcePanelPlugin *plugin,
                     gint             size,
                     TwitchPlugin    *twitch)
{
  GtkOrientation orientation;

  /* get the orientation of the plugin */
  orientation = xfce_panel_plugin_get_orientation (plugin);
  gtk_container_set_border_width(GTK_CONTAINER(twitch->hvbox), 0);
  twitch->size = ICON_SIZE(twitch);
  twitch_plugin_update_status
(twitch);
  // create_grid_layout(twitch);
  g_hash_table_foreach(twitch->buttons, update_button, twitch);

  /* set the widget size */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_widget_set_size_request (GTK_WIDGET (plugin), -1, size);
  else
    gtk_widget_set_size_request (GTK_WIDGET (plugin), size, -1);

  /* we handled the orientation */
  return TRUE;
}



static void
twitch_plugin_construct (XfcePanelPlugin *plugin)
{
  TwitchPlugin *twitch;

  /* setup transation domain */
  xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* create the plugin */
  twitch = twitch_plugin_new (plugin);

  /* add the ebox to the panel */
  gtk_container_add (GTK_CONTAINER (plugin), twitch->ebox);

  /* show the panel's right-click menu on this ebox */
  xfce_panel_plugin_add_action_widget (plugin, twitch->ebox);

  /* connect plugin signals */
  g_signal_connect (G_OBJECT (plugin), "free-data",
                    G_CALLBACK (twitch_plugin_free), twitch);

  g_signal_connect (G_OBJECT (plugin), "save",
                    G_CALLBACK (twitch_plugin_save), twitch);

  g_signal_connect (G_OBJECT (plugin), "size-changed",
                    G_CALLBACK (twitch_plugin_size_changed), twitch);

  g_signal_connect (G_OBJECT (plugin), "orientation-changed",
                    G_CALLBACK (twitch_plugin_orientation_changed), twitch);

  /* show the configure menu item and connect signal */
  xfce_panel_plugin_menu_show_configure (plugin);
  g_signal_connect (G_OBJECT (plugin), "configure-plugin",
                    G_CALLBACK (twitch_plugin_configure), twitch);

  /* show the about menu item and connect signal */
  xfce_panel_plugin_menu_show_about (plugin);
  g_signal_connect (G_OBJECT (plugin), "about",
                    G_CALLBACK (twitch_plugin_about), NULL);
}

static void update_button_img(TwitchPlugin *twitch, TwitchUser *user) {
  GtkWidget *button;
  GdkPixbuf *scaled = gdk_pixbuf_scale_simple(user->pfp, twitch->size, twitch->size, GDK_INTERP_BILINEAR);
  button = g_hash_table_lookup(twitch->buttons, user->id);
  gtk_button_set_image(GTK_BUTTON(button), gtk_image_new_from_pixbuf(scaled));
  g_object_unref(G_OBJECT(scaled));
}

static void add_button(TwitchPlugin *twitch, TwitchUser *user) {
  GtkWidget *button;
  gchar *link = g_strconcat("https://twitch.tv/", user->url, NULL);
  button = gtk_link_button_new(link);
  // g_object_ref_sink(G_OBJECT(button));
  gtk_button_set_label(GTK_BUTTON(button), NULL);
  gtk_widget_set_tooltip_text(button, user->status);
  g_hash_table_insert(twitch->buttons, user->id, button);
  update_button_img(twitch, user);
  gtk_style_context_add_provider(GTK_STYLE_CONTEXT(gtk_widget_get_style_context(button)),
                    GTK_STYLE_PROVIDER(twitch->provider),
                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  gtk_box_pack_start(GTK_BOX(twitch->hvbox), button, FALSE, FALSE, 0);
                  // GET_H(twitch, len), GET_V(twitch, len), 1, 1);
  gtk_widget_show(button);
  g_free(link);
}

static void remove_button(TwitchPlugin *twitch, TwitchUser *user) {
  GtkWidget *button = (GtkWidget*)g_hash_table_lookup(twitch->buttons, user->id);
  g_hash_table_remove(twitch->buttons, user->id);
  // g_object_unref(G_OBJECT(button));
  // gtk_widget_hide(button);
  gtk_container_remove(GTK_CONTAINER(twitch->hvbox), button);
}

static void update_button_status(TwitchPlugin *twitch, TwitchUser *user) {
  gchar *tooltip;
  GtkWidget *button = (GtkWidget*)g_hash_table_lookup(twitch->buttons, user->id);
  tooltip = gtk_widget_get_tooltip_text(button);
  if (!g_str_equal(tooltip, user->status)) {
    gtk_widget_set_tooltip_text(button, user->status);
  }
  g_free(tooltip);
}

static void check_button(gpointer key, gpointer value, gpointer userdata) {
  TwitchPlugin *twitch = (TwitchPlugin*)userdata;
  TwitchUser *user = (TwitchUser*)value;
  if (g_hash_table_contains(twitch->buttons, key)) {
    update_button_status(twitch, user);
    if (user->update_pfp) {
      update_button_img(twitch, user);
      user->update_pfp = FALSE;
    }
    if (!user->live || user->to_remove) {
      remove_button(twitch, user);
      if(user->to_remove) g_hash_table_remove(twitch->api->following, user->id);
    }
  } else if (user->live) {
    add_button(twitch, user);
  }
}

// static void
// create_grid_layout(TwitchPlugin *twitch) {
//   GHashTableIter iter;
//   gpointer key, value;
//   if (twitch->grid != NULL) {
//     gtk_container_remove(GTK_CONTAINER(twitch->ebox), twitch->grid);
//   }
//   twitch->grid = gtk_grid_new();
//   gtk_grid_attach(GTK_GRID(twitch->grid), twitch->icon, 0, 0, NROWS(twitch), NROWS(twitch));
//   g_hash_table_iter_init(&iter, twitch->buttons);
//   for(int i = 0; g_hash_table_iter_next(&iter, &key, &value); i++) {
//     gtk_grid_attach(GTK_GRID(twitch->grid), value, 
//         GET_H(twitch, i), GET_V(twitch, i), 1, 1);
//   }
//   gtk_container_add(GTK_CONTAINER(twitch->ebox), twitch->grid);
//   gtk_widget_show_all(twitch->grid);
// }

static void
update_button(gpointer key, gpointer value, gpointer userdata) {
  GtkWidget *button = value, *image;
  TwitchPlugin *twitch = userdata;
  GdkPixbuf *scaled;
  gint size;
  TwitchUser *user = g_hash_table_lookup(twitch->api->following, key);
  image = gtk_button_get_image(GTK_BUTTON(button));
  scaled = gtk_image_get_pixbuf(GTK_IMAGE(image));
  size = gdk_pixbuf_get_height(scaled);
  if (size != twitch->size) {
    update_button_img(twitch, user);
  }
}
