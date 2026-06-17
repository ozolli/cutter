/*
 * colgen.h - Column generation solver for the Cutting Stock Problem
 *
 * Main entry point for solving cutting stock problems optimally.
 */

#ifndef COLGEN_H
#define COLGEN_H

#include "csp_types.h"

/*
 * Solve a cutting stock problem instance using column generation.
 *
 * Parameters:
 *   instance - Problem instance (pieces, stocks, parameters)
 *   params   - Algorithm parameters (iterations, tolerances, etc.)
 *   solution - Output: the computed solution
 *
 * Returns: 0 on success, -1 on failure
 *
 * The instance->patterns array will be populated with generated patterns.
 */
int colgen_solve(CSPInstance *instance, CSPParameters params, CSPSolution *solution);

/*
 * Check if a problem instance is feasible (all pieces fit in some stock).
 */
bool colgen_check_feasibility(const CSPInstance *instance);

/*
 * Print solution details to stdout.
 */
void colgen_print_solution(const CSPInstance *instance, const CSPSolution *solution);

#endif /* COLGEN_H */
