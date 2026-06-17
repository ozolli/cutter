/*
 * glpk_master.c - GLPK master problem implementation
 */

#include "glpk_master.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

MasterProblem *master_create(const CSPInstance *instance)
{
    MasterProblem *master = calloc(1, sizeof(MasterProblem));
    if (!master) return NULL;

    /* Create GLPK problem */
    master->lp = glp_create_prob();
    glp_set_prob_name(master->lp, "cutting_stock_master");
    glp_set_obj_dir(master->lp, GLP_MIN);  /* Minimize number of bars */

    master->num_pieces = instance->num_pieces;
    master->num_stocks = instance->num_stocks;

    /* Count stocks with limited quantity */
    int num_stock_constraints = 0;
    for (int s = 0; s < instance->num_stocks; s++) {
        if (instance->stocks[s].quantity >= 0) {
            num_stock_constraints++;
        }
    }

    /* Add rows: piece demands + stock quantity constraints */
    master->num_rows = instance->num_pieces + num_stock_constraints;
    master->stock_row_start = instance->num_pieces + 1;  /* 1-indexed */
    glp_add_rows(master->lp, master->num_rows);

    /* Piece demand constraints: sum(a_ij * x_j) >= d_i */
    for (int i = 0; i < instance->num_pieces; i++) {
        char name[64];
        snprintf(name, sizeof(name), "demand_%d", i);
        glp_set_row_name(master->lp, i + 1, name);  /* GLPK is 1-indexed */

        glp_set_row_bnds(master->lp, i + 1, GLP_LO,
                         (double)instance->pieces[i].quantity, 0.0);
    }

    /* Stock quantity constraints: sum(x_j : pattern j uses stock s) <= q_s */
    int row = master->stock_row_start;
    for (int s = 0; s < instance->num_stocks; s++) {
        if (instance->stocks[s].quantity >= 0) {
            char name[64];
            snprintf(name, sizeof(name), "stock_%d", s);
            glp_set_row_name(master->lp, row, name);

            /* Upper bound = available quantity */
            glp_set_row_bnds(master->lp, row, GLP_UP,
                             0.0, (double)instance->stocks[s].quantity);
            row++;
        }
    }

    master->num_cols = 0;
    master->obj_value = 0.0;

    /* Add artificial variables to ensure initial feasibility.
     * These have very high cost and satisfy 1 unit of demand each.
     * Column generation will find real patterns to replace them. */
    master->num_artificial = instance->num_pieces;
    for (int i = 0; i < instance->num_pieces; i++) {
        int col = glp_add_cols(master->lp, 1);
        master->num_cols++;

        char name[64];
        snprintf(name, sizeof(name), "artificial_%d", i);
        glp_set_col_name(master->lp, col, name);

        /* Very high cost to discourage use */
        glp_set_obj_coef(master->lp, col, 10000.0);

        /* Bounds: x >= 0 */
        glp_set_col_bnds(master->lp, col, GLP_LO, 0.0, 0.0);

        /* This artificial variable provides 1 unit of piece i */
        int ind[2] = {0, i + 1};  /* GLPK 1-indexed, element 0 unused */
        double val[2] = {0.0, 1.0};
        glp_set_mat_col(master->lp, col, 1, ind, val);
    }

    return master;
}

void master_add_column(MasterProblem *master,
                       const CSPInstance *instance,
                       const CuttingPattern *pattern)
{
    /* Add new column */
    int col = glp_add_cols(master->lp, 1);
    master->num_cols++;

    /* Set column name */
    char name[64];
    snprintf(name, sizeof(name), "pattern_%d", pattern->pattern_id);
    glp_set_col_name(master->lp, col, name);

    /* Set column bounds: x_j >= 0 (continuous for LP) */
    glp_set_col_bnds(master->lp, col, GLP_LO, 0.0, 0.0);

    /* Set objective coefficient: cost of using this pattern */
    double cost = instance->stocks[pattern->stock_id].cost;
    if (cost <= 0.0) cost = 1.0;  /* Default cost = 1 bar */
    glp_set_obj_coef(master->lp, col, cost);

    /* Count non-zeros: piece cuts + stock constraint (if limited) */
    int nz = 0;
    for (int i = 0; i < instance->num_pieces; i++) {
        if (pattern->cuts[i] > 0) nz++;
    }

    /* Check if this pattern's stock has a quantity constraint */
    int stock_id = pattern->stock_id;
    int stock_row = -1;
    if (instance->stocks[stock_id].quantity >= 0) {
        /* Find the row for this stock */
        int row = master->stock_row_start;
        for (int s = 0; s < stock_id; s++) {
            if (instance->stocks[s].quantity >= 0) {
                row++;
            }
        }
        stock_row = row;
        nz++;  /* Add one more non-zero for stock constraint */
    }

    /* GLPK uses 1-indexed arrays with element 0 unused */
    int *ind = malloc((nz + 1) * sizeof(int));
    double *val = malloc((nz + 1) * sizeof(double));

    int k = 1;  /* Start at 1 */

    /* Piece demand coefficients */
    for (int i = 0; i < instance->num_pieces; i++) {
        if (pattern->cuts[i] > 0) {
            ind[k] = i + 1;  /* GLPK is 1-indexed */
            val[k] = (double)pattern->cuts[i];
            k++;
        }
    }

    /* Stock quantity coefficient: using this pattern consumes 1 bar of this stock */
    if (stock_row > 0) {
        ind[k] = stock_row;
        val[k] = 1.0;
        k++;
    }

    glp_set_mat_col(master->lp, col, nz, ind, val);

    free(ind);
    free(val);
}

int master_solve_lp(MasterProblem *master)
{
    /* Set simplex parameters */
    glp_smcp parm;
    glp_init_smcp(&parm);
    parm.msg_lev = GLP_MSG_OFF;  /* Suppress output */

    /* Solve LP using simplex */
    int ret = glp_simplex(master->lp, &parm);

    if (ret != 0) {
        fprintf(stderr, "  Simplex retourne erreur %d\n", ret);
        return -1;  /* Simplex failed */
    }

    int status = glp_get_status(master->lp);
    if (status == GLP_OPT) {
        master->obj_value = glp_get_obj_val(master->lp);
        return 0;  /* Success */
    } else if (status == GLP_FEAS) {
        master->obj_value = glp_get_obj_val(master->lp);
        return 0;  /* Feasible but not proven optimal */
    }

    /* Diagnostic output for infeasibility */
    fprintf(stderr, "  LP status: %d (OPT=5, FEAS=2, INFEAS=1, NOFEAS=4, UNBND=6)\n", status);
    fprintf(stderr, "  Rows: %d, Cols: %d\n", master->num_rows, master->num_cols);

    /* Show demand constraints */
    fprintf(stderr, "  Contraintes demande:\n");
    for (int i = 1; i <= master->num_pieces; i++) {
        double lb = glp_get_row_lb(master->lp, i);
        fprintf(stderr, "    Piece %d: demande >= %.0f\n", i-1, lb);
    }

    /* Show stock constraints */
    fprintf(stderr, "  Contraintes stock:\n");
    for (int r = master->stock_row_start; r <= master->num_rows; r++) {
        double ub = glp_get_row_ub(master->lp, r);
        const char *name = glp_get_row_name(master->lp, r);
        fprintf(stderr, "    %s: usage <= %.0f\n", name ? name : "?", ub);
    }

    return -1;  /* Infeasible or other failure */
}

void master_get_duals(const MasterProblem *master, double *duals, int32_t num_pieces)
{
    /*
     * Get dual prices (shadow prices) for the demand constraints.
     * These are used in the pricing subproblem to determine the
     * value of each piece type.
     */
    for (int i = 0; i < num_pieces; i++) {
        duals[i] = glp_get_row_dual(master->lp, i + 1);
    }
}

void master_get_primals(const MasterProblem *master, double *primals, int32_t num_patterns)
{
    /* Skip artificial variables (first num_artificial columns) */
    int offset = master->num_artificial;
    for (int j = 0; j < num_patterns; j++) {
        primals[j] = glp_get_col_prim(master->lp, j + 1 + offset);
    }
}

int master_solve_ilp(MasterProblem *master, double time_limit_seconds)
{
    /*
     * Solve as Integer Linear Program.
     * All pattern usage variables must be integers.
     */

    /* Change demand constraints from >= to = to prevent overproduction */
    for (int i = 1; i <= master->num_pieces; i++) {
        double demand = glp_get_row_lb(master->lp, i);
        glp_set_row_bnds(master->lp, i, GLP_FX, demand, demand);
    }

    /* Set all columns to integer type */
    for (int j = 1; j <= master->num_cols; j++) {
        glp_set_col_kind(master->lp, j, GLP_IV);  /* Integer variable */
    }

    /* Set MIP parameters */
    glp_iocp parm;
    glp_init_iocp(&parm);
    parm.msg_lev = GLP_MSG_OFF;
    parm.tm_lim = (int)(time_limit_seconds * 1000);  /* Milliseconds */
    parm.presolve = GLP_ON;

    /* Solve MIP (requires LP relaxation solved first) */
    int ret = glp_intopt(master->lp, &parm);

    if (ret != 0 && ret != GLP_ETMLIM) {
        return -1;  /* MIP failed */
    }

    int status = glp_mip_status(master->lp);
    if (status == GLP_OPT || status == GLP_FEAS) {
        master->obj_value = glp_mip_obj_val(master->lp);
        return 0;
    }

    return -1;
}

void master_get_integer_solution(const MasterProblem *master, int32_t *solution, int32_t num_patterns)
{
    /* Skip artificial variables (first num_artificial columns) */
    int offset = master->num_artificial;
    for (int j = 0; j < num_patterns; j++) {
        double val = glp_mip_col_val(master->lp, j + 1 + offset);
        solution[j] = (int32_t)(val + 0.5);  /* Round to nearest integer */
    }
}

/* Check if any artificial variables are used (indicates true infeasibility) */
double master_get_artificial_usage(const MasterProblem *master)
{
    double total = 0.0;
    for (int j = 1; j <= master->num_artificial; j++) {
        total += glp_mip_col_val(master->lp, j);
    }
    return total;
}

void master_destroy(MasterProblem *master)
{
    if (master) {
        if (master->lp) {
            glp_delete_prob(master->lp);
        }
        free(master);
    }
}
