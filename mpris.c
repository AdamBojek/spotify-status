#include "./mpris.h"
#include "gio/gio.h"
#include "glib.h"
#include "glibconfig.h"
#include <stdio.h>

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
    g_dbus_proxy_call(proxy, method, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, &error);
    
    if (error) {
        printf("Error sending DBus message: %s\n", error->message);
        g_error_free(error);
        return 0;
    }
    return 1;
}

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
    const char* title;
    const char* artist;
    GVariant* metadata = g_dbus_proxy_get_cached_property(proxy, "Metadata");
    //get title
    GVariant* title_variant = g_variant_lookup_value(metadata, "xesam:title", G_VARIANT_TYPE_STRING);
    title = g_variant_get_string(title_variant, NULL);
    if (title == NULL)
        title = "Unknown title";
    //get artist
    GVariant* artist_variant = g_variant_lookup_value(metadata, "xesam:artist", G_VARIANT_TYPE_STRING_ARRAY);
    if (artist_variant == NULL)
    {
        artist = "Unknown artist";
    } else
    {
        //get just the first artist
        GVariant* first_artist = g_variant_get_child_value(artist_variant, 0);
        artist = g_variant_get_string(first_artist, NULL);
        g_variant_unref(first_artist);
    }

    char* result =  g_strconcat(title, " - ", artist, NULL);
    g_variant_unref(artist_variant);
    g_variant_unref(metadata);
    return result;
}