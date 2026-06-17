/*
 * ods_io.h - ODS (OpenDocument Spreadsheet) import/export for stock
 */

#ifndef ODS_IO_H
#define ODS_IO_H

#include "csp_types.h"
#include "db.h"

/*
 * Import stock from ODS file
 *
 * Format attendu:
 * - Colonne A: ID (ignoré à l'import)
 * - Colonne B: Label
 * - Colonne C: Matière (Alu/Epoxy)
 * - Colonne D: Longueur (mm)
 * - Colonne E: Diamètre (mm)
 * - Colonne F: Epaisseur (mm)
 * - Colonne G: Quantité
 * - Colonne H: Coût (ignoré, recalculé)
 * - Colonne I: Type (Stock/Chute)
 *
 * Retourne le nombre d'éléments importés, ou -1 en cas d'erreur
 */
int ods_import_stock(const char *filename);

/*
 * Export stock to ODS file
 *
 * Exporte tout le stock de la base de données vers un fichier ODS
 * avec le même format que l'import
 *
 * Retourne 0 en cas de succès, -1 en cas d'erreur
 */
int ods_export_stock(const char *filename);

#endif /* ODS_IO_H */
