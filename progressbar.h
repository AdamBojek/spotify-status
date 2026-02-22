#pragma once
#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include "gio/gio.h"
#include <gtk/gtk.h>
#include <stdint.h>

struct ProgressbarData
{
    GtkWidget* progressbar;
    GDBusProxy* proxy;
    GtkAdjustment* adjustment;
    gdouble min_val;
    gdouble current_val;
    gdouble max_val;
    gdouble step;
    guint timer_id;
    gboolean is_playing;
    gboolean is_dragging;
};

gboolean switch_dragging_state(GtkScale* scale, GdkEventButton* event, gpointer user_data);
void free_progressbar_data(gpointer data);
struct ProgressbarData* create_progressbar(GDBusProxy* proxy);
void update_progressbar_on_seeked(GDBusProxy* self, gchar* sender_name, gchar* signal_name, GVariant* parameters, gpointer user_data);
gboolean increment_progressbar_value(gpointer data);
gboolean on_progressbar_value_changed(GtkRange* range, GtkScrollType scroll, gdouble value, gpointer user_data);
void update_progressbar_on_track_changed(struct ProgressbarData* data);

#endif