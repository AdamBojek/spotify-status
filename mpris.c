#include "./mpris.h"
#include "gio/gio.h"
#include "glib.h"
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
    if (!proxy) return 0;
    GError* error = NULL;
    g_dbus_proxy_call(proxy, method, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, &error);
    
    if (error) {
        printf("Error sending DBus message: %s\n", error->message);
        g_error_free(error);
        return 0;
    }
    return 1;
}