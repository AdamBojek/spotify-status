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
#include "config.h"
#include "progressbar.h"

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
    g_printerr("Error loading style sheets: %s \n", error->message);
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
  g_print("Intercepted SIGUSR1\n");
  if (gtk_widget_is_visible(window_instance))
      gtk_widget_hide(window_instance);
  else
      gtk_widget_show_all(window_instance);

  return G_SOURCE_CONTINUE;
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
    struct AnchorPoint* anchor = (struct AnchorPoint*)user_data;
    
    //calculate new position, if window gets bigger, resize to the left
    gint new_x = anchor->x - allocation->width;
    
    if (new_x < 0) new_x = 0;

    if (anchor->x != new_x) {
        gtk_window_move(GTK_WINDOW(window), new_x, anchor->y);
    }
}

//update track metadata (title - artist) when the track changes
static void update_label(GDBusProxy* proxy, GtkWidget* label)
{
  char* metadata = get_track_metadata(proxy);
  gtk_label_set_text(GTK_LABEL(label), metadata);
  gtk_window_resize(GTK_WINDOW(window_instance), 1, 1);
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
  //voodoo magic
  GtkWidget* window = GTK_WIDGET(user_data);
  struct WidgetData* widgets = (struct WidgetData*)g_object_get_data(G_OBJECT(window), "widget_data_instance");
  struct ProgressbarData* progressbar_data = (struct ProgressbarData*)g_object_get_data(G_OBJECT(window), "progressbar_data_instance");
  if (widgets == NULL) return;
  GVariant* metadata_variant = g_variant_lookup_value(changed_properties, "Metadata", G_VARIANT_TYPE("a{sv}"));
  GVariant* playback_variant = g_variant_lookup_value(changed_properties, "PlaybackStatus", G_VARIANT_TYPE_STRING);

  if (metadata_variant != NULL)
  {
    //metadata has changed, so update the label
    update_label(proxy, widgets->label);
    //and update progressbar
    update_progressbar_on_track_changed(progressbar_data);
  }
  if (playback_variant != NULL)
  {
    //PlaybackStatus has changed, update the button icon
    const char* playback_status = g_variant_get_string(playback_variant, NULL);
    if (!strcmp(playback_status, "Playing"))
    {
      progressbar_data->is_playing = TRUE;
      update_button_icon(widgets->button, "media-playback-pause");
    }
    else
    {
      progressbar_data->is_playing = FALSE;
      update_button_icon(widgets->button, "media-playback-start");
    }
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
  //from this point on, this code should only run once
  GDBusProxy* proxy = (GDBusProxy*)user_data;
  struct ProgressbarData* progressbar_data = create_progressbar(proxy);
  struct ButtonData* button_data = g_malloc(sizeof(struct ButtonData));
  struct DbusData* dbus_data = g_malloc(sizeof(struct DbusData)*3);
  struct WidgetData* widget_data = g_malloc(sizeof(struct WidgetData));
  struct AnchorPoint* anchor = g_malloc(sizeof(struct AnchorPoint));
  //load configuration
  struct AppConfig* app_config = load_application_config();

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
  g_object_set_data_full(G_OBJECT(main_window), "button_data_instance", button_data, g_free);
  g_object_set_data_full(G_OBJECT(main_window), "dbus_data_instance", dbus_data, g_free);
  g_object_set_data_full(G_OBJECT(main_window), "widget_data_instance", widget_data, g_free);
  g_object_set_data_full(G_OBJECT(main_window), "anchor_point_instance", anchor, g_free);
  g_object_set_data_full(G_OBJECT(main_window), "app_config_instance", app_config, (GDestroyNotify)free_app_config);
  g_object_set_data_full(G_OBJECT(main_window), "progressbar_data_instance", progressbar_data, (GDestroyNotify)free_progressbar_data);
  
  //window configuration; window type is GTK_WINDOW_TOPLEVEL, but we have to make it look like a popup menu
  gtk_window_set_title(GTK_WINDOW(main_window), "spotify-status");
  gtk_window_set_decorated(GTK_WINDOW(main_window), 0);
  gtk_window_set_type_hint(GTK_WINDOW(main_window), GDK_WINDOW_TYPE_HINT_NOTIFICATION);
  if (app_config->resizable)
    gtk_window_set_resizable(GTK_WINDOW(main_window), 1);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(main_window),1);
  //if a window is sticky it will show up on all workspaces / desktops
  if (app_config->g_stick)
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
  gtk_grid_insert_row(GTK_GRID(main_grid), 2);
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
    g_printerr("Couldn't access the playback status. Assuming it's paused.\n");
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
  gtk_grid_attach(GTK_GRID(main_grid), progressbar_data->progressbar, 0, 2, 3, 1);

  gtk_container_add(GTK_CONTAINER(main_window), main_grid);

  //get mouse position when clicked
  gint mouse_x = (gint)event->x_root;
  gint mouse_y = (gint)event->y_root;

  //apply offset
  anchor->x = mouse_x + app_config->x_offset;
  anchor->y = mouse_y + app_config->y_offset;

  gtk_window_move(GTK_WINDOW(main_window), anchor->x, anchor->y);

  //whenever the window is resized (because the track names can be longer or shorter and the size of the window is not static)
  //call on_window_size_allocate which should make sure the window stays anchored to a specific position
  //if the widget shows up on the left size of the screen, this is a non-issue, because the window by default gets extened to the right
  //but if it show up on the right, it needs to be corrected
  if (!strcmp(app_config->system_tray_position, "right"))
    g_signal_connect(main_window, "size-allocate", G_CALLBACK(on_window_size_allocate), anchor);

  widget_data->button = pause_button;
  widget_data->label = label;

  //assign names to widgets to make them easy to format in style.css
  gtk_widget_set_name(main_window, "spotify-status-window");
  gtk_widget_set_name(previous_button, "spotify-status-previous-button");
  gtk_widget_set_name(pause_button, "spotify-status-pause-button");
  gtk_widget_set_name(next_button, "spotify-status-next-button");
  gtk_widget_set_name(label, "spotify-status-label");
  gtk_widget_set_name(main_grid, "spotify-status-grid");
  gtk_widget_set_name(progressbar_data->progressbar, "spotify-status-progressbar");

  //signals
  g_signal_connect(main_window, "key-press-event", G_CALLBACK(on_key_press_event), proxy);
  g_signal_connect(previous_button, "clicked", G_CALLBACK(on_button_click), &dbus_data[0]);
  g_signal_connect(pause_button, "clicked", G_CALLBACK(on_button_click), &dbus_data[1]);
  g_signal_connect(next_button, "clicked", G_CALLBACK(on_button_click), &dbus_data[2]);
  //the signal persists so long as the window exists. Once the window is destroyed, the signal disappears (although proxy still lives)
  g_signal_connect_object(proxy, "g-properties-changed", G_CALLBACK(on_spotify_properties_changed), main_window, G_CONNECT_DEFAULT);
  g_signal_connect(proxy, "g-signal::Seeked", G_CALLBACK(update_progressbar_on_seeked), progressbar_data);
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
  GtkStatusIcon* system_tray_icon = create_tray_icon();
  load_css();
  g_object_set_data_full(G_OBJECT(app), "system_tray_icon_instance", system_tray_icon, g_object_unref);
  g_signal_connect(system_tray_icon, "button-press-event", G_CALLBACK(create_main_window), proxy);
  //hold the application so it doesn't close
  g_application_hold(G_APPLICATION(app));
}

int main (int argc, char** argv)
{
  GDBusProxy* proxy = connect_to_dbus();
  if (proxy == NULL)
  {
    g_printerr("Could not access the D-Bus. Exiting...\n");
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