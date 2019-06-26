#ifndef __TWITCH_API_H__
#define __TWITCH_API_H__
#include <glib.h>
#include <curl/curl.h>

#define TWITCH_CAN_RUN(api) (api->curl && api->user.name && (api->client_id || api->access_token) \
            && api->user.name[0] && (api->client_id[0] || api->access_token[0]))

G_BEGIN_DECLS
typedef struct {
    gchar       *name;
    gchar       *url;
    gchar       *id;
    gchar       *pfp_url;
    gchar       *status;
    GdkPixbuf   *pfp;
    gboolean    live;
    gboolean    update_pfp;
    gboolean    to_remove;
} TwitchUser;

typedef struct {
    CURL *curl;
    gboolean init_complete;
    gchar *client_id;
    gchar *access_token;
    TwitchUser user;
    GHashTable *following;
    gsize follow_size;
} TwitchApi;

gboolean
twitch_init (TwitchApi *api);

gboolean
twitch_update_status (TwitchApi *api);

gboolean
twitch_update_users (TwitchApi *api);

gboolean 
twitch_load_user (TwitchApi *api);

void
twitch_free_user (gpointer data);

void
twitch_free (TwitchApi *api);

G_END_DECLS
#endif