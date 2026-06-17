/*
 * pdf_export.h - PDF export for cutting diagrams
 *
 * Generates visual cutting plans using Cairo PDF surface.
 */

#ifndef PDF_EXPORT_H
#define PDF_EXPORT_H

#include "csp_types.h"
#include "db.h"

/*
 * Export solution as a PDF cutting diagram.
 *
 * Parameters:
 *   filename - Output PDF file path
 *   instance - Problem instance with patterns
 *   solution - Computed solution with pattern usage
 *
 * Returns: 0 on success, -1 on error
 */
int pdf_export_solution(const char *filename,
                        const CSPInstance *instance,
                        const CSPSolution *solution);

/*
 * Export stock inventory as a PDF table (A4 format).
 *
 * Parameters:
 *   filename - Output PDF file path
 *   items    - Array of stock items
 *   count    - Number of items
 *
 * Returns: 0 on success, -1 on error
 */
int pdf_export_stock(const char *filename,
                     const StockItem *items,
                     int count);

#endif /* PDF_EXPORT_H */
