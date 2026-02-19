#include "progressbar.h"
#include "gtk/gtk.h"
#include "mpris.h"

GtkWidget* create_progressbar(GDBusProxy* proxy)
{
    gint track_length = get_track_length(proxy)*1000000; //convert microseconds to seconds
    gint current_position = get_current_position(proxy);
    g_print("Track length: %d\n", track_length);
    g_print("Current position: %d\n", current_position);
    GtkAdjustment* adjustment = gtk_adjustment_new(current_position, 0, track_length, 1, 1, 1);
    GtkWidget* progressbar = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adjustment);
    gtk_scale_set_draw_value(GTK_SCALE(progressbar), FALSE);
    return progressbar;
}