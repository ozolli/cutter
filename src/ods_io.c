/*
 * ods_io.c - ODS (OpenDocument Spreadsheet) import/export for stock
 */

#define _POSIX_C_SOURCE 200809L
#include "ods_io.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <locale.h>

/* Path to Python scripts (relative to binary) */
#define PYTHON_IMPORT_SCRIPT "scripts/ods_import.py"
#define PYTHON_EXPORT_SCRIPT "scripts/ods_export.py"

/* Temporary CSV file */
#define TEMP_CSV "/tmp/cutter_stock_temp.csv"

static int run_python_script(const char *script, const char *arg1, const char *arg2)
{
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        execl("/usr/bin/python3", "python3", script, arg1, arg2, NULL);
        perror("execl");
        exit(1);
    }

    /* Parent process - wait for child */
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    }

    return -1;
}

int ods_import_stock(const char *filename)
{
    /* Step 1: Convert ODS to CSV using Python script */
    if (run_python_script(PYTHON_IMPORT_SCRIPT, filename, TEMP_CSV) != 0) {
        fprintf(stderr, "Erreur: Impossible de convertir le fichier ODS\n");
        return -1;
    }

    /* Step 2: Clear existing stock from database */
    if (db_clear_stock() != 0) {
        fprintf(stderr, "Erreur: Impossible de vider le stock existant\n");
        unlink(TEMP_CSV);
        return -1;
    }

    /* Save current locale and switch to C locale for reading decimal numbers */
    char *old_locale = setlocale(LC_NUMERIC, NULL);
    char *saved_locale = old_locale ? strdup(old_locale) : NULL;
    setlocale(LC_NUMERIC, "C");

    /* Step 3: Read CSV and import to database */
    FILE *fp = fopen(TEMP_CSV, "r");
    if (!fp) {
        fprintf(stderr, "Erreur: Impossible d'ouvrir %s\n", TEMP_CSV);
        if (saved_locale) {
            setlocale(LC_NUMERIC, saved_locale);
            free(saved_locale);
        }
        return -1;
    }

    char line[1024];
    int count = 0;
    bool header_skipped = false;

    while (fgets(line, sizeof(line), fp)) {
        /* Skip header */
        if (!header_skipped) {
            header_skipped = true;
            continue;
        }

        /* Parse CSV: label,length,diameter,thickness,quantity,cost,material,is_offcut */
        char label[256];
        double length, diameter, thickness, cost;
        int quantity, material, is_offcut;

        int parsed = sscanf(line, "%255[^,],%lf,%lf,%lf,%d,%lf,%d,%d",
                            label, &length, &diameter, &thickness,
                            &quantity, &cost, &material, &is_offcut);

        if (parsed != 8) {
            continue;  /* Skip malformed lines */
        }

        /* Add to database */
        int64_t id = db_add_stock(label, length, diameter, thickness,
                                  quantity, cost, (MaterialType)material, is_offcut);
        if (id >= 0) {
            count++;
        }
    }

    fclose(fp);
    unlink(TEMP_CSV);  /* Remove temporary file */

    /* Restore original locale */
    if (saved_locale) {
        setlocale(LC_NUMERIC, saved_locale);
        free(saved_locale);
    }

    return count;
}

int ods_export_stock(const char *filename)
{
    /* Save current locale and switch to C locale for consistent decimal separator */
    char *old_locale = setlocale(LC_NUMERIC, NULL);
    char *saved_locale = old_locale ? strdup(old_locale) : NULL;
    setlocale(LC_NUMERIC, "C");

    /* Step 1: Export database to CSV */
    FILE *fp = fopen(TEMP_CSV, "w");
    if (!fp) {
        fprintf(stderr, "Erreur: Impossible de créer %s\n", TEMP_CSV);
        if (saved_locale) {
            setlocale(LC_NUMERIC, saved_locale);
            free(saved_locale);
        }
        return -1;
    }

    /* Write CSV header */
    fprintf(fp, "id,label,length,diameter,thickness,quantity,cost,material,is_offcut\n");

    /* Get all stock items */
    StockItem *items = NULL;
    int count = db_list_stock(&items, true);  /* Include zero quantity */

    if (count < 0) {
        fclose(fp);
        if (saved_locale) {
            setlocale(LC_NUMERIC, saved_locale);
            free(saved_locale);
        }
        return -1;
    }

    /* Write stock items */
    for (int i = 0; i < count; i++) {
        fprintf(fp, "%ld,%s,%.1f,%.1f,%.1f,%d,%.2f,%d,%d\n",
                items[i].id,
                items[i].label,
                items[i].length,
                items[i].diameter,
                items[i].thickness,
                items[i].quantity,
                items[i].cost,
                (int)items[i].material,
                items[i].is_offcut ? 1 : 0);
    }

    free(items);
    fclose(fp);

    /* Restore original locale */
    if (saved_locale) {
        setlocale(LC_NUMERIC, saved_locale);
        free(saved_locale);
    }

    /* Step 2: Convert CSV to ODS using Python script */
    int result = run_python_script(PYTHON_EXPORT_SCRIPT, TEMP_CSV, filename);
    unlink(TEMP_CSV);  /* Remove temporary file */

    if (result != 0) {
        fprintf(stderr, "Erreur: Impossible de créer le fichier ODS\n");
        return -1;
    }

    return 0;
}
