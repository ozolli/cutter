/*
 * colgen.c - Column generation algorithm implementation
 */

#include "colgen.h"
#include "glpk_master.h"
#include "knapsack.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Generate initial patterns: multiple patterns per (stock, piece) pair with varying quantities */
static void generate_initial_patterns(CSPInstance *instance, MasterProblem *master)
{
    int pattern_id = instance->num_patterns;  /* continue after any offcut patterns */

    for (int s = 0; s < instance->num_stocks; s++) {
        /* Offcuts are consumed by the greedy pre-pass, not the master LP/ILP */
        if (instance->stocks[s].is_offcut) continue;

        double stock_len = instance->stocks[s].length;

        for (int p = 0; p < instance->num_pieces; p++) {
            double piece_len = instance->pieces[p].length;
            int demand = instance->pieces[p].quantity;

            /* Check if piece is compatible with this stock (length + dimensions) */
            if (csp_piece_fits_stock(&instance->pieces[p], &instance->stocks[s])) {
                /* Calculate maximum pieces that fit */
                int max_fit = 0;
                double remaining = stock_len;
                while (remaining >= piece_len + instance->saw_kerf) {
                    max_fit++;
                    remaining -= (piece_len + instance->saw_kerf);
                }

                if (max_fit == 0) continue;  /* Piece doesn't fit with kerf */

                /* Generate patterns with 1 to min(demand, max_fit) pieces
                 * This gives the ILP flexibility to choose exact quantities */
                int max_pattern_qty = (demand < max_fit) ? demand : max_fit;

                for (int qty = 1; qty <= max_pattern_qty; qty++) {
                    CuttingPattern pattern;
                    memset(&pattern, 0, sizeof(pattern));

                    pattern.pattern_id = pattern_id++;
                    pattern.stock_id = s;
                    pattern.cuts[p] = qty;

                    /* Calculate waste: remaining length after cutting */
                    double used = qty * (piece_len + instance->saw_kerf);
                    pattern.waste = stock_len - used;

                    /* Add to instance and master */
                    if (instance->num_patterns < MAX_PATTERNS) {
                        instance->patterns[instance->num_patterns++] = pattern;
                        master_add_column(master, instance, &pattern);
                    }
                }
            }
        }
    }
}

/*
 * Greedy offcut consumption.
 *
 * Workshop rule: use offcuts (chutes) before starting any fresh bar, consuming
 * them from the SMALLEST to the LARGEST. This clears small scrap first and is a
 * deliberate priority that cannot be expressed as cost minimisation (adding an
 * otherwise-unneeded small offcut always increases total cost).
 *
 * Each offcut unit is filled as much as possible with still-demanded, compatible
 * pieces (knapsack maximising filled length => minimising that offcut's waste).
 * Produced patterns are appended to out_patterns; the uncovered demand is written
 * back into remaining[]. Returns the number of offcut patterns produced.
 */
static int consume_offcuts_greedy(CSPInstance *instance, int32_t *remaining,
                                  CuttingPattern *out_patterns, int max_out)
{
    int num_out = 0;

    /* Collect usable offcut stock indices */
    int *order = malloc(instance->num_stocks * sizeof(int));
    if (!order) return 0;
    int n_off = 0;
    for (int s = 0; s < instance->num_stocks; s++) {
        if (instance->stocks[s].is_offcut && instance->stocks[s].quantity != 0)
            order[n_off++] = s;
    }

    /* Sort offcut indices by length ascending (smallest first) */
    for (int i = 1; i < n_off; i++) {
        int key = order[i];
        double kl = instance->stocks[key].length;
        int j = i - 1;
        while (j >= 0 && instance->stocks[order[j]].length > kl) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }

    double *value = malloc(instance->num_pieces * sizeof(double));
    if (!value) { free(order); return 0; }

    for (int oi = 0; oi < n_off && num_out < max_out; oi++) {
        int s = order[oi];
        const StockBar *stock = &instance->stocks[s];
        int avail = stock->quantity;
        if (avail < 0) avail = 1;  /* defensive: offcuts are normally finite */

        for (int u = 0; u < avail && num_out < max_out; u++) {
            /* Value = piece length if compatible and still demanded, else 0 */
            bool any = false;
            for (int p = 0; p < instance->num_pieces; p++) {
                if (remaining[p] > 0 &&
                    csp_piece_fits_stock(&instance->pieces[p], stock)) {
                    value[p] = instance->pieces[p].length;
                    any = true;
                } else {
                    value[p] = 0.0;
                }
            }
            if (!any) break;  /* nothing left fits this offcut */

            KnapsackResult ks = knapsack_solve(
                instance->pieces, instance->num_pieces, value, remaining,
                stock->length, instance->saw_kerf, 1.0);

            int total = 0;
            for (int p = 0; p < instance->num_pieces; p++) total += ks.pattern[p];
            if (total == 0) break;  /* this offcut can't hold any remaining piece */

            CuttingPattern *pat = &out_patterns[num_out++];
            memset(pat, 0, sizeof(*pat));
            pat->stock_id = s;
            memcpy(pat->cuts, ks.pattern, sizeof(pat->cuts));
            pat->waste = ks.waste;

            for (int p = 0; p < instance->num_pieces; p++)
                remaining[p] -= ks.pattern[p];
        }
    }

    free(value);
    free(order);
    return num_out;
}

bool colgen_check_feasibility(const CSPInstance *instance)
{
    bool feasible = true;

    /* Check each piece has compatible stock */
    for (int p = 0; p < instance->num_pieces; p++) {
        bool fits = false;
        for (int s = 0; s < instance->num_stocks; s++) {
            if (csp_piece_fits_stock(&instance->pieces[p], &instance->stocks[s])) {
                fits = true;
                break;
            }
        }
        if (!fits) {
            const PieceDemand *piece = &instance->pieces[p];
            if (piece->diameter > 0) {
                fprintf(stderr, "Erreur: La piece '%s' (%.1fmm, D%.0fx%.1f) ne correspond a aucun stock\n",
                        piece->label, piece->length, piece->diameter, piece->thickness);
            } else {
                fprintf(stderr, "Erreur: La piece '%s' (%.1fmm) ne rentre dans aucun stock\n",
                        piece->label, piece->length);
            }
            feasible = false;
        }
    }

    /* Estimate minimum bars needed per stock type using GROUPED estimation
     * (sum total length needed, divide by stock length - allows pieces to share bars) */
    for (int s = 0; s < instance->num_stocks; s++) {
        if (instance->stocks[s].quantity < 0) continue;  /* Unlimited */

        double total_length_needed = 0.0;
        bool has_dependent_pieces = false;

        for (int p = 0; p < instance->num_pieces; p++) {
            if (!csp_piece_fits_stock(&instance->pieces[p], &instance->stocks[s])) continue;

            /* Check if this is the ONLY compatible stock for this piece */
            bool only_stock = true;
            for (int s2 = 0; s2 < instance->num_stocks; s2++) {
                if (s2 != s && csp_piece_fits_stock(&instance->pieces[p], &instance->stocks[s2])) {
                    only_stock = false;
                    break;
                }
            }

            if (only_stock) {
                /* This piece MUST use this stock - add to total length needed */
                total_length_needed += (instance->pieces[p].length + instance->saw_kerf)
                                      * instance->pieces[p].quantity;
                has_dependent_pieces = true;
            }
        }

        if (has_dependent_pieces) {
            /* Estimate minimum bars using grouped approach (pieces can share bars).
             * Each piece is counted as (length + kerf); a bar's effective capacity
             * is therefore (length + kerf) too - the last piece on a bar needs no
             * trailing kerf. Without the +kerf on the denominator, a stock whose
             * length exactly equals a piece (e.g. a 275mm offcut for a 275mm piece)
             * is wrongly judged insufficient. */
            double bar_capacity = instance->stocks[s].length + instance->saw_kerf;
            int min_bars_needed = (int)ceil(total_length_needed / bar_capacity);

            if (min_bars_needed > instance->stocks[s].quantity) {
                fprintf(stderr, "Erreur: Stock '%s' insuffisant: %d barres disponibles, ~%d necessaires (%.0fmm)\n",
                        instance->stocks[s].label,
                        instance->stocks[s].quantity,
                        min_bars_needed,
                        total_length_needed);
                feasible = false;
            }
        }
    }

    return feasible;
}

int colgen_solve(CSPInstance *instance, CSPParameters params, CSPSolution *solution)
{
    memset(solution, 0, sizeof(CSPSolution));

    if (instance->num_pieces <= 0) {
        return 0;  /* nothing to cut */
    }
    /* Local, provably-positive piece count for allocation sizes */
    const size_t np = (size_t)instance->num_pieces;

    /* Check feasibility first */
    if (!colgen_check_feasibility(instance)) {
        return -1;
    }

    /* Phase 0: consume offcuts greedily (smallest first) before any fresh bar.
     * Pieces covered by offcuts are removed from the demand the master LP/ILP
     * must satisfy with fresh bars. We temporarily overwrite pieces[].quantity
     * with the remaining demand and restore it before returning. */
    int32_t *orig_qty = malloc(np * sizeof(int32_t));
    int32_t *remaining = malloc(np * sizeof(int32_t));
    if (!orig_qty || !remaining) {
        free(orig_qty); free(remaining);
        return -1;
    }
    for (int p = 0; p < instance->num_pieces; p++) {
        orig_qty[p] = remaining[p] = instance->pieces[p].quantity;
    }

    instance->num_patterns = 0;
    int offcut_count = consume_offcuts_greedy(instance, remaining,
                                              instance->patterns, MAX_PATTERNS);
    for (int i = 0; i < offcut_count; i++) {
        instance->patterns[i].pattern_id = i;
        solution->pattern_usage[i] = 1;  /* each offcut pattern is used once */
    }
    instance->num_patterns = offcut_count;

    for (int p = 0; p < instance->num_pieces; p++) {
        instance->pieces[p].quantity = remaining[p];
    }

    /* Create master problem (over the reduced, fresh-bar demand) */
    MasterProblem *master = master_create(instance);
    if (!master) {
        fprintf(stderr, "Erreur: Impossible de creer le probleme maitre\n");
        for (int p = 0; p < instance->num_pieces; p++)
            instance->pieces[p].quantity = orig_qty[p];
        free(orig_qty); free(remaining);
        return -1;
    }

    /* Generate initial fresh-bar patterns (appended after the offcut patterns) */
    generate_initial_patterns(instance, master);

    if (params.verbose) {
        fprintf(stderr, "Patterns initiaux: %d (pieces: %d, stocks: %d)\n",
                instance->num_patterns, instance->num_pieces, instance->num_stocks);

        /* Debug: show stock info */
        for (int s = 0; s < instance->num_stocks; s++) {
            fprintf(stderr, "  Stock %d: %s, L=%.0f, D=%.0fx%.1f, qty=%d, mat=%d\n",
                    s, instance->stocks[s].label, instance->stocks[s].length,
                    instance->stocks[s].diameter, instance->stocks[s].thickness,
                    instance->stocks[s].quantity, instance->stocks[s].material);
        }

        /* Debug: show piece info */
        for (int p = 0; p < instance->num_pieces; p++) {
            fprintf(stderr, "  Piece %d: %s, L=%.0f, D=%.0fx%.1f, qty=%d, mat=%d\n",
                    p, instance->pieces[p].label, instance->pieces[p].length,
                    instance->pieces[p].diameter, instance->pieces[p].thickness,
                    instance->pieces[p].quantity, instance->pieces[p].material);
        }
    }

    /* Verify each piece has at least one pattern */
    for (int p = 0; p < instance->num_pieces; p++) {
        bool has_pattern = false;
        for (int j = 0; j < instance->num_patterns; j++) {
            if (instance->patterns[j].cuts[p] > 0) {
                has_pattern = true;
                break;
            }
        }
        /* pieces[].quantity is the remaining (post-offcut) demand here */
        if (instance->pieces[p].quantity > 0 && !has_pattern) {
            fprintf(stderr, "Attention: Aucun pattern pour '%s' (D%.0fx%.1f L%.0f)\n",
                    instance->pieces[p].label,
                    instance->pieces[p].diameter,
                    instance->pieces[p].thickness,
                    instance->pieces[p].length);
        }
    }

    /* Allocate dual prices array */
    double *dual_prices = calloc(np, sizeof(double));
    if (!dual_prices) {
        for (int p = 0; p < instance->num_pieces; p++)
            instance->pieces[p].quantity = orig_qty[p];
        free(orig_qty); free(remaining);
        master_destroy(master);
        return -1;
    }

    /* Main column generation loop */
    int iteration = 0;
    double lp_bound = 0.0;

    while (iteration < params.max_iterations) {
        /* Step 1: Solve master LP */
        int ret = master_solve_lp(master);
        if (ret != 0) {
            fprintf(stderr, "Erreur: LP maitre infaisable a l'iteration %d\n", iteration);
            for (int p = 0; p < instance->num_pieces; p++)
                instance->pieces[p].quantity = orig_qty[p];
            free(orig_qty); free(remaining);
            free(dual_prices);
            master_destroy(master);
            return -1;
        }

        lp_bound = master->obj_value;

        if (params.verbose) {
            printf("Iteration %d: LP = %.4f, Patterns = %d\n",
                   iteration, lp_bound, instance->num_patterns);
        }

        /* Step 2: Get dual prices */
        master_get_duals(master, dual_prices, instance->num_pieces);

        /* Step 3: Solve pricing subproblem for each stock type */
        CuttingPattern best_pattern;
        memset(&best_pattern, 0, sizeof(best_pattern));
        double best_reduced_cost = 0.0;
        bool found_improving = false;

        /* Filtered dual prices for dimension constraints */
        double *filtered_duals = calloc(np, sizeof(double));

        /* Demand limits for each piece type */
        int32_t *demand_limits = calloc(np, sizeof(int32_t));
        for (int p = 0; p < instance->num_pieces; p++) {
            demand_limits[p] = instance->pieces[p].quantity;
        }

        for (int s = 0; s < instance->num_stocks; s++) {
            /* Offcuts are handled by the greedy pre-pass, not column generation */
            if (instance->stocks[s].is_offcut) continue;

            double bar_cost = instance->stocks[s].cost;
            if (bar_cost <= 0.0) bar_cost = 1.0;

            /* Filter dual prices: only pieces compatible with this stock */
            for (int p = 0; p < instance->num_pieces; p++) {
                if (csp_piece_fits_stock(&instance->pieces[p], &instance->stocks[s])) {
                    filtered_duals[p] = dual_prices[p];
                } else {
                    filtered_duals[p] = 0.0;  /* Incompatible piece */
                }
            }

            KnapsackResult ks = knapsack_solve(
                instance->pieces,
                instance->num_pieces,
                filtered_duals,
                demand_limits,
                instance->stocks[s].length,
                instance->saw_kerf,
                bar_cost
            );

            /* Check if this pattern has negative reduced cost */
            if (ks.reduced_cost < best_reduced_cost - params.epsilon) {
                best_reduced_cost = ks.reduced_cost;

                best_pattern.pattern_id = instance->num_patterns;
                best_pattern.stock_id = s;
                memcpy(best_pattern.cuts, ks.pattern, sizeof(best_pattern.cuts));
                best_pattern.waste = ks.waste;
                best_pattern.reduced_cost = ks.reduced_cost;

                found_improving = true;
            }
        }

        free(filtered_duals);
        free(demand_limits);

        /* Step 4: Check termination */
        if (!found_improving) {
            if (params.verbose) {
                printf("Generation de colonnes convergee apres %d iterations\n", iteration);
            }
            break;
        }

        /* Step 5: Add improving pattern */
        if (instance->num_patterns >= MAX_PATTERNS) {
            fprintf(stderr, "Attention: Nombre maximum de patterns atteint\n");
            break;
        }

        master_add_column(master, instance, &best_pattern);
        instance->patterns[instance->num_patterns++] = best_pattern;

        if (params.verbose) {
            printf("  Ajout pattern %d (cout reduit: %.6f)\n",
                   best_pattern.pattern_id, best_reduced_cost);
        }

        iteration++;
    }

    free(dual_prices);
    solution->lp_bound = lp_bound;

    /* The master only holds the fresh-bar patterns (indices offcut_count..).
     * Its column j maps to instance->patterns[offcut_count + j]. */
    int num_fresh = instance->num_patterns - offcut_count;
    if (num_fresh < 0) num_fresh = 0;

    /* Step 6: Solve integer program for final solution */
    if (params.use_branch_and_bound) {
        if (params.verbose) {
            printf("Resolution du programme en nombres entiers...\n");
        }

        int ret = master_solve_ilp(master, params.time_limit_seconds);
        if (ret != 0) {
            fprintf(stderr, "Attention: ILP non resolu, utilisation de l'arrondi LP\n");
            /* Fall back to rounding */
            double *primals = calloc(num_fresh + 1, sizeof(double));
            master_get_primals(master, primals, num_fresh);
            for (int j = 0; j < num_fresh; j++) {
                solution->pattern_usage[offcut_count + j] = (int32_t)ceil(primals[j]);
            }
            free(primals);
            solution->is_optimal = false;
        } else {
            /* Check if artificial variables are still used (indicates true infeasibility) */
            double art_usage = master_get_artificial_usage(master);
            if (art_usage > 0.5) {
                fprintf(stderr, "Erreur: Probleme infaisable - stock insuffisant (%.0f unites manquantes)\n",
                        art_usage);
                for (int p = 0; p < instance->num_pieces; p++)
                    instance->pieces[p].quantity = orig_qty[p];
                free(orig_qty); free(remaining);
                master_destroy(master);
                return -1;
            }

            master_get_integer_solution(master, solution->pattern_usage + offcut_count, num_fresh);
            solution->is_optimal = (fabs(master->obj_value - lp_bound) < 0.5);
        }
    } else {
        /* Simple rounding */
        double *primals = calloc(num_fresh + 1, sizeof(double));
        master_get_primals(master, primals, num_fresh);
        for (int j = 0; j < num_fresh; j++) {
            solution->pattern_usage[offcut_count + j] = (int32_t)ceil(primals[j]);
        }
        free(primals);
        solution->is_optimal = false;
    }

    /* Restore the original demand for post-processing, stats and reporting */
    for (int p = 0; p < instance->num_pieces; p++) {
        instance->pieces[p].quantity = orig_qty[p];
    }
    free(orig_qty);
    free(remaining);

    /* Post-process: reduce pattern usage to match exact demands (no overproduction) */
    /* Calculate current production for each piece */
    int *produced = calloc(np, sizeof(int));
    for (int i = 0; i < instance->num_pieces; i++) {
        for (int j = 0; j < instance->num_patterns; j++) {
            produced[i] += solution->pattern_usage[j] * instance->patterns[j].cuts[i];
        }
    }

    /* Greedily reduce pattern usage while maintaining feasibility */
    bool reduced = true;
    while (reduced) {
        reduced = false;
        for (int j = 0; j < instance->num_patterns; j++) {
            if (solution->pattern_usage[j] <= 0) continue;

            /* Never drop an offcut pattern: consuming offcuts is the priority */
            if (instance->stocks[instance->patterns[j].stock_id].is_offcut) continue;

            /* Check if we can reduce this pattern's usage by 1 */
            bool can_reduce = true;
            for (int i = 0; i < instance->num_pieces; i++) {
                int new_produced = produced[i] - instance->patterns[j].cuts[i];
                if (new_produced < instance->pieces[i].quantity) {
                    can_reduce = false;
                    break;
                }
            }

            if (can_reduce) {
                solution->pattern_usage[j]--;
                for (int i = 0; i < instance->num_pieces; i++) {
                    produced[i] -= instance->patterns[j].cuts[i];
                }
                reduced = true;
            }
        }
    }
    free(produced);

    /* Calculate solution statistics */
    solution->num_bars_used = 0;
    solution->total_waste = 0.0;
    solution->total_length_used = 0.0;

    for (int j = 0; j < instance->num_patterns; j++) {
        if (solution->pattern_usage[j] > 0) {
            int stock_id = instance->patterns[j].stock_id;
            double stock_len = instance->stocks[stock_id].length;

            solution->num_bars_used += solution->pattern_usage[j];
            solution->total_waste += solution->pattern_usage[j] * instance->patterns[j].waste;
            solution->total_length_used += solution->pattern_usage[j] * stock_len;
        }
    }

    if (solution->total_length_used > 0) {
        solution->efficiency = 100.0 * (1.0 - solution->total_waste / solution->total_length_used);
    }

    master_destroy(master);
    return 0;
}

void colgen_print_solution(const CSPInstance *instance, const CSPSolution *solution)
{
    printf("\n========== SOLUTION ==========\n");
    printf("Barres utilisees: %d\n", solution->num_bars_used);
    printf("Chute totale: %.1f mm\n", solution->total_waste);
    printf("Rendement: %.2f%%\n", solution->efficiency);
    printf("Borne LP: %.2f\n", solution->lp_bound);
    printf("Optimal: %s\n", solution->is_optimal ? "Oui" : "Non");

    printf("\n--- Patterns utilises ---\n");
    for (int j = 0; j < instance->num_patterns; j++) {
        if (solution->pattern_usage[j] > 0) {
            const CuttingPattern *p = &instance->patterns[j];
            int stock_id = p->stock_id;

            printf("\nPattern %d (x%d) - Stock: %s (%.0fmm)\n",
                   p->pattern_id,
                   solution->pattern_usage[j],
                   instance->stocks[stock_id].label,
                   instance->stocks[stock_id].length);

            printf("  Decoupes:");
            for (int i = 0; i < instance->num_pieces; i++) {
                if (p->cuts[i] > 0) {
                    printf(" %dx%s(%.0fmm)",
                           p->cuts[i],
                           instance->pieces[i].label,
                           instance->pieces[i].length);
                }
            }
            printf("\n  Chute: %.1f mm\n", p->waste);
        }
    }

    /* Verify demands are satisfied */
    printf("\n--- Verification des demandes ---\n");
    for (int i = 0; i < instance->num_pieces; i++) {
        int produced = 0;
        for (int j = 0; j < instance->num_patterns; j++) {
            produced += solution->pattern_usage[j] * instance->patterns[j].cuts[i];
        }
        printf("%s: demande=%d, produit=%d %s\n",
               instance->pieces[i].label,
               instance->pieces[i].quantity,
               produced,
               produced >= instance->pieces[i].quantity ? "[OK]" : "[MANQUE]");
    }
}
