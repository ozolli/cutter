/*
 * main.c - CLI entry point for the cutting stock optimizer
 *
 * Commands:
 *   cutter cut -p pieces.csv [-s stock.csv] [--use-inventory] [-o out.csv] [-P out.pdf]
 *   cutter stock add <label> <length> [--diameter D] [--thickness T] [--qty N] [--cost C]
 *   cutter stock list [--all]
 *   cutter stock remove <id> [--qty N]
 *   cutter stock delete <id>
 *   cutter --demo [--pdf FILE]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "csp_types.h"
#include "colgen.h"
#include "csv_io.h"
#include "pdf_export.h"
#include "db.h"
#include "settings.h"

/* Helper to get settings (loads once and caches) */
static const AppSettings *get_settings(void)
{
    static AppSettings settings;
    static int loaded = 0;

    if (!loaded) {
        settings_load(&settings);
        loaded = 1;
    }

    return &settings;
}

/* Helper to get database path from settings */
static const char *get_db_path(void)
{
    return get_settings()->db_path;
}

static void print_usage(const char *prog)
{
    printf("Cutter - Optimiseur de decoupe lineaire\n\n");
    printf("Usage: %s <commande> [options]\n\n", prog);
    printf("Commandes:\n");
    printf("  cut              Lancer une session de decoupe\n");
    printf("  stock            Gerer l'inventaire du stock\n");
    printf("  --demo           Executer une demonstration\n");
    printf("  --help           Afficher cette aide\n");
    printf("\n");
    printf("Exemples:\n");
    printf("  %s cut -p pieces.csv -s stock.csv -P plan.pdf\n", prog);
    printf("  %s cut -p pieces.csv --use-inventory --apply\n", prog);
    printf("  %s stock add Tube_6m 6000 --diameter 50 --thickness 3 --qty 10\n", prog);
    printf("  %s stock list\n", prog);
}

static void print_cut_usage(void)
{
    printf("Usage: cutter cut [options]\n\n");
    printf("Options:\n");
    printf("  -p, --pieces FILE      Fichier CSV des pieces a decouper (requis)\n");
    printf("  -s, --stock FILE       Fichier CSV du stock (ou --use-inventory)\n");
    printf("  -I, --use-inventory    Utiliser le stock de l'inventaire\n");
    printf("  -A, --apply            Appliquer la coupe a l'inventaire (decrementer)\n");
    printf("  -S, --save-offcuts     Sauvegarder les chutes utiles dans l'inventaire\n");
    printf("  -P, --pdf FILE         Exporter le schema en PDF\n");
    printf("  -k, --kerf MM          Trait de scie en mm (defaut: depuis config)\n");
    printf("  -m, --min-offcut MM    Longueur minimale des chutes utiles (defaut: depuis config)\n");
    printf("  -v, --verbose          Afficher la progression\n");
}

static void print_stock_usage(void)
{
    printf("Usage: cutter stock <action> [options]\n\n");
    printf("Actions:\n");
    printf("  add <label> <longueur>   Ajouter du stock\n");
    printf("  list                     Lister le stock\n");
    printf("  remove <id> [--qty N]    Retirer du stock\n");
    printf("  delete <id>              Supprimer une entree\n");
    printf("\n");
    printf("Options pour 'add':\n");
    printf("  --diameter, -D MM      Diametre exterieur (tubes)\n");
    printf("  --thickness, -T MM     Epaisseur paroi (tubes)\n");
    printf("  --qty, -n N            Quantite (defaut: 1)\n");
    printf("  --cost, -c PRIX        Cout unitaire (defaut: 1.0)\n");
    printf("  --offcut               Marquer comme chute\n");
    printf("\n");
    printf("Options pour 'list':\n");
    printf("  --all, -a              Inclure les items a quantite 0\n");
}

/* ========== DEMO ========== */

static void run_demo(const char *pdf_file)
{
    printf("=== DEMONSTRATION ===\n\n");

    /* CSPInstance is ~16 MB (large static arrays), so it must be heap-allocated
     * to avoid overflowing the default thread stack. */
    CSPInstance *instance = calloc(1, sizeof(CSPInstance));
    CSPSolution *solution = calloc(1, sizeof(CSPSolution));
    if (!instance || !solution) {
        fprintf(stderr, "Erreur: allocation memoire echouee\n");
        free(instance);
        free(solution);
        return;
    }

    csp_instance_init(instance);

    instance->saw_kerf = 5.0;
    instance->min_usable_offcut = 100.0;

    csp_add_stock(instance, 6000.0, -1, 1.0, "Barre_6m");

    csp_add_piece(instance, 2200.0, 5, "Pied_2200");
    csp_add_piece(instance, 1500.0, 10, "Traverse_1500");
    csp_add_piece(instance, 800.0, 15, "Montant_800");
    csp_add_piece(instance, 450.0, 20, "Entretoise_450");

    printf("Probleme:\n");
    printf("  Stock: Barres de 6000mm (illimite)\n");
    printf("  Trait de scie: %.0fmm\n", instance->saw_kerf);
    printf("  Pieces a decouper:\n");
    for (int i = 0; i < instance->num_pieces; i++) {
        printf("    - %d x %.0fmm (%s)\n",
               instance->pieces[i].quantity,
               instance->pieces[i].length,
               instance->pieces[i].label);
    }

    CSPParameters params = csp_default_params();
    params.verbose = true;

    printf("\nResolution par generation de colonnes + GLPK...\n\n");

    if (colgen_solve(instance, params, solution) == 0) {
        colgen_print_solution(instance, solution);

        if (pdf_file) {
            printf("\nExport PDF vers: %s\n", pdf_file);
            pdf_export_solution(pdf_file, instance, solution);
        }
    } else {
        printf("Erreur lors de la resolution\n");
    }

    free(instance);
    free(solution);
}

/* ========== STOCK COMMANDS ========== */

static int cmd_stock_add(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Erreur: label et longueur requis\n");
        print_stock_usage();
        return 1;
    }

    const char *label = argv[0];
    double length = atof(argv[1]);
    double diameter = 0;
    double thickness = 0;
    int32_t quantity = 1;
    double cost = 1.0;
    bool is_offcut = false;

    static struct option opts[] = {
        {"diameter", required_argument, 0, 'D'},
        {"thickness", required_argument, 0, 'T'},
        {"qty", required_argument, 0, 'n'},
        {"cost", required_argument, 0, 'c'},
        {"offcut", no_argument, 0, 'O'},
        {0, 0, 0, 0}
    };

    /* Reset getopt */
    optind = 0;

    int c;
    while ((c = getopt_long(argc, argv, "D:T:n:c:O", opts, NULL)) != -1) {
        switch (c) {
            case 'D': diameter = atof(optarg); break;
            case 'T': thickness = atof(optarg); break;
            case 'n': quantity = atoi(optarg); break;
            case 'c': cost = atof(optarg); break;
            case 'O': is_offcut = true; break;
        }
    }

    if (length <= 0) {
        fprintf(stderr, "Erreur: longueur invalide\n");
        return 1;
    }

    if (db_init(get_db_path()) != 0) {
        return 1;
    }

    int64_t id = db_add_stock(label, length, diameter, thickness, quantity, cost, MATERIAL_UNKNOWN, is_offcut);
    if (id < 0) {
        fprintf(stderr, "Erreur: impossible d'ajouter le stock\n");
        db_close();
        return 1;
    }

    printf("Stock ajoute: %s %.0fmm", label, length);
    if (diameter > 0) printf(" D%.0f", diameter);
    if (thickness > 0) printf(" ep%.1f", thickness);
    printf(" x%d (ID: %ld)\n", quantity, id);

    db_close();
    return 0;
}

static int cmd_stock_list(int argc, char *argv[])
{
    bool include_zero = false;

    static struct option opts[] = {
        {"all", no_argument, 0, 'a'},
        {0, 0, 0, 0}
    };

    optind = 0;
    int c;
    while ((c = getopt_long(argc, argv, "a", opts, NULL)) != -1) {
        if (c == 'a') include_zero = true;
    }

    if (db_init(get_db_path()) != 0) {
        return 1;
    }

    StockItem *items = NULL;
    int count = db_list_stock(&items, include_zero);

    if (count < 0) {
        fprintf(stderr, "Erreur: impossible de lire l'inventaire\n");
        db_close();
        return 1;
    }

    if (count == 0) {
        printf("Inventaire vide.\n");
        printf("Utilisez 'cutter stock add' pour ajouter du stock.\n");
        db_close();
        return 0;
    }

    printf("\n=== INVENTAIRE DU STOCK ===\n\n");
    printf("%-4s %-20s %8s %8s %8s %5s %8s %6s\n",
           "ID", "Label", "Long.", "Diam.", "Epa.", "Qty", "Cout", "Type");
    printf("-------------------------------------------------------------------------------\n");

    for (int i = 0; i < count; i++) {
        StockItem *it = &items[i];
        printf("%-4ld %-20s %7.0fmm", it->id, it->label, it->length);

        if (it->diameter > 0)
            printf(" %7.0fmm", it->diameter);
        else
            printf(" %8s", "-");

        if (it->thickness > 0)
            printf(" %7.1fmm", it->thickness);
        else
            printf(" %8s", "-");

        printf(" %5d %8.2f %6s\n",
               it->quantity, it->cost,
               it->is_offcut ? "Chute" : "Stock");
    }

    printf("\nTotal: %d type(s) de stock\n", count);

    free(items);
    db_close();
    return 0;
}

static int cmd_stock_remove(int argc, char *argv[])
{
    if (argc < 1) {
        fprintf(stderr, "Erreur: ID requis\n");
        return 1;
    }

    int64_t id = atoll(argv[0]);
    int32_t quantity = 1;

    static struct option opts[] = {
        {"qty", required_argument, 0, 'n'},
        {0, 0, 0, 0}
    };

    optind = 0;
    int c;
    while ((c = getopt_long(argc, argv, "n:", opts, NULL)) != -1) {
        if (c == 'n') quantity = atoi(optarg);
    }

    if (db_init(NULL) != 0) return 1;

    if (db_remove_stock(id, quantity) != 0) {
        fprintf(stderr, "Erreur: impossible de retirer le stock\n");
        db_close();
        return 1;
    }

    printf("Stock retire: ID %ld, quantite %d\n", id, quantity);
    db_close();
    return 0;
}

static int cmd_stock_delete(int argc, char *argv[])
{
    if (argc < 1) {
        fprintf(stderr, "Erreur: ID requis\n");
        return 1;
    }

    int64_t id = atoll(argv[0]);

    if (db_init(NULL) != 0) return 1;

    if (db_delete_stock(id) != 0) {
        fprintf(stderr, "Erreur: impossible de supprimer l'entree\n");
        db_close();
        return 1;
    }

    printf("Entree supprimee: ID %ld\n", id);
    db_close();
    return 0;
}

static int cmd_stock(int argc, char *argv[])
{
    if (argc < 1) {
        print_stock_usage();
        return 1;
    }

    const char *action = argv[0];

    if (strcmp(action, "add") == 0) {
        return cmd_stock_add(argc - 1, argv + 1);
    } else if (strcmp(action, "list") == 0) {
        return cmd_stock_list(argc - 1, argv + 1);
    } else if (strcmp(action, "remove") == 0) {
        return cmd_stock_remove(argc - 1, argv + 1);
    } else if (strcmp(action, "delete") == 0) {
        return cmd_stock_delete(argc - 1, argv + 1);
    } else {
        fprintf(stderr, "Action inconnue: %s\n", action);
        print_stock_usage();
        return 1;
    }
}

/* ========== CUT COMMAND ========== */

static int cmd_cut(int argc, char *argv[])
{
    const char *pieces_file = NULL;
    const char *stock_file = NULL;
    const char *pdf_file = NULL;

    /* Load defaults from settings */
    const AppSettings *settings = get_settings();
    double saw_kerf = settings->saw_kerf;
    double min_usable_offcut = settings->min_usable_offcut;

    bool verbose = false;
    bool use_inventory = false;
    bool apply_to_inventory = false;
    bool save_offcuts = false;

    static struct option opts[] = {
        {"pieces", required_argument, 0, 'p'},
        {"stock", required_argument, 0, 's'},
        {"pdf", required_argument, 0, 'P'},
        {"kerf", required_argument, 0, 'k'},
        {"min-offcut", required_argument, 0, 'm'},
        {"verbose", no_argument, 0, 'v'},
        {"use-inventory", no_argument, 0, 'I'},
        {"apply", no_argument, 0, 'A'},
        {"save-offcuts", no_argument, 0, 'S'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    optind = 0;
    int c;
    while ((c = getopt_long(argc, argv, "p:s:P:k:m:vIASh", opts, NULL)) != -1) {
        switch (c) {
            case 'p': pieces_file = optarg; break;
            case 's': stock_file = optarg; break;
            case 'P': pdf_file = optarg; break;
            case 'k': saw_kerf = atof(optarg); break;
            case 'm': min_usable_offcut = atof(optarg); break;
            case 'v': verbose = true; break;
            case 'I': use_inventory = true; break;
            case 'A': apply_to_inventory = true; break;
            case 'S': save_offcuts = true; break;
            case 'h':
                print_cut_usage();
                return 0;
        }
    }

    if (!pieces_file) {
        fprintf(stderr, "Erreur: fichier pieces requis (-p)\n");
        print_cut_usage();
        return 1;
    }

    if (!stock_file && !use_inventory) {
        fprintf(stderr, "Erreur: fichier stock (-s) ou --use-inventory requis\n");
        return 1;
    }

    /* Initialize instance.
     * CSPInstance is ~16 MB (large static arrays), so it must be heap-allocated
     * to avoid overflowing the default thread stack. */
    CSPInstance *instance = calloc(1, sizeof(CSPInstance));
    CSPSolution *solution = calloc(1, sizeof(CSPSolution));
    if (!instance || !solution) {
        fprintf(stderr, "Erreur: allocation memoire echouee\n");
        free(instance);
        free(solution);
        return 1;
    }

    int ret = 1;
    bool db_open = false;

    csp_instance_init(instance);
    instance->saw_kerf = saw_kerf;
    instance->min_usable_offcut = min_usable_offcut;

    /* Load pieces */
    printf("Chargement des pieces: %s\n", pieces_file);
    int np = csv_load_pieces(pieces_file, instance);
    if (np < 0) goto cleanup;
    printf("  %d types de pieces charges\n", np);

    /* Load stock */
    if (use_inventory) {
        printf("Chargement du stock depuis l'inventaire...\n");
        if (db_init(get_db_path()) != 0) goto cleanup;
        db_open = true;
        int ns = db_load_inventory(instance);
        if (ns < 0) goto cleanup;
        printf("  %d types de stock charges depuis l'inventaire\n", ns);
    } else {
        printf("Chargement du stock: %s\n", stock_file);
        int ns = csv_load_stock(stock_file, instance);
        if (ns < 0) goto cleanup;
        printf("  %d types de stock charges\n", ns);
    }

    if (instance->num_stocks == 0) {
        fprintf(stderr, "Erreur: aucun stock disponible\n");
        goto cleanup;
    }

    /* Solve */
    CSPParameters params = csp_default_params();
    params.verbose = verbose;

    printf("\nTrait de scie: %.1fmm\n", instance->saw_kerf);
    printf("Resolution en cours...\n\n");

    if (colgen_solve(instance, params, solution) != 0) {
        fprintf(stderr, "Erreur: resolution echouee\n");
        goto cleanup;
    }

    colgen_print_solution(instance, solution);

    /* Apply to inventory if requested */
    if (apply_to_inventory && use_inventory) {
        printf("\nApplication a l'inventaire...\n");
        if (db_record_cutting_session(instance, solution, save_offcuts) == 0) {
            printf("Inventaire mis a jour.\n");
            if (save_offcuts) {
                printf("Chutes utiles (>= %.0fmm) ajoutees a l'inventaire.\n",
                       instance->min_usable_offcut);
            }
        }
    }

    /* Export PDF */
    if (pdf_file) {
        printf("\nExport PDF vers: %s\n", pdf_file);
        pdf_export_solution(pdf_file, instance, solution);
    }

    ret = 0;

cleanup:
    if (db_open) db_close();
    free(instance);
    free(solution);
    return ret;
}

/* ========== MAIN ========== */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    /* Handle --help and --demo at top level */
    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(cmd, "--demo") == 0 || strcmp(cmd, "-d") == 0) {
        const char *pdf_file = NULL;
        if (argc >= 4 && (strcmp(argv[2], "--pdf") == 0 || strcmp(argv[2], "-P") == 0)) {
            pdf_file = argv[3];
        }
        run_demo(pdf_file);
        return 0;
    }

    /* Subcommands */
    if (strcmp(cmd, "cut") == 0) {
        return cmd_cut(argc - 1, argv + 1);
    }

    if (strcmp(cmd, "stock") == 0) {
        return cmd_stock(argc - 2, argv + 2);
    }

    /* Legacy: if first arg looks like an option, treat as cut command */
    if (cmd[0] == '-') {
        return cmd_cut(argc, argv);
    }

    fprintf(stderr, "Commande inconnue: %s\n", cmd);
    print_usage(argv[0]);
    return 1;
}
