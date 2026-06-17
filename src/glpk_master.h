/*
 * glpk_master.h - GLPK interface for the master problem
 *
 * The master problem in column generation:
 *   Minimize: sum(c_j * x_j)               (minimize bars used / cost)
 *   Subject to: sum(a_ij * x_j) >= d_i     (satisfy all demands)
 *               x_j >= 0                    (LP) or integer (ILP)
 *
 * Where:
 *   x_j   = number of times pattern j is used
 *   a_ij  = number of pieces of type i in pattern j
 *   d_i   = demand for piece type i
 *   c_j   = cost of one bar used by pattern j
 */

#ifndef GLPK_MASTER_H
#define GLPK_MASTER_H

#include <glpk.h>
#include "csp_types.h"

/*
 * MasterProblem: encapsulates the GLPK LP/ILP
 */
typedef struct {
    glp_prob *lp;               /* GLPK problem object */
    int32_t num_rows;           /* Number of constraints (= num piece types + num stocks) */
    int32_t num_cols;           /* Number of columns (= num patterns) */
    int32_t num_pieces;         /* Number of piece demand constraints */
    int32_t num_stocks;         /* Number of stock quantity constraints */
    int32_t stock_row_start;    /* First row index for stock constraints (1-indexed) */
    int32_t num_artificial;     /* Number of artificial variables (for initial feasibility) */
    double obj_value;           /* Objective function value after solving */
} MasterProblem;

/* Create and initialize master problem with demand constraints */
MasterProblem *master_create(const CSPInstance *instance);

/* Add a new column (pattern) to the master problem */
void master_add_column(MasterProblem *master,
                       const CSPInstance *instance,
                       const CuttingPattern *pattern);

/* Solve the LP relaxation */
int master_solve_lp(MasterProblem *master);

/* Get dual prices (shadow prices) after solving LP */
void master_get_duals(const MasterProblem *master, double *duals, int32_t num_pieces);

/* Get primal solution values (pattern usage) after solving */
void master_get_primals(const MasterProblem *master, double *primals, int32_t num_patterns);

/* Solve as Integer Linear Program (for final solution) */
int master_solve_ilp(MasterProblem *master, double time_limit_seconds);

/* Get integer solution values after ILP solve */
void master_get_integer_solution(const MasterProblem *master, int32_t *solution, int32_t num_patterns);

/* Check if any artificial variables are used (indicates true infeasibility) */
double master_get_artificial_usage(const MasterProblem *master);

/* Clean up */
void master_destroy(MasterProblem *master);

#endif /* GLPK_MASTER_H */
