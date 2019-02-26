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
#define FOLLOW_SIZE api->follow_size
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
    data = g_strconcat("Client-ID: ", api->client_id, NULL);
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
static int twitch_init_user_id (TwitchApi* api) {
    json_object *data, *id, *user, *json;
    gchar *url;
    const gchar *id_c;
    url = g_strconcat(TWITCH_API_USERS, "?login=", api->user.name, NULL);
    json = curl_fetch_json(api, url);
    if (json == NULL) return 1;
    json_object_object_get_ex(json, "data", &data);
    user = json_object_array_get_idx(data, 0);
    json_object_object_get_ex(user, "id", &id);
    id_c = json_object_get_string(id);
    api->user.id = g_strdup(id_c);
    json_object_put(json);
    g_free(url);
    return 0;
}
static int twitch_init_user_following (TwitchApi* api) {
    json_object *total, *data, *to_id, *to_name, *json, *user;
    TwitchUser *tw_user;
    gchar *url = g_strconcat(TWITCH_API_FOLLOWING, "?from_id=", api->user.id, "&first=100", NULL);
    json = curl_fetch_json(api, url);
    if (json == NULL) return 1;
    json_object_object_get_ex(json, "total", &total);
    json_object_object_get_ex(json, "data", &data);
    api->follow_size = json_object_get_int(total);
    //TODO: >100
    for (gsize i = 0; i < FOLLOW_SIZE; i++) {
        user = json_object_array_get_idx(data, i);
        json_object_object_get_ex(user, "to_id", &to_id);
        json_object_object_get_ex(user, "to_name", &to_name);
        tw_user = g_new0(TwitchUser, 1);
        tw_user->name = g_strdup(json_object_get_string(to_name));
        tw_user->id = g_strdup(json_object_get_string(to_id));
        g_hash_table_insert(api->following, tw_user->id, tw_user);
    }
    json_object_put(json);
    g_free(url);
    return 0;
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

static void twitch_update_pfp(TwitchApi *api) {
    json_object *data, *id, *pfp, *json;
    id_loop loop;
    TwitchUser *user;
    gchar *url, *offset, *url_final;
    const gchar *pfp_temp, *prefix = "&id=";
    gsize size_total;
    url = g_strdup_printf("%s%s%lu", TWITCH_API_USERS, "?first=", FOLLOW_SIZE);
    size_total = strlen(url) + strlen(prefix) * FOLLOW_SIZE;
    g_hash_table_foreach(api->following, key_add, &size_total);
    size_total +=  1;
    url_final = g_slice_alloc0(size_total);
    offset = g_stpcpy(url_final, url);
    loop.offset = &offset;
    loop.prefix = prefix;
    g_hash_table_foreach(api->following, copy_ids, &loop);
    json = curl_fetch_json(api, url_final);
    json_object_object_get_ex(json, "data", &data);
    for (gsize i = 0; i < json_object_array_length(data); i++) {
        json_object_object_get_ex(json_object_array_get_idx(data, i), "profile_image_url", &pfp);
        json_object_object_get_ex(json_object_array_get_idx(data, i), "id", &id);
        pfp_temp = json_object_get_string(pfp);
        user = (TwitchUser*)g_hash_table_lookup(api->following, json_object_get_string(id));
        if (g_strcmp0(pfp_temp, user->pfp_url) != 0) {
            if (user->pfp_url != NULL) {
                g_free(user->pfp_url);
                user->update_pfp = TRUE;
            }
            user->pfp_url = g_strdup(pfp_temp);
        }
    }
    g_free(url);
    g_slice_free1(size_total, url_final);
    json_object_put(json);
}

static void set_false (gpointer key, gpointer value, gpointer userdata){
    ((TwitchUser*)value)->live = FALSE;
}
void twitch_update_status (TwitchApi *api) {
    gchar *offset, *url, *url_final;
    json_object *data, *type, *id, *json, *title;
    id_loop loop;
    TwitchUser *user;
    gsize size_total;
    const char* prefix = "&user_id=";
    if (api->follow_size == 0) return;
    url = g_strdup_printf("%s%s%lu", TWITCH_API_STREAMS, "?first=", FOLLOW_SIZE);
    size_total = strlen(url) + strlen(prefix) * FOLLOW_SIZE;
    g_hash_table_foreach(api->following, key_add, &size_total);
    size_total +=  1;
    url_final = g_slice_alloc0(size_total);
    offset = g_stpcpy(url_final, url);
    loop.offset = &offset;
    loop.prefix = prefix;
    g_hash_table_foreach(api->following, copy_ids, &loop);
    g_hash_table_foreach(api->following, set_false, NULL);
    json = curl_fetch_json(api, url_final);
    if (json == NULL) return;
    json_object_object_get_ex(json, "data", &data);
    for (gsize i = 0; i < json_object_array_length(data); i++) {
        json_object_object_get_ex(json_object_array_get_idx(data, i), "user_id", &id);
        json_object_object_get_ex(json_object_array_get_idx(data, i), "type", &type);
        json_object_object_get_ex(json_object_array_get_idx(data, i), "title", &title);
        user = (TwitchUser*)g_hash_table_lookup(api->following, json_object_get_string(id));
        user->live = g_str_equal(json_object_get_string(type), "live");
        if (user->live) {
            g_message("%s %s", user->name, json_object_get_string(type));
            if (user->pfp == NULL || user->update_pfp)
                user->pfp = curl_read_image(api, user->pfp_url);
            if (!(g_strcmp0(user->status, json_object_get_string(title)) == 0)) {
                if (user->status != NULL) g_free(user->status);
                user->status = g_strdup(json_object_get_string(title));
            }
        }
    }
    json_object_put(json);
    g_free(url);
    g_slice_free1(size_total, url_final);
}

void twitch_free_user(gpointer data) {
    TwitchUser *user = data;
    if (G_LIKELY(user->pfp_url != NULL))
        g_free(user->pfp_url);
    
    if (user->pfp != NULL)
        g_object_unref(G_OBJECT(user->pfp));

    if (user->status != NULL)
        g_free(user->status);

    g_free (user->id);
    g_free (user->name);
}

int twitch_init(TwitchApi *api) {
    api->curl = curl_easy_init();
    return twitch_load_user(api);
}

int twitch_load_user (TwitchApi *api) {
    int err = 0;
    if (TWITCH_CAN_INIT(api)) {
        api->following = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, twitch_free_user);
        err |= twitch_init_user_id(api);
        err |= twitch_init_user_following(api);
        if (api->follow_size != 0)
            twitch_update_pfp(api);
        else err = 1;
    } else err = 1;
    return err;
}


void twitch_free(TwitchApi *api) {
    if (G_LIKELY (api->client_id != NULL))
        g_free(api->client_id);

    curl_easy_cleanup(api->curl);
    g_slice_free(TwitchApi, api);
    g_object_unref(G_OBJECT(api->following));
    twitch_free_user(&api->user);
}