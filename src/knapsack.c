/*
 * knapsack.c - Bounded knapsack solver implementation
 *
 * Uses dynamic programming with discretization (1mm precision).
 * Implements binary representation optimization for bounded knapsack.
 *
 * The pattern achieving each DP value is recovered with a compact decision
 * trace (one byte per (item, capacity) cell) rather than by carrying a full
 * pattern array per capacity. This avoids copying a large array on every DP
 * update, which dominated the runtime for long bars / many piece types.
 */

#include "knapsack.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Discretization: 1mm precision (can increase for sub-mm precision) */
#define PRECISION 1.0

/* One 0-1 item produced by binary decomposition of a bounded piece type */
typedef struct {
    int32_t piece;   /* Index into the pieces array */
    int32_t take;    /* Number of copies this item represents */
    int32_t weight;  /* Discretized size = take * piece_size */
    double  value;   /* take * dual_price */
} BinItem;

KnapsackResult knapsack_solve(
    const PieceDemand *pieces,
    int32_t num_pieces,
    const double *dual_prices,
    const int32_t *demand_limits,
    double stock_length,
    double saw_kerf,
    double bar_cost)
{
    KnapsackResult result;
    memset(&result, 0, sizeof(result));

    /* Discretize capacity: we add one kerf because the last piece doesn't need one after it */
    int32_t capacity = (int32_t)((stock_length + saw_kerf) / PRECISION);
    if (capacity <= 0) {
        result.reduced_cost = bar_cost;
        return result;
    }

    /*
     * Build the list of 0-1 items via binary decomposition of each piece type.
     * This turns bounded knapsack into 0-1 knapsack with O(n * log(max_count))
     * items instead of O(n * max_count).
     */
    int32_t items_cap = 16;
    int32_t num_items = 0;
    BinItem *items = malloc(items_cap * sizeof(*items));
    if (!items) {
        result.reduced_cost = bar_cost;
        return result;
    }

    for (int32_t i = 0; i < num_pieces; i++) {
        if (dual_prices[i] <= 1e-9) continue;  /* Skip if no value */

        /* Effective piece size: length + kerf (kerf consumed after each cut) */
        int32_t piece_size = (int32_t)((pieces[i].length + saw_kerf) / PRECISION);
        if (piece_size <= 0 || piece_size > capacity) continue;

        /* Maximum copies: min of what fits and demand limit */
        int32_t max_copies = capacity / piece_size;
        if (demand_limits && demand_limits[i] > 0 && demand_limits[i] < max_copies) {
            max_copies = demand_limits[i];
        }

        int32_t remaining = max_copies;
        int32_t k = 1;
        while (remaining > 0) {
            int32_t take = (k <= remaining) ? k : remaining;

            if (num_items == items_cap) {
                items_cap *= 2;
                BinItem *tmp = realloc(items, items_cap * sizeof(*items));
                if (!tmp) {
                    free(items);
                    result.reduced_cost = bar_cost;
                    return result;
                }
                items = tmp;
            }

            items[num_items].piece = i;
            items[num_items].take = take;
            items[num_items].weight = take * piece_size;
            items[num_items].value = take * dual_prices[i];
            num_items++;

            remaining -= take;
            k *= 2;
        }
    }

    /* DP value array and decision trace (keep[item][c] = item taken at capacity c) */
    double *dp = calloc(capacity + 1, sizeof(double));
    char *keep = (num_items > 0)
        ? calloc((size_t)num_items * (capacity + 1), sizeof(char))
        : NULL;

    if (!dp || (num_items > 0 && !keep)) {
        free(dp);
        free(keep);
        free(items);
        result.reduced_cost = bar_cost;
        return result;
    }

    /* Standard 0-1 knapsack with reverse iteration, recording kept items */
    for (int32_t it = 0; it < num_items; it++) {
        int32_t weight = items[it].weight;
        double value = items[it].value;
        char *keep_row = keep + (size_t)it * (capacity + 1);

        for (int32_t c = capacity; c >= weight; c--) {
            double new_val = dp[c - weight] + value;
            if (new_val > dp[c] + 1e-9) {
                dp[c] = new_val;
                keep_row[c] = 1;
            }
        }
    }

    /* Find the best solution */
    double best_value = 0.0;
    int32_t best_capacity = 0;
    for (int32_t c = 0; c <= capacity; c++) {
        if (dp[c] > best_value) {
            best_value = dp[c];
            best_capacity = c;
        }
    }

    /* Reconstruct the pattern by walking the decision trace backwards */
    int32_t c = best_capacity;
    for (int32_t it = num_items - 1; it >= 0 && c > 0; it--) {
        char *keep_row = keep + (size_t)it * (capacity + 1);
        if (keep_row[c]) {
            result.pattern[items[it].piece] += items[it].take;
            c -= items[it].weight;
        }
    }

    result.total_value = best_value;
    result.reduced_cost = bar_cost - best_value;

    /* Calculate actual waste */
    double used = 0.0;
    int num_cuts = 0;
    for (int32_t i = 0; i < num_pieces; i++) {
        if (result.pattern[i] > 0) {
            used += result.pattern[i] * pieces[i].length;
            num_cuts += result.pattern[i];
        }
    }
    /* Add kerf for each piece cut (n pieces = n cuts = n kerfs) */
    used += num_cuts * saw_kerf;
    result.waste = stock_length - used;
    if (result.waste < 0) result.waste = 0;

    /* Cleanup */
    free(dp);
    free(keep);
    free(items);

    return result;
}
