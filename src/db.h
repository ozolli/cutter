/*
 * db.h - SQLite database for inventory management
 *
 * Manages persistent stock of tubes and offcuts.
 */

#ifndef DB_H
#define DB_H

#include "csp_types.h"
#include <stdbool.h>

/*
 * StockItem: represents an item in the inventory database
 */
typedef struct {
    int64_t id;                     /* Database row ID */
    char label[MAX_LABEL_LEN];      /* Material type label */
    double length;                  /* Length in mm */
    double diameter;                /* Outer diameter in mm (0 if not tube) */
    double thickness;               /* Wall thickness in mm (0 if not tube) */
    int32_t quantity;               /* Available quantity */
    double cost;                    /* Cost per unit */
    MaterialType material;          /* Material type (alu, epoxy) */
    bool is_offcut;                 /* True if this is an offcut (chute) */
    char created_at[32];            /* Creation timestamp */
    char updated_at[32];            /* Last update timestamp */
} StockItem;

/*
 * Enable/disable network-share mode for the database.
 *
 * When enabled, the database is opened with the "unix-dotfile" VFS, which uses
 * a lock file (<db>.lock) instead of POSIX byte-range locks. POSIX locks are
 * unreliable on network filesystems (SMB/CIFS, NFS), which prevents writes;
 * dotfile locking works there and still guards against concurrent writers.
 *
 * Must be called BEFORE db_init(). Default is disabled (local disk).
 */
void db_set_network_mode(bool enabled);

/*
 * Initialize the database (create tables if needed).
 * Default path: ~/.cutter/inventory.db
 *
 * Returns: 0 on success, -1 on error
 */
int db_init(const char *db_path);

/*
 * Close the database connection.
 */
void db_close(void);

/*
 * Add stock to inventory.
 * If an item with same label, length, diameter, thickness, material exists, quantity is added.
 *
 * Parameters:
 *   label     - Material type label (e.g., "Tube_acier")
 *   length    - Length in mm
 *   diameter  - Outer diameter in mm (0 for non-tubes)
 *   thickness - Wall thickness in mm (0 for non-tubes)
 *   quantity  - Quantity to add
 *   cost      - Cost per unit
 *   material  - Material type (alu, epoxy)
 *   is_offcut - True if this is an offcut
 *
 * Returns: item ID on success, -1 on error
 */
int64_t db_add_stock(const char *label, double length, double diameter,
                     double thickness, int32_t quantity, double cost,
                     MaterialType material, bool is_offcut);

/*
 * Remove stock from inventory (after cutting).
 *
 * Returns: 0 on success, -1 on error
 */
int db_remove_stock(int64_t id, int32_t quantity);

/*
 * Update stock quantity directly.
 *
 * Returns: 0 on success, -1 on error
 */
int db_set_stock_quantity(int64_t id, int32_t quantity);

/*
 * Delete a stock item completely.
 *
 * Returns: 0 on success, -1 on error
 */
int db_delete_stock(int64_t id);

/*
 * Update all fields of a stock item.
 *
 * Returns: 0 on success, -1 on error
 */
int db_update_stock(int64_t id, const char *label, double length,
                    double diameter, double thickness, int32_t quantity,
                    double cost, MaterialType material);

/*
 * Clear all stock items from database.
 *
 * Returns: 0 on success, -1 on error
 */
int db_clear_stock(void);

/*
 * List all stock items.
 * Caller must free the returned array.
 *
 * Returns: number of items, or -1 on error
 */
int db_list_stock(StockItem **items, bool include_zero_qty);

/*
 * Get a single stock item by ID.
 *
 * Returns: 0 on success, -1 on error
 */
int db_get_stock(int64_t id, StockItem *item);

/*
 * Load inventory into a CSPInstance for solving.
 * Only loads items with quantity > 0.
 *
 * Returns: number of stock types loaded
 */
int db_load_inventory(CSPInstance *instance);

/*
 * Record a cutting session and update inventory.
 * Decreases used stock and optionally adds new offcuts.
 * If solution is not const, offcut_ids will be populated with the IDs of created offcuts.
 *
 * Returns: 0 on success, -1 on error
 */
int db_record_cutting_session(const CSPInstance *instance,
                              CSPSolution *solution,
                              bool save_usable_offcuts);

/*
 * Get default database path (~/.cutter/inventory.db)
 */
const char *db_default_path(void);

#endif /* DB_H */
