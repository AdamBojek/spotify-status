#include "config.h"
#include "glib.h"

struct AppConfig* load_application_config()
{
    GError* error = NULL;
    char* file_path = g_strconcat(g_get_user_config_dir(), "/spotify-status/config.ini", NULL);
    GKeyFile* parser = g_key_file_new();
    g_key_file_load_from_file(parser, file_path, G_KEY_FILE_NONE, &error);
    struct AppConfig* default_config = g_malloc(sizeof(struct AppConfig));
    default_config->x_offset = 0;
    default_config->y_offset = 0;
    default_config->g_stick = FALSE;
    default_config->resizable = FALSE;
    default_config->system_tray_position = g_strdup("right");
    if (error)
    {
        g_printerr("Could not load config file: %s", error->message);
        g_error_free(error);
    } else 
    {
        default_config->x_offset = g_key_file_get_integer(parser, "AppConfig", "x_offset", NULL);
        default_config->y_offset = g_key_file_get_integer(parser, "AppConfig", "y_offset", NULL);
        default_config->g_stick = g_key_file_get_boolean(parser, "AppConfig", "g_stick", NULL);
        default_config->resizable = g_key_file_get_boolean(parser, "AppConfig", "resizable", NULL);
        default_config->system_tray_position = g_key_file_get_string(parser, "AppConfig", "system_tray_position", &error);
        if (error)
        {
            g_printerr("Error loading configuration parameter system_tray_position: %s", error->message);
            default_config->system_tray_position = g_strdup("right");
            g_error_free(error);
        }
    }
    g_free(file_path);
    g_key_file_free(parser);
    return default_config;
}

void free_app_config(struct AppConfig* config)
{
    if (config->system_tray_position)
        g_free(config->system_tray_position);
    g_free(config);
}