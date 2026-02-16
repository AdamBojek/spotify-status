#include <gtk/gtk.h>

struct AppConfig {
    gint x_offset; //how many pixels to move the widget to the right, can be negative
    gint y_offset; //how many pixels to move the widget downwards, can be negative
    gboolean g_stick; //sticky window
    gboolean resizable;
    char* system_tray_position; //right or left, needed to show the widget correctly
};

struct AppConfig* load_application_config();
//custom function to free memory; useful because one of the properties of AppConfig is a string that itself needs to be freed
void free_app_config(struct AppConfig* config);