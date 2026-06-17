/*
 * inventory_view.h - Inventory management view
 */

#ifndef INVENTORY_VIEW_H
#define INVENTORY_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CUTTER_TYPE_INVENTORY_VIEW (inventory_view_get_type())
G_DECLARE_FINAL_TYPE(InventoryView, inventory_view, CUTTER, INVENTORY_VIEW, GtkBox)

GtkWidget *inventory_view_new(void);
void inventory_view_refresh(InventoryView *self);

G_END_DECLS

#endif /* INVENTORY_VIEW_H */
