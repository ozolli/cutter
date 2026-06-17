/*
 * csp_types.h - Core data structures for the Cutting Stock Problem
 *
 * This file defines all types used by the column generation solver.
 */

#ifndef CSP_TYPES_H
#define CSP_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include <math.h>

/* Tolerance for floating-point comparison (0.01mm) */
#define DOUBLE_EPSILON 0.01
#define DOUBLE_EQ(a, b) (fabs((a) - (b)) < DOUBLE_EPSILON)

/* Maximum constants */
#define MAX_PIECE_TYPES     1024  /* Support very large cutting lists */
#define MAX_STOCK_TYPES     1024  /* Support very large inventories */
#define MAX_PATTERNS        4096
#define MAX_LABEL_LEN       64

/* Material types */
typedef enum {
    MATERIAL_UNKNOWN = 0,
    MATERIAL_ALU = 1,
    MATERIAL_EPOXY = 2
} MaterialType;

/*
 * PieceDemand: represents a type of piece to cut
 */
typedef struct {
    int32_t id;                     /* Unique identifier */
    double length;                  /* Length in mm */
    double diameter;                /* Outer diameter in mm (0 if not specified) */
    double thickness;               /* Wall thickness in mm (0 if not specified) */
    int32_t quantity;               /* Quantity needed */
    MaterialType material;          /* Material type (alu, epoxy) */
    char label[MAX_LABEL_LEN];      /* Human-readable label */
} PieceDemand;

/*
 * StockBar: represents available raw material
 */
typedef struct {
    int32_t id;                     /* Unique identifier (also used for DB ID) */
    double length;                  /* Length in mm */
    double diameter;                /* Outer diameter in mm (0 if not tube) */
    double thickness;               /* Wall thickness in mm (0 if not tube) */
    int32_t quantity;               /* Quantity available (-1 for unlimited) */
    double cost;                    /* Cost per bar (for optimization objective) */
    MaterialType material;          /* Material type (alu, epoxy) */
    bool is_offcut;                 /* True if this is a reusable offcut (chute) */
    char label[MAX_LABEL_LEN];      /* Human-readable label */
} StockBar;

/*
 * CuttingPattern: describes how to cut one stock bar
 */
typedef struct {
    int32_t pattern_id;             /* Unique pattern identifier */
    int32_t stock_id;               /* Which stock bar type this pattern uses */
    int32_t cuts[MAX_PIECE_TYPES];  /* Number of each piece type in pattern */
    double waste;                   /* Waste (chute) in mm */
    double reduced_cost;            /* Reduced cost from pricing problem */
} CuttingPattern;

/*
 * CSPInstance: complete problem definition
 */
typedef struct {
    /* Pieces to cut */
    PieceDemand pieces[MAX_PIECE_TYPES];
    int32_t num_pieces;

    /* Available stock */
    StockBar stocks[MAX_STOCK_TYPES];
    int32_t num_stocks;

    /* Generated patterns */
    CuttingPattern patterns[MAX_PATTERNS];
    int32_t num_patterns;

    /* Parameters */
    double saw_kerf;                /* Saw blade width (trait de scie) in mm */
    double min_usable_offcut;       /* Minimum useful offcut (chute utile) in mm */
} CSPInstance;

/*
 * CSPSolution: result of the optimization
 */
typedef struct {
    int32_t pattern_usage[MAX_PATTERNS]; /* How many times each pattern is used */
    int64_t offcut_ids[MAX_PATTERNS];    /* DB ID of offcut for each pattern (0 if none) */
    int32_t num_bars_used;               /* Total number of bars cut */
    double total_waste;                  /* Total waste in mm */
    double total_length_used;            /* Total stock length consumed */
    double efficiency;                   /* Material efficiency percentage */
    bool is_optimal;                     /* Whether solution is proven optimal */
    double lp_bound;                     /* LP relaxation lower bound */
} CSPSolution;

/*
 * CSPParameters: algorithm parameters
 */
typedef struct {
    int32_t max_iterations;         /* Max column generation iterations */
    double epsilon;                 /* Tolerance for reduced cost */
    double time_limit_seconds;      /* Time limit for ILP solve */
    bool use_branch_and_bound;      /* Use B&B for final integer solution */
    bool verbose;                   /* Print progress */
} CSPParameters;

/* Default parameters */
static inline CSPParameters csp_default_params(void) {
    return (CSPParameters){
        .max_iterations = 1000,
        .epsilon = 1e-6,
        .time_limit_seconds = 60.0,
        .use_branch_and_bound = true,
        .verbose = false
    };
}

/* Initialize an empty instance */
static inline void csp_instance_init(CSPInstance *inst) {
    inst->num_pieces = 0;
    inst->num_stocks = 0;
    inst->num_patterns = 0;
    inst->saw_kerf = 5.0;           /* Default 5mm */
    inst->min_usable_offcut = 100.0; /* Default 100mm */
}

/* Find existing piece with same dimensions (for merging) */
static inline int csp_find_piece(const CSPInstance *inst, double length,
                                  double diameter, double thickness,
                                  MaterialType material) {
    for (int i = 0; i < inst->num_pieces; i++) {
        const PieceDemand *p = &inst->pieces[i];
        if (DOUBLE_EQ(p->length, length) &&
            DOUBLE_EQ(p->diameter, diameter) &&
            DOUBLE_EQ(p->thickness, thickness) &&
            p->material == material) {
            return i;
        }
    }
    return -1;
}

/* Add a piece demand to the instance (simple version without dimensions)
 * Merges with existing piece if dimensions match */
static inline int csp_add_piece(CSPInstance *inst, double length,
                                 int32_t quantity, const char *label) {
    /* Check for existing piece with same dimensions */
    int existing = csp_find_piece(inst, length, 0, 0, MATERIAL_UNKNOWN);
    if (existing >= 0) {
        inst->pieces[existing].quantity += quantity;
        return existing;
    }

    if (inst->num_pieces >= MAX_PIECE_TYPES) return -1;

    PieceDemand *p = &inst->pieces[inst->num_pieces];
    p->id = inst->num_pieces;
    p->length = length;
    p->diameter = 0;
    p->thickness = 0;
    p->material = MATERIAL_UNKNOWN;
    p->quantity = quantity;
    if (label) {
        snprintf(p->label, MAX_LABEL_LEN, "%s", label);
    } else {
        snprintf(p->label, MAX_LABEL_LEN, "Piece_%d", p->id);
    }

    return inst->num_pieces++;
}

/* Add a piece demand with tube dimensions and material
 * Merges with existing piece if dimensions AND material match */
static inline int csp_add_piece_tube(CSPInstance *inst, double length,
                                      double diameter, double thickness,
                                      int32_t quantity, MaterialType material,
                                      const char *label) {
    /* Check for existing piece with same dimensions AND material */
    int existing = csp_find_piece(inst, length, diameter, thickness, material);
    if (existing >= 0) {
        inst->pieces[existing].quantity += quantity;
        return existing;
    }

    if (inst->num_pieces >= MAX_PIECE_TYPES) return -1;

    PieceDemand *p = &inst->pieces[inst->num_pieces];
    p->id = inst->num_pieces;
    p->length = length;
    p->diameter = diameter;
    p->thickness = thickness;
    p->material = material;
    p->quantity = quantity;
    if (label) {
        snprintf(p->label, MAX_LABEL_LEN, "%s", label);
    } else {
        snprintf(p->label, MAX_LABEL_LEN, "Piece_%d", p->id);
    }

    return inst->num_pieces++;
}

/* Add a stock bar type to the instance (simple version) */
static inline int csp_add_stock(CSPInstance *inst, double length,
                                 int32_t quantity, double cost, const char *label) {
    if (inst->num_stocks >= MAX_STOCK_TYPES) return -1;

    StockBar *s = &inst->stocks[inst->num_stocks];
    s->id = inst->num_stocks;
    s->length = length;
    s->diameter = 0;
    s->thickness = 0;
    s->quantity = quantity;
    s->cost = cost;
    s->is_offcut = false;
    if (label) {
        snprintf(s->label, MAX_LABEL_LEN, "%s", label);
    } else {
        snprintf(s->label, MAX_LABEL_LEN, "Stock_%d", s->id);
    }

    return inst->num_stocks++;
}

/* Add a stock bar with tube dimensions */
static inline int csp_add_stock_tube(CSPInstance *inst, double length,
                                      double diameter, double thickness,
                                      int32_t quantity, double cost, const char *label) {
    if (inst->num_stocks >= MAX_STOCK_TYPES) return -1;

    StockBar *s = &inst->stocks[inst->num_stocks];
    s->id = inst->num_stocks;
    s->length = length;
    s->diameter = diameter;
    s->thickness = thickness;
    s->quantity = quantity;
    s->cost = cost;
    s->is_offcut = false;
    if (label) {
        snprintf(s->label, MAX_LABEL_LEN, "%s", label);
    } else {
        snprintf(s->label, MAX_LABEL_LEN, "Tube_D%.0fx%.1f", diameter, thickness);
    }

    return inst->num_stocks++;
}

/* Helper function to convert material type to string */
static inline const char *material_to_string(MaterialType mat) {
    switch (mat) {
        case MATERIAL_ALU: return "Alu";
        case MATERIAL_EPOXY: return "Epoxy";
        default: return "-";
    }
}

/* Helper function to parse material from string */
static inline MaterialType material_from_string(const char *str) {
    if (!str) return MATERIAL_UNKNOWN;
    /* Case-insensitive comparison */
    if (strcasecmp(str, "alu") == 0 || strcasecmp(str, "aluminium") == 0) return MATERIAL_ALU;
    if (strcasecmp(str, "epoxy") == 0) return MATERIAL_EPOXY;
    return MATERIAL_UNKNOWN;
}

/* Check if a piece is compatible with a stock (same dimensions, material, and length fits) */
static inline bool csp_piece_fits_stock(const PieceDemand *piece, const StockBar *stock) {
    /* Length check */
    if (piece->length > stock->length) return false;

    /* If piece has no dimension constraint, it fits any stock (for backward compatibility) */
    if (piece->diameter == 0 && piece->thickness == 0 && piece->material == MATERIAL_UNKNOWN) {
        return true;
    }

    /* Check material compatibility if piece specifies a material */
    if (piece->material != MATERIAL_UNKNOWN && stock->material != MATERIAL_UNKNOWN) {
        if (piece->material != stock->material) return false;
    }

    /* If piece has dimension constraints, they must match */
    if (piece->diameter != 0 || piece->thickness != 0) {
        if (!DOUBLE_EQ(piece->diameter, stock->diameter) || !DOUBLE_EQ(piece->thickness, stock->thickness)) {
            return false;
        }
    }

    return true;
}

#endif /* CSP_TYPES_H */
