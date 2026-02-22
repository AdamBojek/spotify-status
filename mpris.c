#include "./mpris.h"
#include "gio/gio.h"
#include "glib.h"
#include "glibconfig.h"
#include <stdio.h>

void seek_to_position(GDBusProxy* proxy, gdouble position_seconds)
{
    //we use method SetPosition which takes arguments: "o" - TrackId, "x" - Position
    //first, get TrackId
    char* track_id = get_track_id(proxy);
    if (track_id == NULL)
    {
        g_printerr("Error getting track ID\n");
        return;
    }
    GError* error = NULL;
    GVariant* result = g_dbus_proxy_call_sync(proxy, "SetPosition", g_variant_new("(ox)", track_id, (gint64)(position_seconds * 1000000.0)), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (error)
    {
        g_printerr("Error seeking to position: %s\n", error->message);
        g_error_free(error);
    }
    if (result != NULL) g_variant_unref(result);
    if (track_id != NULL) g_free(track_id);
}

char* get_track_id(GDBusProxy* proxy)
{
    GVariant* metadata = g_dbus_proxy_get_cached_property(proxy, "Metadata");
    if (metadata == NULL) return NULL;
    
    GVariant* track_id_variant = g_variant_lookup_value(metadata, "mpris:trackid", NULL);
    if (track_id_variant == NULL)
    {
        g_variant_unref(metadata);
        return NULL;
    }
    char* track_id = g_variant_dup_string(track_id_variant, NULL);
    g_variant_unref(track_id_variant);
    g_variant_unref(metadata);
    return track_id;
}

//voodoo magic
gdouble get_current_position(GDBusProxy* proxy)
{
    GError *error = NULL;
    //get position (variant)
    GVariant *result = g_dbus_proxy_call_sync(proxy,
                                             "org.freedesktop.DBus.Properties.Get",
                                             g_variant_new("(ss)", PLAYER_INTERFACE, "Position"),
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1, NULL, &error);

    if (error) {
        g_printerr("Error getting position: %s\n", error->message);
        g_error_free(error);
        return 0;
    }

    GVariant *inner;
    g_variant_get(result, "(v)", &inner);
    
    gint64 position = g_variant_get_int64(inner);
    
    g_variant_unref(inner);
    g_variant_unref(result);

    return (gdouble)(position / 1000000.0);
}

gdouble get_track_length(GDBusProxy* proxy)
{
    GVariant* metadata_variant = g_dbus_proxy_get_cached_property(proxy, "Metadata");
    if (metadata_variant == NULL)
        return 0;
    GVariant* track_length_variant = g_variant_lookup_value(metadata_variant, "mpris:length", G_VARIANT_TYPE_UINT64);
    if (track_length_variant == NULL)
    {
        g_variant_unref(metadata_variant);
        return 0;
    }
    gint64 track_length = g_variant_get_uint64(track_length_variant);
    g_variant_unref(track_length_variant);
    g_variant_unref(metadata_variant);
    return (gdouble)(track_length / 1000000.0); //convert microseconds to seconds
}

GDBusProxy* connect_to_dbus()
{
    GError* error = NULL;
    GDBusProxy* proxy = g_dbus_proxy_new_for_bus_sync   (G_BUS_TYPE_SESSION,
                                                        G_DBUS_PROXY_FLAGS_NONE, 
                                                        NULL,
                                                        SPOTIFY_DBUS_NAME, 
                                                        MEDIA_PLAYER_PATH, 
                                                        PLAYER_INTERFACE, 
                                                        NULL, 
                                                        &error);

    if (proxy == NULL)
    {
      printf("Error creating proxy: %s\n", error->message);
      g_error_free (error);
      return NULL;
    }

    return proxy;
}

gboolean send_dbus_message(GDBusProxy *proxy, const char *method)
{
    GError* error = NULL;
    GVariant* result =g_dbus_proxy_call_sync(proxy, method, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    
    if (error) {
        printf("Error sending DBus message: %s\n", error->message);
        g_error_free(error);
        if (result != NULL) g_variant_unref(result);
        return 0;
    }
    if (result != NULL) g_variant_unref(result);
    return 1;
}

//caller is responsible for freeing the string
char* get_playback_status(GDBusProxy* proxy)
{
    GVariant* playback_status_value = g_dbus_proxy_get_cached_property(proxy, "PlaybackStatus");
    if (playback_status_value == NULL)
        return NULL;
    const char* playback_status = g_variant_get_string(playback_status_value, NULL);
    char* playback_status_copy = g_strdup(playback_status);
    g_variant_unref(playback_status_value);

    return playback_status_copy;
}

char* get_track_metadata(GDBusProxy* proxy)
{
    const char* title; //title does not need to be freed, it's just a pointer to the string in the variant
    char* artist; //artist needs to be freed, because we dupe it
    GVariant* metadata = g_dbus_proxy_get_cached_property(proxy, "Metadata");
    if (metadata == NULL)
    {
        //caller is responsible for freeing the memory, so we have to dupe it
        return g_strdup("Unknown title - Unknown artist");
    }
    //get title
    GVariant* title_variant = g_variant_lookup_value(metadata, "xesam:title", G_VARIANT_TYPE_STRING);
    if (title_variant == NULL)
    {
        title = "Unknown title";
    } else
    {
        title = g_variant_get_string(title_variant, NULL);
    }
    //get artist
    GVariant* artist_variant = g_variant_lookup_value(metadata, "xesam:artist", G_VARIANT_TYPE_STRING_ARRAY);
    if (artist_variant == NULL || g_variant_n_children(artist_variant) == 0)
    {
        artist = g_strdup("Unknown artist");
    } else
    {
        //get just the first artist
        GVariant* first_artist = g_variant_get_child_value(artist_variant, 0);
        //dupe the string so it persists after the variant is unreffed
        artist = g_strdup(g_variant_get_string(first_artist, NULL));
        g_variant_unref(first_artist);
    }

    char* result =  g_strconcat(title, " - ", artist, NULL);
    if (artist_variant != NULL) g_variant_unref(artist_variant);
    if (metadata != NULL) g_variant_unref(metadata);
    if (title_variant != NULL) g_variant_unref(title_variant);
    g_free(artist);
    return result;
}