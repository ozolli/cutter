/*
 * db.c - SQLite database implementation for inventory management
 */

#include "db.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

static sqlite3 *db = NULL;
static char default_db_path[512] = "";
static bool network_mode = false;  /* use a network-safe VFS (SMB/NFS) */

void db_set_network_mode(bool enabled)
{
    network_mode = enabled;
}

/* Get or create default database path */
const char *db_default_path(void)
{
    if (default_db_path[0] == '\0') {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(default_db_path, sizeof(default_db_path),
                     "%s/.cutter/inventory.db", home);
        } else {
            snprintf(default_db_path, sizeof(default_db_path),
                     "./inventory.db");
        }
    }
    return default_db_path;
}

/* Create directory if it doesn't exist */
static int ensure_dir(const char *path)
{
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", path);

    /* Find last slash */
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "Erreur: Impossible de creer %s: %s\n",
                    dir, strerror(errno));
            return -1;
        }
    }
    return 0;
}

int db_init(const char *db_path)
{
    if (db) {
        return 0;  /* Already initialized */
    }

    const char *path = db_path ? db_path : db_default_path();

    /* Ensure directory exists */
    if (ensure_dir(path) != 0) {
        return -1;
    }

    /* Open database. On a network share, use the "unix-dotfile" VFS: POSIX
     * byte-range locks (the default) are unreliable on SMB/CIFS/NFS and block
     * writes, whereas dotfile locking works there. WAL mode is NOT usable on a
     * network share, so we keep the default rollback journal. */
    int open_flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    const char *vfs = network_mode ? "unix-dotfile" : NULL;
    int rc = sqlite3_open_v2(path, &db, open_flags, vfs);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Erreur SQLite: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        db = NULL;
        return -1;
    }

    /* Busy timeout: longer on a network share to absorb latency */
    sqlite3_busy_timeout(db, network_mode ? 15000 : 5000);

    /* Create tables (without material column for compatibility with existing DBs) */
    const char *create_sql =
        "CREATE TABLE IF NOT EXISTS stock ("
        "  id INTEGER PRIMARY KEY,"
        "  label TEXT NOT NULL,"
        "  length REAL NOT NULL,"
        "  diameter REAL NOT NULL DEFAULT 0,"
        "  thickness REAL NOT NULL DEFAULT 0,"
        "  quantity INTEGER NOT NULL DEFAULT 0,"
        "  cost REAL NOT NULL DEFAULT 1.0,"
        "  is_offcut INTEGER NOT NULL DEFAULT 0,"
        "  created_at TEXT NOT NULL,"
        "  updated_at TEXT NOT NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_stock_label ON stock(label);"
        "CREATE INDEX IF NOT EXISTS idx_stock_length ON stock(length);"
        "CREATE INDEX IF NOT EXISTS idx_stock_diameter ON stock(diameter);";

    char *err_msg = NULL;
    rc = sqlite3_exec(db, create_sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Erreur SQL: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        db = NULL;
        return -1;
    }

    /* Add material column if it doesn't exist (migration for existing DBs) */
    const char *alter_sql = "ALTER TABLE stock ADD COLUMN material INTEGER NOT NULL DEFAULT 0;";
    sqlite3_exec(db, alter_sql, NULL, NULL, NULL);  /* Ignore error if column exists */

    /* Create material index (after column is ensured to exist) */
    sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS idx_stock_material ON stock(material);", NULL, NULL, NULL);

    return 0;
}

void db_close(void)
{
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }
}

static void get_timestamp(char *buf, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm);
}

int64_t db_add_stock(const char *label, double length, double diameter,
                     double thickness, int32_t quantity, double cost,
                     MaterialType material, bool is_offcut)
{
    if (!db) {
        fprintf(stderr, "Erreur: Base de donnees non initialisee\n");
        return -1;
    }

    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));

    /* Try to update existing entry first */
    const char *update_sql =
        "UPDATE stock SET quantity = quantity + ?, cost = ?, "
        "is_offcut = ?, updated_at = ? "
        "WHERE label = ? AND length = ? AND diameter = ? AND thickness = ? AND material = ?";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, update_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Erreur SQL: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, quantity);
    sqlite3_bind_double(stmt, 2, cost);
    sqlite3_bind_int(stmt, 3, is_offcut ? 1 : 0);
    sqlite3_bind_text(stmt, 4, timestamp, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, label, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 6, length);
    sqlite3_bind_double(stmt, 7, diameter);
    sqlite3_bind_double(stmt, 8, thickness);
    sqlite3_bind_int(stmt, 9, (int)material);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE && sqlite3_changes(db) > 0) {
        /* Updated existing entry, get its ID */
        const char *select_sql =
            "SELECT id FROM stock WHERE label = ? AND length = ? "
            "AND diameter = ? AND thickness = ? AND material = ?";
        rc = sqlite3_prepare_v2(db, select_sql, -1, &stmt, NULL);
        sqlite3_bind_text(stmt, 1, label, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 2, length);
        sqlite3_bind_double(stmt, 3, diameter);
        sqlite3_bind_double(stmt, 4, thickness);
        sqlite3_bind_int(stmt, 5, (int)material);

        int64_t id = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            id = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return id;
    }

    /* Insert new entry */
    const char *insert_sql =
        "INSERT INTO stock (label, length, diameter, thickness, quantity, cost, "
        "material, is_offcut, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Erreur SQL: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, label, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, length);
    sqlite3_bind_double(stmt, 3, diameter);
    sqlite3_bind_double(stmt, 4, thickness);
    sqlite3_bind_int(stmt, 5, quantity);
    sqlite3_bind_double(stmt, 6, cost);
    sqlite3_bind_int(stmt, 7, (int)material);
    sqlite3_bind_int(stmt, 8, is_offcut ? 1 : 0);
    sqlite3_bind_text(stmt, 9, timestamp, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, timestamp, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Erreur SQL: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    return sqlite3_last_insert_rowid(db);
}

int db_remove_stock(int64_t id, int32_t quantity)
{
    if (!db) return -1;

    /* First, get current quantity and check if it's an offcut */
    const char *check_sql = "SELECT quantity, is_offcut FROM stock WHERE id = ?";
    sqlite3_stmt *check_stmt;
    int rc = sqlite3_prepare_v2(db, check_sql, -1, &check_stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Erreur SQL: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_int64(check_stmt, 1, id);

    int current_qty = 0;
    int is_offcut = 0;
    if (sqlite3_step(check_stmt) == SQLITE_ROW) {
        current_qty = sqlite3_column_int(check_stmt, 0);
        is_offcut = sqlite3_column_int(check_stmt, 1);
    }
    sqlite3_finalize(check_stmt);

    /* Calculate new quantity */
    int new_qty = current_qty - quantity;
    if (new_qty < 0) new_qty = 0;

    /* If it's an offcut and quantity becomes 0, delete it */
    if (is_offcut && new_qty == 0) {
        return db_delete_stock(id);
    }

    /* Otherwise, update quantity */
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));

    const char *sql =
        "UPDATE stock SET quantity = ?, updated_at = ? WHERE id = ?";

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Erreur SQL: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, new_qty);
    sqlite3_bind_text(stmt, 2, timestamp, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_set_stock_quantity(int64_t id, int32_t quantity)
{
    if (!db) return -1;

    /* If quantity is 0, check if item is an offcut and delete it */
    if (quantity == 0) {
        const char *check_sql = "SELECT is_offcut FROM stock WHERE id = ?";
        sqlite3_stmt *check_stmt;
        int rc = sqlite3_prepare_v2(db, check_sql, -1, &check_stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int64(check_stmt, 1, id);
            if (sqlite3_step(check_stmt) == SQLITE_ROW) {
                int is_offcut = sqlite3_column_int(check_stmt, 0);
                sqlite3_finalize(check_stmt);

                /* Delete offcuts with quantity 0 */
                if (is_offcut) {
                    return db_delete_stock(id);
                }
            } else {
                sqlite3_finalize(check_stmt);
            }
        }
    }

    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));

    const char *sql = "UPDATE stock SET quantity = ?, updated_at = ? WHERE id = ?";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, quantity);
    sqlite3_bind_text(stmt, 2, timestamp, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_delete_stock(int64_t id)
{
    if (!db) return -1;

    const char *sql = "DELETE FROM stock WHERE id = ?";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int64(stmt, 1, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return -1;
    /* Report failure if no row matched (avoids a misleading success message) */
    return (sqlite3_changes(db) > 0) ? 0 : -1;
}

int db_clear_stock(void)
{
    if (!db) return -1;

    const char *sql = "DELETE FROM stock";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_update_stock(int64_t id, const char *label, double length,
                    double diameter, double thickness, int32_t quantity,
                    double cost, MaterialType material)
{
    if (!db) return -1;

    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));

    const char *sql =
        "UPDATE stock SET label = ?, length = ?, diameter = ?, thickness = ?, "
        "quantity = ?, cost = ?, material = ?, updated_at = ? WHERE id = ?";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Erreur SQL: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, label, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, length);
    sqlite3_bind_double(stmt, 3, diameter);
    sqlite3_bind_double(stmt, 4, thickness);
    sqlite3_bind_int(stmt, 5, quantity);
    sqlite3_bind_double(stmt, 6, cost);
    sqlite3_bind_int(stmt, 7, (int)material);
    sqlite3_bind_text(stmt, 8, timestamp, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 9, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_list_stock(StockItem **items, bool include_zero_qty)
{
    if (!db) return -1;

    const char *sql = include_zero_qty
        ? "SELECT id, label, length, diameter, thickness, quantity, cost, material, is_offcut, "
          "created_at, updated_at FROM stock ORDER BY material, diameter, thickness, is_offcut, length DESC"
        : "SELECT id, label, length, diameter, thickness, quantity, cost, material, is_offcut, "
          "created_at, updated_at FROM stock WHERE quantity != 0 "
          "ORDER BY material, diameter, thickness, is_offcut, length DESC";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Erreur SQL: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    /* Count rows first */
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        count++;
    }
    sqlite3_reset(stmt);

    if (count == 0) {
        sqlite3_finalize(stmt);
        *items = NULL;
        return 0;
    }

    /* Allocate and fill array */
    *items = calloc(count, sizeof(StockItem));
    if (!*items) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < count) {
        StockItem *item = &(*items)[i];
        item->id = sqlite3_column_int64(stmt, 0);
        snprintf(item->label, sizeof(item->label), "%s",
                 (const char *)sqlite3_column_text(stmt, 1));
        item->length = sqlite3_column_double(stmt, 2);
        item->diameter = sqlite3_column_double(stmt, 3);
        item->thickness = sqlite3_column_double(stmt, 4);
        item->quantity = sqlite3_column_int(stmt, 5);
        item->cost = sqlite3_column_double(stmt, 6);
        item->material = (MaterialType)sqlite3_column_int(stmt, 7);
        item->is_offcut = sqlite3_column_int(stmt, 8) != 0;
        snprintf(item->created_at, sizeof(item->created_at), "%s",
                 (const char *)sqlite3_column_text(stmt, 9));
        snprintf(item->updated_at, sizeof(item->updated_at), "%s",
                 (const char *)sqlite3_column_text(stmt, 10));
        i++;
    }

    sqlite3_finalize(stmt);
    return count;
}

int db_get_stock(int64_t id, StockItem *item)
{
    if (!db || !item) return -1;

    const char *sql =
        "SELECT id, label, length, diameter, thickness, quantity, cost, material, is_offcut, "
        "created_at, updated_at FROM stock WHERE id = ?";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int64(stmt, 1, id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    item->id = sqlite3_column_int64(stmt, 0);
    snprintf(item->label, sizeof(item->label), "%s",
             (const char *)sqlite3_column_text(stmt, 1));
    item->length = sqlite3_column_double(stmt, 2);
    item->diameter = sqlite3_column_double(stmt, 3);
    item->thickness = sqlite3_column_double(stmt, 4);
    item->quantity = sqlite3_column_int(stmt, 5);
    item->cost = sqlite3_column_double(stmt, 6);
    item->material = (MaterialType)sqlite3_column_int(stmt, 7);
    item->is_offcut = sqlite3_column_int(stmt, 8) != 0;
    snprintf(item->created_at, sizeof(item->created_at), "%s",
             (const char *)sqlite3_column_text(stmt, 9));
    snprintf(item->updated_at, sizeof(item->updated_at), "%s",
             (const char *)sqlite3_column_text(stmt, 10));

    sqlite3_finalize(stmt);
    return 0;
}

int db_load_inventory(CSPInstance *instance)
{
    StockItem *items = NULL;
    int count = db_list_stock(&items, false);

    if (count < 0) return -1;
    if (count == 0) return 0;

    int loaded = 0;
    for (int i = 0; i < count && loaded < MAX_STOCK_TYPES; i++) {
        /* Skip stocks with quantity 0 - they can't be used (but -1 = unlimited) */
        if (items[i].quantity == 0) continue;

        /* Cost for the optimiser's objective.
         *
         * Offcut priority (consume offcuts smallest-first, before any fresh bar)
         * is enforced by a dedicated greedy pass in colgen_solve(), driven by the
         * is_offcut flag below - NOT by this cost. The cost here only matters for
         * fresh bars, which the solver allocates to the remaining demand. */
        double cost;
        if (items[i].is_offcut) {
            cost = items[i].length / 6000.0;
            if (cost < 0.01) cost = 0.01;  /* Minimum cost */
        } else {
            cost = items[i].cost;
            if (cost < 1.0) cost = 1.0;
        }

        int idx;
        /* Use csp_add_stock_tube if dimensions are present */
        if (items[i].diameter > 0 || items[i].thickness > 0) {
            idx = csp_add_stock_tube(instance, items[i].length,
                                     items[i].diameter, items[i].thickness,
                                     items[i].quantity, cost,
                                     items[i].label);
        } else {
            idx = csp_add_stock(instance, items[i].length, items[i].quantity,
                                cost, items[i].label);
        }
        if (idx >= 0) {
            /* We need to track the DB ID, material and offcut flag */
            instance->stocks[idx].id = (int32_t)items[i].id;
            instance->stocks[idx].material = items[i].material;
            instance->stocks[idx].is_offcut = items[i].is_offcut;
            loaded++;
        }
    }

    free(items);
    return loaded;
}

int db_record_cutting_session(const CSPInstance *instance,
                              CSPSolution *solution,
                              bool save_usable_offcuts)
{
    if (!db) return -1;

    /* Begin transaction */
    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);

    /* For each pattern used, decrease stock */
    for (int j = 0; j < instance->num_patterns; j++) {
        int usage = solution->pattern_usage[j];
        if (usage <= 0) continue;

        const CuttingPattern *pattern = &instance->patterns[j];
        int stock_idx = pattern->stock_id;
        int64_t db_id = instance->stocks[stock_idx].id;

        if (db_id > 0) {
            /* Decrease stock quantity */
            db_remove_stock(db_id, usage);
        }

        /* Optionally save usable offcuts */
        if (save_usable_offcuts && pattern->waste >= instance->min_usable_offcut) {
            char offcut_label[MAX_LABEL_LEN];
            const StockBar *stock = &instance->stocks[stock_idx];

            /* Use original bar label with "Chute " prefix (avoid double prefix) */
            if (strncmp(stock->label, "Chute ", 6) == 0) {
                snprintf(offcut_label, sizeof(offcut_label), "%s", stock->label);
            } else {
                snprintf(offcut_label, sizeof(offcut_label), "Chute %s", stock->label);
            }

            /* Add offcuts to inventory (inherit diameter/thickness/material from parent stock)
             * Cost = length / 6000 (e.g., 2500mm -> 0.42) */
            double offcut_cost = pattern->waste / 6000.0;
            int64_t offcut_id = db_add_stock(offcut_label, pattern->waste, stock->diameter,
                         stock->thickness, usage, offcut_cost, stock->material, true);

            /* Store offcut ID in solution */
            if (solution) {
                solution->offcut_ids[j] = offcut_id;
            }
        }
    }

    /* Commit transaction */
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);

    return 0;
}
