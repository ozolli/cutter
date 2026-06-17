/*
 * settings.c - Application settings management
 */

#include "settings.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char settings_path[SETTINGS_MAX_PATH] = "";

const char *settings_file_path(void)
{
    if (settings_path[0] == '\0') {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(settings_path, sizeof(settings_path),
                     "%s/.cutter/settings.conf", home);
        } else {
            snprintf(settings_path, sizeof(settings_path),
                     ".cutter/settings.conf");
        }
    }
    return settings_path;
}

AppSettings settings_default(void)
{
    AppSettings settings;
    memset(&settings, 0, sizeof(settings));

    /* Default database path */
    strncpy(settings.db_path, db_default_path(), SETTINGS_MAX_PATH - 1);
    settings.db_path[SETTINGS_MAX_PATH - 1] = '\0';

    /* Default cutting parameters */
    settings.min_usable_offcut = 100.0;  /* 100mm minimum offcut */
    settings.saw_kerf = 5.0;             /* 5mm saw blade */
    settings.db_network = false;         /* local disk by default */

    return settings;
}

int settings_load(AppSettings *settings)
{
    /* Start with defaults */
    *settings = settings_default();

    FILE *f = fopen(settings_file_path(), "r");
    if (!f) {
        /* File doesn't exist, use defaults */
        return 0;
    }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n') continue;

        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';

        /* Parse key=value */
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;

        /* Trim spaces */
        while (*key == ' ') key++;
        while (*value == ' ') value++;

        if (strcmp(key, "db_path") == 0) {
            strncpy(settings->db_path, value, SETTINGS_MAX_PATH - 1);
            settings->db_path[SETTINGS_MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "min_usable_offcut") == 0) {
            settings->min_usable_offcut = atof(value);
        } else if (strcmp(key, "saw_kerf") == 0) {
            settings->saw_kerf = atof(value);
        } else if (strcmp(key, "db_network") == 0) {
            settings->db_network = (atoi(value) != 0);
        }
    }

    fclose(f);

    /* Configure the DB layer so it opens the database in network-safe mode */
    db_set_network_mode(settings->db_network);

    return 0;
}

int settings_save(const AppSettings *settings)
{
    /* Ensure directory exists */
    const char *home = getenv("HOME");
    if (home) {
        char dir[SETTINGS_MAX_PATH];
        snprintf(dir, sizeof(dir), "%s/.cutter", home);
        mkdir(dir, 0755);
    }

    FILE *f = fopen(settings_file_path(), "w");
    if (!f) {
        fprintf(stderr, "Erreur: Impossible d'ecrire %s\n", settings_file_path());
        return -1;
    }

    fprintf(f, "# Cutter - Fichier de configuration\n");
    fprintf(f, "# Genere automatiquement\n\n");

    fprintf(f, "# Chemin de la base de donnees\n");
    fprintf(f, "db_path=%s\n\n", settings->db_path);

    fprintf(f, "# Longueur minimale des chutes utilisables (mm)\n");
    fprintf(f, "min_usable_offcut=%.1f\n\n", settings->min_usable_offcut);

    fprintf(f, "# Largeur du trait de scie (mm)\n");
    fprintf(f, "saw_kerf=%.1f\n\n", settings->saw_kerf);

    fprintf(f, "# Base de donnees sur un partage reseau (SMB/NFS): 1 = oui, 0 = non\n");
    fprintf(f, "# Active le verrouillage par fichier (.lock) compatible reseau\n");
    fprintf(f, "db_network=%d\n", settings->db_network ? 1 : 0);

    fclose(f);

    printf("Configuration sauvegardee: %s\n", settings_file_path());
    return 0;
}
