#ifndef __TWITCH_API_H__
#define __TWITCH_API_H__
#include <glib.h>
#include <curl/curl.h>

#define TWITCH_CAN_INIT(api) (api->curl && api->user.name != NULL && api->client_id != NULL \
            && strlen(api->user.name) != 0 && strlen(api->client_id) != 0)

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
    //TODO: maybe add GtkButton here
} TwitchUser;

typedef struct {
    CURL *curl;
    gchar *client_id;
    gchar *access_token;
    TwitchUser user;
    GHashTable *following;
    gsize follow_size;
} TwitchApi;

int
twitch_init (TwitchApi *api);

void
twitch_update_status (TwitchApi *api);

int 
twitch_load_user (TwitchApi *api);

void
twitch_free_user (gpointer data);

void
twitch_free (TwitchApi *api);

G_END_DECLS
#endif