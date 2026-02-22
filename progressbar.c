#include "progressbar.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "mpris.h"

gboolean switch_dragging_state(GtkScale* scale, GdkEventButton* event, gpointer user_data)
{
    struct ProgressbarData* progressbar_data = (struct ProgressbarData*)user_data;
    if (event->type == GDK_BUTTON_PRESS)
    {
        progressbar_data->is_dragging = TRUE;
    }
    else if (event->type == GDK_BUTTON_RELEASE)
    {
        progressbar_data->is_dragging = FALSE;
        //only call seek_to_position when the button is released to avoid jittering
        seek_to_position(progressbar_data->proxy, progressbar_data->current_val);
    }
    return FALSE;
}

struct ProgressbarData* create_progressbar(GDBusProxy* proxy)
{
    struct ProgressbarData* progressbar_data = g_malloc(sizeof(struct ProgressbarData));
    char* status = get_playback_status(proxy);
    //assign some values
    progressbar_data->proxy = proxy;
    progressbar_data->max_val = get_track_length(proxy);
    progressbar_data->current_val = get_current_position(proxy);
    progressbar_data->adjustment = gtk_adjustment_new(progressbar_data->current_val, progressbar_data->min_val, progressbar_data->max_val, 1, 1, 1);
    progressbar_data->progressbar = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, progressbar_data->adjustment);
    progressbar_data->is_playing = (status && !strcmp(status, "Playing"));
    progressbar_data->is_dragging = FALSE;
    //cosmetic changes so it looks decent
    gtk_scale_set_draw_value(GTK_SCALE(progressbar_data->progressbar), FALSE);
    gtk_widget_set_hexpand(progressbar_data->progressbar, TRUE);
    gtk_widget_set_halign(progressbar_data->progressbar, GTK_ALIGN_FILL);
    //print debug info
    g_print("Track length: %f\n", progressbar_data->max_val);
    g_print("Current position: %f\n", progressbar_data->current_val);
    //add callback function to increment progressbar value (every 100ms)
    progressbar_data->timer_id = g_timeout_add(100, G_SOURCE_FUNC(increment_progressbar_value), progressbar_data);
    //add callback function to handle seeking
    g_signal_connect(GTK_RANGE(progressbar_data->progressbar), "change-value", G_CALLBACK(on_progressbar_value_changed), progressbar_data);
    //add callback function to switch dragging state, used to prevent the timer from incrementing the value while the user is dragging the slider
    g_signal_connect(GTK_WIDGET(progressbar_data->progressbar), "button-press-event", G_CALLBACK(switch_dragging_state), progressbar_data);
    g_signal_connect(GTK_WIDGET(progressbar_data->progressbar), "button-release-event", G_CALLBACK(switch_dragging_state), progressbar_data);
    g_free(status);
    return progressbar_data;
}

//timer
gboolean increment_progressbar_value(gpointer user_data)
{
    struct ProgressbarData* progressbar_data = (struct ProgressbarData*)user_data;
    if (!progressbar_data->is_dragging && progressbar_data->is_playing && progressbar_data->current_val < progressbar_data->max_val)
    {
        progressbar_data->current_val += 0.1;
        gtk_adjustment_set_value(progressbar_data->adjustment, progressbar_data->current_val);
        g_print("Successfully incremented progressbar value. New value: %f \n", progressbar_data->current_val);
    }
    return G_SOURCE_CONTINUE; //same as return 1;
}

void update_progressbar_on_track_changed(struct ProgressbarData* data)
{
    data->current_val = 0;
    data->max_val = get_track_length(data->proxy);

    gtk_adjustment_set_value(data->adjustment, data->current_val);
    gtk_adjustment_set_upper(data->adjustment, data->max_val);

    g_print("Track changed. New length: %f s\n", data->max_val);
}

void update_progressbar_on_seeked(GDBusProxy* self, gchar* sender_name, gchar* signal_name, GVariant* parameters, gpointer user_data)
{
    //first, get the track position
    gint64 position_microseconds;
    g_variant_get(parameters, "(x)", &position_microseconds);
    struct ProgressbarData* progressbar_data = (struct ProgressbarData*)user_data;
    progressbar_data->current_val = (gdouble)(position_microseconds / 1000000.0);
    gtk_adjustment_set_value(progressbar_data->adjustment, progressbar_data->current_val);

    g_print("Seeked signal handled. New position is %f seconds. \n", progressbar_data->current_val);
}

//custom destructor
void free_progressbar_data(gpointer data)
{
    struct ProgressbarData* progressbar_data = (struct ProgressbarData*)data;
    if (!progressbar_data) return;

    //stop the timer
    if (progressbar_data->timer_id > 0) {
        g_source_remove(progressbar_data->timer_id);
        g_print("Timer stopped (ID: %u)\n", progressbar_data->timer_id);
    }

    g_free(progressbar_data);
    g_print("progressbar_data has been freed.\n");
}

//this function only updates the internal values, it does not seek to the position
gboolean on_progressbar_value_changed(GtkRange* range, GtkScrollType scroll, gdouble value, gpointer user_data)
{
    struct ProgressbarData* progressbar_data = (struct ProgressbarData*)user_data;
    if (progressbar_data->is_dragging)
    {
        progressbar_data->current_val = value;
        gtk_adjustment_set_value(progressbar_data->adjustment, progressbar_data->current_val);
        g_print("Updated current value to: %f seconds\n", value);
    }
    return FALSE;
}