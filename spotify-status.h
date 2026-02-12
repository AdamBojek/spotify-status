#pragma once
#ifndef SPOTIFY_STATUS_H
#define SPOTIFY_STATUS_H

#include "gio/gio.h"
#include "glib.h"
#include <gtk/gtk.h>

//definitions
static gboolean create_main_window(GtkWidget* systray_icon, GdkEventButton *event, gpointer user_data);
static void swap_button_icon(GtkWidget* button, gpointer user_data);
static void activate (GtkApplication* app, gpointer user_data);
static gboolean on_focus_out(GtkWidget* window, GdkEventFocus* event, gpointer user_data);
static GtkStatusIcon* create_tray_icon();
static void on_button_click(GtkWidget* widget, gpointer user_data);
static void set_correct_button_icon(GtkWidget* button, gpointer user_data);
static void on_spotify_properties_changed(GDBusProxy* proxy, GVariant* changed_properties, char** invalidated_properties, gpointer user_data);
static void update_label(GDBusProxy* proxy, GtkWidget* label);
static void update_button_icon(GtkWidget* button, const char* icon_name);

//custom struct used to pass multiple arguments to swap_button_icon
struct ButtonData
{
  GtkWidget* button;
  char* current_icon;
  GtkIconSize icon_size;
};

//custom struct used to pass data to on_button_clicked
struct DbusData
{
  GDBusProxy* proxy;
  const char* method;
};

//custom struct used to pass data to on_spotify_properties_changed
struct WidgetData
{
  GtkWidget* button;
  GtkWidget* label;
};

#endif