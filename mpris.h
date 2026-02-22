#pragma once
#ifndef MPRIS_H
#define MPRIS_H

#include <gio/gio.h>

#define SPOTIFY_DBUS_NAME "org.mpris.MediaPlayer2.spotify"
#define MEDIA_PLAYER_PATH "/org/mpris/MediaPlayer2"
#define PLAYER_METHOD_PLAYPAUSE "PlayPause"
#define PLAYER_METHOD_NEXT "Next"
#define PLAYER_METHOD_PREVIOUS "Previous"
#define PLAYER_INTERFACE "org.mpris.MediaPlayer2.Player"
#define CSS_FILE "./style.css"

//definitions
gdouble get_current_position(GDBusProxy* proxy);
gdouble get_track_length(GDBusProxy* proxy);
GDBusProxy* connect_to_dbus();
char* get_track_metadata(GDBusProxy* proxy);
gboolean send_dbus_message(GDBusProxy* proxy, const char* method);
char* get_playback_status(GDBusProxy* proxy);

#endif