#include <json-c/json.h>
#include <curl/curl.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "twitch-api.h"

#define TWITCH_API_FOLLOWING "https://api.twitch.tv/helix/users/follows"
#define TWITCH_API_USERS "https://api.twitch.tv/helix/users"
#define TWITCH_API_STREAMS "https://api.twitch.tv/helix/streams"
#define TWITCH_PFP_DEFAULT "https://static-cdn.jtvnw.net/jtv_user_pictures/xarth/404_user_70x70.png"
typedef struct {
    char *data;
    size_t size;
} api_result;

typedef struct {
    gchar **offset;
    const gchar *prefix;
} id_loop;

static size_t curl_callback (void *contents, size_t size, size_t num, void *user) {
    size_t fullsize = size*num;
    api_result *res = user;
    res->data = realloc(res->data, res->size + fullsize + 1);
    if (res->data == NULL) {
        return 1;
    }

    memcpy(&(res->data[res->size]), contents, fullsize);

    res->size += fullsize;
    res->data[res->size] = 0;
    
    return fullsize;
}

static json_object* curl_fetch_json (TwitchApi *api, const gchar *url) {
    api_result res;
    gchar *data;
    json_object *obj;
    struct curl_slist *headers = NULL;
    res.data = calloc(1, sizeof(res.data));
    if(res.data == NULL) return NULL;
    res.size = 0;
    curl_easy_setopt(api->curl, CURLOPT_URL, url);
    curl_easy_setopt(api->curl, CURLOPT_WRITEFUNCTION, curl_callback);
    curl_easy_setopt(api->curl, CURLOPT_WRITEDATA, &res);
    curl_easy_setopt(api->curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    if(api->access_token[0] != '\0') {
        data = g_strconcat("Authorization: Bearer ", api->access_token, NULL);
    } else {
        data = g_strconcat("Client-ID: ", api->client_id, NULL);
    }
    headers = curl_slist_append(headers, data);
    curl_easy_setopt(api->curl, CURLOPT_HTTPHEADER, headers);
    if (curl_easy_perform(api->curl) != CURLE_OK) return NULL;
    obj = json_tokener_parse(res.data);
    free(res.data);
    g_free(data);
    return obj;
}
static size_t img_read_buf(void *data, size_t size, size_t num, void* user) {
    GdkPixbufLoader *loader = user;
    gdk_pixbuf_loader_write(loader, (guchar*)data, size * num, NULL);
    return size * num;
}
static GdkPixbuf* curl_read_image(TwitchApi* api, gchar *url) {
    GdkPixbufLoader *loader;
    GdkPixbuf *pixbuf;
    loader = gdk_pixbuf_loader_new();
    curl_easy_setopt(api->curl, CURLOPT_URL, url);
    curl_easy_setopt(api->curl, CURLOPT_WRITEFUNCTION, img_read_buf);
    curl_easy_setopt(api->curl, CURLOPT_WRITEDATA, loader);
    curl_easy_setopt(api->curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    if (curl_easy_perform(api->curl) != CURLE_OK) return NULL;
    gdk_pixbuf_loader_close(loader, NULL);
    pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    g_object_ref(G_OBJECT(pixbuf));
    g_object_unref(G_OBJECT(loader));
    return pixbuf;
}

//assumes user->name exists
static gboolean twitch_init_user_id (TwitchApi* api) {
    json_object *data, *id, *user, *json;
    gchar *url;
    const gchar *id_c;
    url = g_strconcat(TWITCH_API_USERS, "?login=", api->user.name, NULL);
    json = curl_fetch_json(api, url);
    if (json == NULL) return FALSE;
    json_object_object_get_ex(json, "data", &data);
    user = json_object_array_get_idx(data, 0);
    json_object_object_get_ex(user, "id", &id);
    id_c = json_object_get_string(id);
    api->user.id = g_strdup(id_c);
    json_object_put(json);
    g_free(url);
    return TRUE;
}
static gboolean twitch_update_following (TwitchApi* api) {
    json_object *total, *data, *to_id, *to_name, *json, *user, *pagination, *cursor = NULL;
    TwitchUser *tw_user;
    gsize count;
    gchar *url, *url_base = g_strconcat(TWITCH_API_FOLLOWING, "?from_id=", api->user.id, "&first=100", NULL);
    g_hash_table_remove_all(api->following);
    api->follow_size = 0;
    do {
        if(cursor) {
            url = g_strconcat(url_base, "&after=", json_object_get_string(cursor), NULL);
        } else url = g_strdup(url_base);
        json = curl_fetch_json(api, url);
        if (json == NULL) return FALSE;
        json_object_object_get_ex(json, "total", &total);
        json_object_object_get_ex(json, "data", &data);
        json_object_object_get_ex(json, "pagination", &pagination);
        json_object_object_get_ex(pagination, "cursor", &cursor);
        count = json_object_get_int(total);
        api->follow_size += json_object_get_int(total);
        for (gsize i = 0; i < count; i++) {
            user = json_object_array_get_idx(data, i);
            json_object_object_get_ex(user, "to_id", &to_id);
            json_object_object_get_ex(user, "to_name", &to_name);
            tw_user = g_new0(TwitchUser, 1);
            tw_user->name = g_strdup(json_object_get_string(to_name));
            tw_user->id = g_strdup(json_object_get_string(to_id));
            g_hash_table_insert(api->following, tw_user->id, tw_user);
        }
        g_free(url);
        json_object_put(json);
    } while(count == 100);
    g_free(url_base);
    return TRUE;
}
static void key_add(gpointer key, gpointer value, gpointer user_data) {
    gsize *size = (gsize*)user_data;
    (*size) += strlen((gchar*)key);
}
static void copy_ids(gpointer key, gpointer value, gpointer user_data) {
    id_loop *loop = (id_loop*)user_data;
    gchar** offset = loop->offset;
    *(offset) = g_stpcpy(*offset, loop->prefix);
    *(offset) = g_stpcpy(*offset, key);
}

static gboolean twitch_update_user_data(TwitchApi *api) {
    json_object *json, *data, *id, *pfp, *login;
    id_loop loop;
    TwitchUser *user;
    gchar *url, *offset, *url_final;
    const gchar *pfp_temp, *prefix = "&id=";
    gsize size_total;
    url = g_strdup_printf("%s%s%lu", TWITCH_API_USERS, "?first=", api->follow_size);
    size_total = strlen(url) + strlen(prefix) * api->follow_size;
    g_hash_table_foreach(api->following, key_add, &size_total);
    size_total += 1;
    url_final = g_slice_alloc0(size_total);
    offset = g_stpcpy(url_final, url);
    loop.offset = &offset;
    loop.prefix = prefix;
    g_hash_table_foreach(api->following, copy_ids, &loop);
    json = curl_fetch_json(api, url_final);
    if(!json || !json_object_object_get_ex(json, "data", &data)) return FALSE;
    for (gsize i = 0; i < json_object_array_length(data); i++) {
        json_object_object_get_ex(json_object_array_get_idx(data, i), "profile_image_url", &pfp);
        json_object_object_get_ex(json_object_array_get_idx(data, i), "id", &id);
        json_object_object_get_ex(json_object_array_get_idx(data, i), "login", &login);

        user = (TwitchUser*)g_hash_table_lookup(api->following, json_object_get_string(id));
        if (g_strcmp0(user->url, json_object_get_string(login)) != 0) {
            if(user->url) g_free(user->url);
            user->url = g_strdup(json_object_get_string(login));
        }

        pfp_temp = json_object_get_string(pfp);
        if (g_strcmp0(user->pfp_url, pfp_temp) != 0) {
            if (user->pfp_url) g_free(user->pfp_url);
            if (user->pfp) g_object_unref(G_OBJECT(user->pfp));
            user->pfp_url = g_strdup(pfp_temp);
            user->pfp = curl_read_image(api, user->pfp_url[0]? user->pfp_url : TWITCH_PFP_DEFAULT);
            user->update_pfp = TRUE;
        }
    }
    g_free(url);
    g_slice_free1(size_total, url_final);
    json_object_put(json);
    return TRUE;
}

static void set_false (gpointer key, gpointer value, gpointer userdata){
    ((TwitchUser*)value)->live = FALSE;
}
static gboolean init_complete(TwitchApi *api) {
    return api->init_complete
        || (api->init_complete = twitch_init(api));
}

gboolean twitch_update_users(TwitchApi *api) {
    return init_complete(api)
        && twitch_update_following(api)
        && api->follow_size > 0
        && twitch_update_user_data(api);
}

gboolean twitch_update_status (TwitchApi *api) {
    gchar *offset, *url, *url_final;
    json_object *data, *type, *id, *json, *title;
    json_object *error, *message;
    id_loop loop;
    TwitchUser *user;
    gsize size_total;
    const char* prefix = "&user_id=";
    if (!init_complete(api) || api->follow_size == 0) return FALSE;
    url = g_strdup_printf("%s%s%lu", TWITCH_API_STREAMS, "?first=", api->follow_size);
    size_total = strlen(url) + strlen(prefix) * api->follow_size;
    g_hash_table_foreach(api->following, key_add, &size_total);
    size_total +=  1;
    url_final = g_slice_alloc0(size_total);
    offset = g_stpcpy(url_final, url);
    loop.offset = &offset;
    loop.prefix = prefix;
    g_hash_table_foreach(api->following, copy_ids, &loop);
    g_hash_table_foreach(api->following, set_false, NULL);
    json = curl_fetch_json(api, url_final);
    if (!json) return FALSE;
    if(!json_object_object_get_ex(json, "data", &data)) {
        json_object_object_get_ex(json, "error", &error);
        json_object_object_get_ex(json, "message", &message);
        g_error("Twitch API Error: %s", json_object_get_string(error));
        g_error("%s", json_object_get_string(message));
        return FALSE;
    }
    for (gsize i = 0; i < json_object_array_length(data); i++) {
        json_object_object_get_ex(json_object_array_get_idx(data, i), "user_id", &id);
        json_object_object_get_ex(json_object_array_get_idx(data, i), "type", &type);
        json_object_object_get_ex(json_object_array_get_idx(data, i), "title", &title);
        user = (TwitchUser*)g_hash_table_lookup(api->following, json_object_get_string(id));
        user->live = g_str_equal(json_object_get_string(type), "live");
        if (user->live) {
            // g_message("%s %s", user->name, json_object_get_string(type));
            if (g_strcmp0(user->status, json_object_get_string(title)) != 0) {
                if (user->status) g_free(user->status);
                user->status = g_strdup(json_object_get_string(title));
            }
        }
    }
    json_object_put(json);
    g_free(url);
    g_slice_free1(size_total, url_final);
    return TRUE;
}

void twitch_free_user(gpointer data) {
    TwitchUser *user = data;
    if (user->pfp_url)
        g_message("%s\n", user->pfp_url);
        g_free(user->pfp_url);
    
    if (user->pfp)
        g_object_unref(G_OBJECT(user->pfp));

    if (user->status)
        g_free(user->status);

    g_free (user->id);
    g_free (user->name);
}

gboolean twitch_init(TwitchApi *api) {
    if(!api->curl) api->curl = curl_easy_init();
    if(!api->following) api->following = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, twitch_free_user);
    return TWITCH_CAN_INIT(api)
        && twitch_init_user_id(api)
        && (api->init_complete = TRUE)
        && twitch_update_users(api);
}


void twitch_free(TwitchApi *api) {
    if (api->client_id)
        g_free(api->client_id);

    curl_easy_cleanup(api->curl);
    g_slice_free(TwitchApi, api);
    g_object_unref(G_OBJECT(api->following));
    twitch_free_user(&api->user);
}