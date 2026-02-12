#include "./spotify-status.h"
#include "./mpris.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include <string.h>

static void update_label(GDBusProxy* proxy, GtkWidget* label)
{
  char* metadata = get_track_metadata(proxy);
  gtk_label_set_text(GTK_LABEL(label), metadata);
  g_free(metadata);
}

static void update_button_icon(GtkWidget* button, const char* icon_name)
{
  GtkWidget* new_image = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image(GTK_BUTTON(button), new_image);
}

static void on_spotify_properties_changed(GDBusProxy* proxy, GVariant* changed_properties, char** invalidated_properties, gpointer user_data)
{
  GtkWidget* window = GTK_WIDGET(user_data);
  struct WidgetData* widgets = (struct WidgetData*)g_object_get_data(G_OBJECT(window), "widget_data_instance");
  if (widgets == NULL) return;
  GVariant* metadata_variant = g_variant_lookup_value(changed_properties, "Metadata", G_VARIANT_TYPE("a{sv}"));
  GVariant* playback_variant = g_variant_lookup_value(changed_properties, "PlaybackStatus", G_VARIANT_TYPE_STRING);

  if (metadata_variant != NULL)
  {
    //metadata has changed, so update the label
    update_label(proxy, widgets->label);
  }
  if (playback_variant != NULL)
  {
    //PlaybackStatus has changed, update the button icon
    const char* playback_status = g_variant_get_string(playback_variant, NULL);
    if (!strcmp(playback_status, "Playing"))
      update_button_icon(widgets->button, "media-playback-pause");
    else
      update_button_icon(widgets->button, "media-playback-start");
  }

  if (metadata_variant != NULL) g_variant_unref(metadata_variant);
  if (playback_variant != NULL) g_variant_unref(playback_variant);
}

static void on_button_click(GtkWidget *widget, gpointer user_data)
{
  struct DbusData* dbus_data = (struct DbusData*)user_data;
  send_dbus_message(dbus_data->proxy, dbus_data->method);
}

static gboolean on_focus_out(GtkWidget* window, GdkEventFocus* event, gpointer user_data)
{
  gtk_widget_destroy(window);
  //return false after destroying the window, let gtk handle the rest
  return 0; 
}

static GtkStatusIcon* create_tray_icon()
{
  GtkStatusIcon* systray_icon = gtk_status_icon_new_from_file("/usr/share/icons/Papirus/22x22/panel/spotify-indicator.svg");
  gtk_status_icon_set_name(systray_icon, "spotify-status");
  gtk_status_icon_set_title(systray_icon, "Spotify Status");
  gtk_status_icon_set_visible(systray_icon, 1);
  gtk_status_icon_set_has_tooltip(systray_icon, 1);
  gtk_status_icon_set_tooltip_text(systray_icon, "Spotify Status");

  return systray_icon;
}

static gboolean create_main_window(GtkWidget* systray_icon, GdkEventButton *event, gpointer user_data)
{
  GDBusProxy* proxy = (GDBusProxy*)user_data;
  struct ButtonData* button_data = malloc(sizeof(struct ButtonData));
  struct DbusData* dbus_data = malloc(sizeof(struct DbusData)*3);
  struct WidgetData* widget_data = malloc(sizeof(struct WidgetData));

  dbus_data[0].method = PLAYER_METHOD_PREVIOUS;
  dbus_data[1].method = PLAYER_METHOD_PLAYPAUSE;
  dbus_data[2].method = PLAYER_METHOD_NEXT;

  for (int i = 0; i < 3; i++)
  {
    dbus_data[i].proxy = proxy;
  }

  //used to set the specific position of the window, shouldn't be hardcoded
  gint xpos, ypos, xoffset, yoffset;
  xoffset = 1800;
  yoffset = 40;
  GtkWidget* main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  // g_object_set_data_full assigns a data structure to a G_OBJECT. When the window is destroyed, memory is release with free
  g_object_set_data_full(G_OBJECT(main_window), "button_data_instance", button_data, free);
  g_object_set_data_full(G_OBJECT(main_window), "dbus_data_instance", dbus_data, free);
  g_object_set_data_full(G_OBJECT(main_window), "widget_data_instance", widget_data, free);

  //window configuration; window type is GTK_WINDOW_TOPLEVEL, but we have to make it look like a popup menu
  gtk_window_set_title(GTK_WINDOW(main_window), "spotify-status");
  gtk_window_set_decorated(GTK_WINDOW(main_window), 0);
  gtk_window_set_resizable(GTK_WINDOW(main_window), 0);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(main_window),1);
  //GTK_WIN_POS_NONE means top-left of the screen, then we apply the offset to get the desired position
  gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_NONE);
  gtk_window_get_position(GTK_WINDOW(main_window), &xpos, &ypos);
  gtk_window_move(GTK_WINDOW(main_window), xpos+xoffset, ypos+yoffset);

  button_data->icon_size = GTK_ICON_SIZE_BUTTON;
  //create grid
  GtkWidget* main_grid = gtk_grid_new();
  gtk_grid_insert_row(GTK_GRID(main_grid), 0);
  gtk_grid_insert_row(GTK_GRID(main_grid), 1);
  gtk_grid_insert_column(GTK_GRID(main_grid), 0);
  gtk_grid_insert_column(GTK_GRID(main_grid), 1);
  gtk_grid_insert_column(GTK_GRID(main_grid), 2);

  char* track_metadata = get_track_metadata(proxy);
  GtkWidget* label = gtk_label_new (track_metadata);
  GtkWidget* next_button = gtk_button_new_from_icon_name("media-skip-forward", button_data->icon_size);
  
  //check playback status to initialize icon correctly
  char* playback_status = get_playback_status(proxy);
  if (playback_status == NULL)
  {
    g_printerr("Couldn't access the playback status. Assuming it's paused.");
    button_data->current_icon = "media-playback-start";
  }
  else if (!strcmp(playback_status, "Playing"))
  {
    button_data->current_icon = "media-playback-pause";
  }
  else {
    button_data->current_icon = "media-playback-start";
  }

  GtkWidget* pause_button = gtk_button_new_from_icon_name(button_data->current_icon, button_data->icon_size);
  GtkWidget* previous_button = gtk_button_new_from_icon_name( "media-skip-backward", button_data->icon_size);
  button_data->button = pause_button;

  gtk_grid_attach(GTK_GRID(main_grid), label, 0, 0, 3, 1);
  gtk_grid_attach(GTK_GRID(main_grid), previous_button, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(main_grid), pause_button, 1, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(main_grid), next_button, 2, 1, 1, 1);

  gtk_container_add(GTK_CONTAINER(main_window), main_grid);

  widget_data->button = pause_button;
  widget_data->label = label;

  //signals
  g_signal_connect(main_window, "focus-out-event", G_CALLBACK(on_focus_out), NULL);
  g_signal_connect(previous_button, "clicked", G_CALLBACK(on_button_click), &dbus_data[0]);
  g_signal_connect(pause_button, "clicked", G_CALLBACK(on_button_click), &dbus_data[1]);
  g_signal_connect(next_button, "clicked", G_CALLBACK(on_button_click), &dbus_data[2]);
  //the signal persists so long as the window exists. Once the window is destroyed, the signal disappears (although proxy still lives)
  g_signal_connect_object(proxy, "g-properties-changed", G_CALLBACK(on_spotify_properties_changed), main_window, G_CONNECT_DEFAULT);
  
  //free memory
  g_free(track_metadata);
  g_free(playback_status);

  gtk_widget_show_all (main_window);
  
  return 1;
}

static void activate (GtkApplication* app, gpointer user_data)
{
  GDBusProxy* proxy = (GDBusProxy*)user_data;
  GtkStatusIcon* systray_icon = create_tray_icon();
  g_signal_connect(systray_icon, "button-press-event", G_CALLBACK(create_main_window), proxy);
  //hold the application so it doesn't close
  g_application_hold(G_APPLICATION(app));
}

int main (int argc, char** argv)
{
  GDBusProxy* proxy = connect_to_dbus();
  if (proxy == NULL)
  {
    g_printerr("Could not access the D-Bus. Exiting...");
    return 1;
  }
  const char* application_id = "org.spotify.status";
  GtkApplication* app = gtk_application_new (application_id, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activate), proxy);
  int status = g_application_run (G_APPLICATION (app), argc, argv);

  g_object_unref(app);
  g_object_unref(proxy);
  
  return status;
}