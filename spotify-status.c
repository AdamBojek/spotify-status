#include "./spotify-status.h"
#include "./mpris.h"
#include "gdk/gdk.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtkcssprovider.h"
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <glib-unix.h>

//store a pointer to the window to avoid creating multiple instances
static GtkWidget *window_instance = NULL;

//load css from file and apply it to the default screen
static void load_css()
{
  GtkCssProvider* css_provider = gtk_css_provider_new();
  GError* error = NULL;
  GFile* css_file = g_file_new_for_path(CSS_FILE);
  gtk_css_provider_load_from_file(css_provider, css_file, &error);
  if (error)
  {
    g_printerr("Error loading style sheets: %s", error->message);
    g_error_free(error);
  } else 
  {
    GdkScreen* screen = gdk_screen_get_default();
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
  }
  if (css_file != NULL) g_object_unref(css_file);
  g_object_unref(css_provider);
}

//this user-sent unix signal is used to tell the app to hide the current window. 
//if the window is already set to hidden, it should show the window
static gboolean unix_signal_handler(gpointer user_data)
{
  if (gtk_widget_is_visible(window_instance))
      gtk_widget_hide(window_instance);
  else
      gtk_widget_show_all(window_instance);

  return 1;
}

static void on_key_press_event(GtkWidget* window, GdkEventKey* event, gpointer user_data)
{
  //one of the ways to close the window is to press escape while the window is focused
  if (event->keyval == GDK_KEY_Escape)
    gtk_widget_hide(window_instance);
  else if (event->keyval == GDK_KEY_space)
    send_dbus_message((GDBusProxy*)user_data, "PlayPause");
}

//uses the window's top-right corner as the anchor. This is useful because the size of the window is dynamic;
//this function makes it so that, if the next track has a longer title than the previous one;
//the window will be extended to the left, as opposed to the right (where the screen would end)
static void on_window_size_allocate(GtkWidget *window, GtkAllocation *allocation, gpointer user_data)
{
    //user data is x position
    gint anchor_x = GPOINTER_TO_INT(user_data);
    
    gint x_offset = 20;
    //calculate new position, if window gets bigger, resize to the left
    gint new_x = anchor_x - allocation->width + x_offset;
    
    if (new_x < 0) new_x = 0;

    gint current_x, current_y;
    gtk_window_get_position(GTK_WINDOW(window), &current_x, &current_y);

    if (current_x != new_x) {
        gtk_window_move(GTK_WINDOW(window), new_x, current_y);
    }
}

//update track metadata (title - artist) when the track changes
static void update_label(GDBusProxy* proxy, GtkWidget* label)
{
  char* metadata = get_track_metadata(proxy);
  gtk_label_set_text(GTK_LABEL(label), metadata);
  g_free(metadata);
}

//dynamically update the button icon (pause/resume)
static void update_button_icon(GtkWidget* button, const char* icon_name)
{
  GtkWidget* new_image = gtk_image_new_from_icon_name(icon_name, BUTTON_ICON_SIZE);
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

//wrapper for send_dbus_message, each button has a different method corresponing to it
static void on_button_click(GtkWidget *widget, gpointer user_data)
{
  struct DbusData* dbus_data = (struct DbusData*)user_data;
  send_dbus_message(dbus_data->proxy, dbus_data->method);
}

static GtkStatusIcon* create_tray_icon()
{
  GdkPixbuf* icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "spotify-indicator", 22, 0, NULL);
  GtkStatusIcon* systray_icon = gtk_status_icon_new_from_pixbuf(icon);
  gtk_status_icon_set_name(systray_icon, "spotify-status");
  gtk_status_icon_set_title(systray_icon, "Spotify Status");
  gtk_status_icon_set_visible(systray_icon, 1);
  gtk_status_icon_set_has_tooltip(systray_icon, 1);
  gtk_status_icon_set_tooltip_text(systray_icon, "Spotify Status");
  g_object_unref(icon);
  return systray_icon;
}

//return 1 if successfully created the window, return 0 if it already exists
static gboolean create_main_window(GtkWidget* systray_icon, GdkEventButton* event, gpointer user_data)
{
  //check if window exists
  if (window_instance)
  {
    //if its hidden, show it; if its's shown, hide it
    if (gtk_widget_is_visible(window_instance))
      gtk_widget_hide(window_instance);
    else
      gtk_widget_show_all(window_instance);
    return 0;
  }
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

  GtkWidget* main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  window_instance = main_window;

  // g_object_set_data_full assigns a data structure to a G_OBJECT. When the window is destroyed, memory is release with free
  g_object_set_data_full(G_OBJECT(main_window), "button_data_instance", button_data, free);
  g_object_set_data_full(G_OBJECT(main_window), "dbus_data_instance", dbus_data, free);
  g_object_set_data_full(G_OBJECT(main_window), "widget_data_instance", widget_data, free);

  //window configuration; window type is GTK_WINDOW_TOPLEVEL, but we have to make it look like a popup menu
  gtk_window_set_title(GTK_WINDOW(main_window), "spotify-status");
  gtk_window_set_decorated(GTK_WINDOW(main_window), 0);
  gtk_window_set_resizable(GTK_WINDOW(main_window), 0);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(main_window),1);
  //if a window is sticky it will show up on all workspaces / desktops
  //shouldnt be hardcoded, add it to the config file in the future
  gtk_window_stick(GTK_WINDOW(main_window));
  
  //add transparency support
  GdkVisual *visual = gdk_screen_get_rgba_visual(gtk_widget_get_screen(main_window));
  if (visual) {
    gtk_widget_set_visual(main_window, visual);
  }

  button_data->icon_size = BUTTON_ICON_SIZE;
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

  //get mouse position when clicked
  gint mouse_x = (gint)event->x_root;
  gint mouse_y = (gint)event->y_root;

  //vertical offset
  gint start_y = mouse_y + 20;
  gtk_window_move(GTK_WINDOW(main_window), mouse_x, start_y);

  //whenever the window is resized (because the track names can be longer or shorter and the size of the window is not static)
  //call on_window_size_allocate which should make sure the window stays anchored to a specific position
  g_signal_connect(main_window, "size-allocate", G_CALLBACK(on_window_size_allocate), GINT_TO_POINTER(mouse_x));

  widget_data->button = pause_button;
  widget_data->label = label;

  //assign names to widgets to make them easy to formt in style.css
  gtk_widget_set_name(main_window, "spotify-status-window");
  gtk_widget_set_name(previous_button, "spotify-status-previous-button");
  gtk_widget_set_name(pause_button, "spotify-status-pause-button");
  gtk_widget_set_name(next_button, "spotify-status-next-button");
  gtk_widget_set_name(label, "spotify-status-label");
  gtk_widget_set_name(main_grid, "spotify-status-grid");

  //signals
  g_signal_connect(main_window, "key-press-event", G_CALLBACK(on_key_press_event), proxy);
  g_signal_connect(previous_button, "clicked", G_CALLBACK(on_button_click), &dbus_data[0]);
  g_signal_connect(pause_button, "clicked", G_CALLBACK(on_button_click), &dbus_data[1]);
  g_signal_connect(next_button, "clicked", G_CALLBACK(on_button_click), &dbus_data[2]);
  //the signal persists so long as the window exists. Once the window is destroyed, the signal disappears (although proxy still lives)
  g_signal_connect_object(proxy, "g-properties-changed", G_CALLBACK(on_spotify_properties_changed), main_window, G_CONNECT_DEFAULT);
  //connect custom user-sent unix signal
  g_unix_signal_add(SIGUSR1, G_SOURCE_FUNC(unix_signal_handler), main_window);
  g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_widget_destroyed), &window_instance);

  //free memory
  g_free(track_metadata);
  g_free(playback_status);

  gtk_widget_show_all(main_window);
  
  return 1;
}

//called when the application starts
static void activate (GtkApplication* app, gpointer user_data)
{
  GDBusProxy* proxy = (GDBusProxy*)user_data;
  GtkStatusIcon* systray_icon = create_tray_icon();
  load_css();
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