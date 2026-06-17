/*
 * csv_io.h - CSV import/export for cutting stock data
 *
 * File formats:
 *
 * Pieces CSV (pieces.csv):
 *   label,length,quantity
 *   Pied_2200,2200,5
 *   Traverse_1500,1500,10
 *
 * Stock CSV (stock.csv):
 *   label,length,quantity,cost
 *   Barre_6m,6000,-1,1.0
 *   Chute_2m,2000,3,0.4
 */

#ifndef CSV_IO_H
#define CSV_IO_H

#include "csp_types.h"

/*
 * Load pieces from a CSV file.
 * Returns: number of pieces loaded, or -1 on error
 */
int csv_load_pieces(const char *filename, CSPInstance *instance);

/*
 * Load pieces from an Odoo production order CSV.
 * Format: "Source","Quantité à produire"
 * Extracts material (alu/epoxy), diameter, thickness, length from Source.
 * Returns: number of pieces loaded, or -1 on error
 */
int csv_load_pieces_odoo(const char *filename, CSPInstance *instance);

/*
 * Load stock from a CSV file.
 * Returns: number of stock types loaded, or -1 on error
 */
int csv_load_stock(const char *filename, CSPInstance *instance);

#endif /* CSV_IO_H */
