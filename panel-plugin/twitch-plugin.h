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

#ifndef __TWITCH_H__
#define __TWITCH_H__

#define TWITCH_PURPLE "#6441a4"
#include "twitch-api.h"
G_BEGIN_DECLS

typedef struct {
    GtkWidget   *username;
    GtkWidget   *client_id;
    GtkWidget   *access_token;
    GtkWidget   *color_picker;
    GtkWidget   *update_users;
    GtkWidget   *update_status;
} DialogSettings;
/* plugin structure */
typedef struct
{
    XfcePanelPlugin *plugin;

    /* panel widgets */
    GtkWidget       *ebox;
    // GtkWidget       *grid;
    GtkWidget       *hvbox;
    GtkWidget       *icon;
    GHashTable      *buttons;
    int size;

    guint           update_status_id;
    guint           update_users_id;
    guint           update_status_rate;
    guint           update_users_rate;
    
    GtkCssProvider  *provider;
    GdkRGBA         color;

    TwitchApi       *api;
    DialogSettings  *settings;
}
TwitchPlugin;



void
twitch_plugin_save (XfcePanelPlugin *plugin,
             TwitchPlugin    *twitch);

gboolean
twitch_plugin_update_status (gpointer userdata);

gboolean
twitch_plugin_update_users (gpointer userdata);

void
twitch_plugin_apply_settings (TwitchPlugin *twitch);

G_END_DECLS

#endif /* !__TWITCH_H__ */
