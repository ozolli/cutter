/*
 * benchmark.c - Benchmark mono-thread vs multi-thread
 */

/* Required for clock_gettime()/CLOCK_MONOTONIC under -std=c11 */
#define _POSIX_C_SOURCE 199309L

#include "colgen.h"
#include "csv_io.h"
#include "db.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _OPENMP
#include <omp.h>
#endif

static double get_time_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pieces.csv>\n", argv[0]);
        return 1;
    }

    const char *pieces_file = argv[1];

    /* Load settings */
    AppSettings settings;
    settings_load(&settings);

    /* Initialize database */
    if (db_init(settings.db_path) != 0) {
        fprintf(stderr, "Erreur: Impossible d'initialiser la base de données\n");
        return 1;
    }

    /* Load pieces */
    CSPInstance *instance = calloc(1, sizeof(CSPInstance));
    csp_instance_init(instance);
    instance->saw_kerf = settings.saw_kerf;
    instance->min_usable_offcut = settings.min_usable_offcut;

    int loaded = csv_load_pieces_odoo(pieces_file, instance);
    if (loaded <= 0) {
        fprintf(stderr, "Erreur: Impossible de charger les pièces depuis %s\n", pieces_file);
        free(instance);
        return 1;
    }
    printf("Chargé %d types de pièces\n", loaded);

    /* Load stock */
    int stock_count = db_load_inventory(instance);
    if (stock_count <= 0) {
        fprintf(stderr, "Erreur: Stock vide\n");
        free(instance);
        return 1;
    }
    printf("Chargé %d types de stock\n\n", stock_count);

    /* Benchmark with different thread counts */
    int thread_counts[] = {1, 2, 4, 8, 0};  /* 0 = default (all cores) */
    double baseline_time = 0.0;

    for (int t = 0; thread_counts[t] >= 0; t++) {
        int num_threads = thread_counts[t];

        #ifdef _OPENMP
        if (num_threads > 0) {
            omp_set_num_threads(num_threads);
        } else {
            num_threads = omp_get_max_threads();
        }
        #else
        if (num_threads != 1) {
            printf("OpenMP non disponible, test ignoré pour %d threads\n\n", num_threads);
            continue;
        }
        #endif

        /* Create fresh copy of instance for this test */
        CSPInstance *test_instance = calloc(1, sizeof(CSPInstance));
        memcpy(test_instance, instance, sizeof(CSPInstance));
        test_instance->num_patterns = 0;  /* Reset patterns */

        printf("=== Test avec %d thread%s ===\n", num_threads, num_threads > 1 ? "s" : "");

        CSPParameters params = csp_default_params();
        params.max_iterations = 100;
        params.time_limit_seconds = 60.0;
        params.verbose = false;

        CSPSolution solution;
        memset(&solution, 0, sizeof(solution));

        double start = get_time_seconds();
        int status = colgen_solve(test_instance, params, &solution);
        double elapsed = get_time_seconds() - start;

        if (status == 0) {
            if (t == 0) {
                baseline_time = elapsed;
            }
            printf("✓ Solution trouvée\n");
            printf("  Temps:      %.2f secondes\n", elapsed);
            printf("  Patterns:   %d générés\n", test_instance->num_patterns);
            printf("  Barres:     %d utilisées\n", solution.num_bars_used);
            printf("  Efficacité: %.1f%%\n", solution.efficiency);
            if (t == 0) {
                printf("  Speedup:    1.00x (baseline)\n");
            } else if (baseline_time > 0) {
                printf("  Speedup:    %.2fx\n", baseline_time / elapsed);
            }
        } else {
            printf("✗ Échec (status=%d)\n", status);
            printf("  Temps: %.2f secondes\n", elapsed);
        }
        printf("\n");

        free(test_instance);
    }

    free(instance);
    db_close();

    return 0;
}
