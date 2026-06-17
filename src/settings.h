/*
 * settings.h - Application settings management
 *
 * Stores settings in ~/.cutter/settings.conf
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#define SETTINGS_MAX_PATH 512

/*
 * Application settings structure
 */
typedef struct {
    char db_path[SETTINGS_MAX_PATH];    /* Path to inventory.db */
    double min_usable_offcut;           /* Minimum usable offcut length (mm) */
    double saw_kerf;                    /* Saw blade width (mm) */
} AppSettings;

/*
 * Load settings from ~/.cutter/settings.conf
 * If file doesn't exist, returns default settings.
 *
 * Returns: 0 on success, -1 on error
 */
int settings_load(AppSettings *settings);

/*
 * Save settings to ~/.cutter/settings.conf
 *
 * Returns: 0 on success, -1 on error
 */
int settings_save(const AppSettings *settings);

/*
 * Get default settings
 */
AppSettings settings_default(void);

/*
 * Get settings file path (~/.cutter/settings.conf)
 */
const char *settings_file_path(void);

#endif /* SETTINGS_H */
