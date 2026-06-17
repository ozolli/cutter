/*
 * knapsack.h - Bounded knapsack solver for the pricing subproblem
 *
 * Solves the pricing subproblem in column generation:
 *   Maximize: sum(pi_i * a_i)   (total dual value)
 *   Subject to: sum((l_i + kerf) * a_i) <= L + kerf  (capacity)
 *               a_i >= 0, integer
 */

#ifndef KNAPSACK_H
#define KNAPSACK_H

#include "csp_types.h"

/*
 * KnapsackResult: result of the pricing subproblem
 */
typedef struct {
    int32_t pattern[MAX_PIECE_TYPES]; /* Count of each piece in the pattern */
    double total_value;               /* Total dual value achieved */
    double reduced_cost;              /* Reduced cost = cost - total_value */
    double waste;                     /* Remaining capacity (waste) in mm */
} KnapsackResult;

/*
 * Solve bounded knapsack using dynamic programming.
 *
 * Parameters:
 *   pieces       - Array of piece demands
 *   num_pieces   - Number of piece types
 *   dual_prices  - Dual prices (shadow prices) from master LP
 *   demand_limits - Maximum pieces of each type (NULL = unlimited)
 *   stock_length - Length of the stock bar
 *   saw_kerf     - Width of saw blade
 *   bar_cost     - Cost of one bar (for reduced cost calculation)
 *
 * Returns: KnapsackResult with the best pattern found
 */
KnapsackResult knapsack_solve(
    const PieceDemand *pieces,
    int32_t num_pieces,
    const double *dual_prices,
    const int32_t *demand_limits,
    double stock_length,
    double saw_kerf,
    double bar_cost
);

#endif /* KNAPSACK_H */
