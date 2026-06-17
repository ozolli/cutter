/*
 * stock_object.h - GObject wrapper for StockItem
 */

#ifndef STOCK_OBJECT_H
#define STOCK_OBJECT_H

#include <glib-object.h>
#include "../../db.h"

G_BEGIN_DECLS

#define CUTTER_TYPE_STOCK_OBJECT (stock_object_get_type())
G_DECLARE_FINAL_TYPE(StockObject, stock_object, CUTTER, STOCK_OBJECT, GObject)

StockObject *stock_object_new(const StockItem *item);
StockObject *stock_object_new_empty(void);

/* Getters */
int64_t stock_object_get_id(StockObject *self);
const char *stock_object_get_label(StockObject *self);
double stock_object_get_length(StockObject *self);
double stock_object_get_diameter(StockObject *self);
double stock_object_get_thickness(StockObject *self);
int32_t stock_object_get_quantity(StockObject *self);
double stock_object_get_cost(StockObject *self);
MaterialType stock_object_get_material(StockObject *self);
gboolean stock_object_get_is_offcut(StockObject *self);

/* Setters */
void stock_object_set_label(StockObject *self, const char *label);
void stock_object_set_length(StockObject *self, double length);
void stock_object_set_diameter(StockObject *self, double diameter);
void stock_object_set_thickness(StockObject *self, double thickness);
void stock_object_set_quantity(StockObject *self, int32_t quantity);
void stock_object_set_cost(StockObject *self, double cost);
void stock_object_set_material(StockObject *self, MaterialType material);
void stock_object_set_is_offcut(StockObject *self, gboolean is_offcut);

/* Convert to StockItem struct */
void stock_object_to_item(StockObject *self, StockItem *out);

G_END_DECLS

#endif /* STOCK_OBJECT_H */
