/*
 * inventory_view.c - Inventory management view with GtkColumnView
 */

#include "inventory_view.h"
#include "../models/stock_object.h"
#include "../../db.h"
#include "../../pdf_export.h"
#include "../../ods_io.h"

/* Comparison functions for sorting */
static int compare_id(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    StockObject *obj_a = CUTTER_STOCK_OBJECT((gpointer)a);
    StockObject *obj_b = CUTTER_STOCK_OBJECT((gpointer)b);
    int64_t id_a = stock_object_get_id(obj_a);
    int64_t id_b = stock_object_get_id(obj_b);
    return (id_a > id_b) - (id_a < id_b);
}

static int compare_label(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    StockObject *obj_a = CUTTER_STOCK_OBJECT((gpointer)a);
    StockObject *obj_b = CUTTER_STOCK_OBJECT((gpointer)b);
    return g_strcmp0(stock_object_get_label(obj_a), stock_object_get_label(obj_b));
}

static int compare_material(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    StockObject *obj_a = CUTTER_STOCK_OBJECT((gpointer)a);
    StockObject *obj_b = CUTTER_STOCK_OBJECT((gpointer)b);
    int mat_a = (int)stock_object_get_material(obj_a);
    int mat_b = (int)stock_object_get_material(obj_b);
    return mat_a - mat_b;
}

static int compare_length(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    StockObject *obj_a = CUTTER_STOCK_OBJECT((gpointer)a);
    StockObject *obj_b = CUTTER_STOCK_OBJECT((gpointer)b);
    double len_a = stock_object_get_length(obj_a);
    double len_b = stock_object_get_length(obj_b);
    return (len_a > len_b) - (len_a < len_b);
}

static int compare_diameter(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    StockObject *obj_a = CUTTER_STOCK_OBJECT((gpointer)a);
    StockObject *obj_b = CUTTER_STOCK_OBJECT((gpointer)b);
    double d_a = stock_object_get_diameter(obj_a);
    double d_b = stock_object_get_diameter(obj_b);
    return (d_a > d_b) - (d_a < d_b);
}

static int compare_thickness(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    StockObject *obj_a = CUTTER_STOCK_OBJECT((gpointer)a);
    StockObject *obj_b = CUTTER_STOCK_OBJECT((gpointer)b);
    double t_a = stock_object_get_thickness(obj_a);
    double t_b = stock_object_get_thickness(obj_b);
    return (t_a > t_b) - (t_a < t_b);
}

static int compare_quantity(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    StockObject *obj_a = CUTTER_STOCK_OBJECT((gpointer)a);
    StockObject *obj_b = CUTTER_STOCK_OBJECT((gpointer)b);
    int q_a = stock_object_get_quantity(obj_a);
    int q_b = stock_object_get_quantity(obj_b);
    return q_a - q_b;
}

static int compare_cost(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    StockObject *obj_a = CUTTER_STOCK_OBJECT((gpointer)a);
    StockObject *obj_b = CUTTER_STOCK_OBJECT((gpointer)b);
    double c_a = stock_object_get_cost(obj_a);
    double c_b = stock_object_get_cost(obj_b);
    return (c_a > c_b) - (c_a < c_b);
}

static int compare_type(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    StockObject *obj_a = CUTTER_STOCK_OBJECT((gpointer)a);
    StockObject *obj_b = CUTTER_STOCK_OBJECT((gpointer)b);
    int t_a = stock_object_get_is_offcut(obj_a) ? 1 : 0;
    int t_b = stock_object_get_is_offcut(obj_b) ? 1 : 0;
    return t_a - t_b;
}

struct _InventoryView {
    GtkBox parent_instance;

    GtkWidget *column_view;
    GtkWidget *add_button;
    GtkWidget *status_label;

    GListStore *store;
    GtkSortListModel *sort_model;
    GtkSingleSelection *selection;
};

G_DEFINE_TYPE(InventoryView, inventory_view, GTK_TYPE_BOX)

static void setup_label_factory(GtkSignalListItemFactory *factory,
                                 GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_list_item_set_child(list_item, label);
}

static void bind_id_factory(GtkSignalListItemFactory *factory,
                             GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    StockObject *obj = gtk_list_item_get_item(list_item);
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", stock_object_get_id(obj));
    gtk_label_set_text(GTK_LABEL(label), buf);
}

static void bind_label_factory(GtkSignalListItemFactory *factory,
                                GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    StockObject *obj = gtk_list_item_get_item(list_item);
    gtk_label_set_text(GTK_LABEL(label), stock_object_get_label(obj));
}

static void bind_length_factory(GtkSignalListItemFactory *factory,
                                 GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    StockObject *obj = gtk_list_item_get_item(list_item);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f mm", stock_object_get_length(obj));
    gtk_label_set_text(GTK_LABEL(label), buf);
}

static void bind_diameter_factory(GtkSignalListItemFactory *factory,
                                   GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    StockObject *obj = gtk_list_item_get_item(list_item);
    double d = stock_object_get_diameter(obj);
    if (d > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f mm", d);
        gtk_label_set_text(GTK_LABEL(label), buf);
    } else {
        gtk_label_set_text(GTK_LABEL(label), "-");
    }
}

static void bind_thickness_factory(GtkSignalListItemFactory *factory,
                                    GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    StockObject *obj = gtk_list_item_get_item(list_item);
    double t = stock_object_get_thickness(obj);
    if (t > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f mm", t);
        gtk_label_set_text(GTK_LABEL(label), buf);
    } else {
        gtk_label_set_text(GTK_LABEL(label), "-");
    }
}

static void bind_quantity_factory(GtkSignalListItemFactory *factory,
                                   GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    StockObject *obj = gtk_list_item_get_item(list_item);
    int qty = stock_object_get_quantity(obj);
    if (qty < 0) {
        gtk_label_set_text(GTK_LABEL(label), "∞");
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", qty);
        gtk_label_set_text(GTK_LABEL(label), buf);
    }
}

static void bind_cost_factory(GtkSignalListItemFactory *factory,
                               GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    StockObject *obj = gtk_list_item_get_item(list_item);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", stock_object_get_cost(obj));
    gtk_label_set_text(GTK_LABEL(label), buf);
}

static void bind_material_factory(GtkSignalListItemFactory *factory,
                                   GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    StockObject *obj = gtk_list_item_get_item(list_item);
    gtk_label_set_text(GTK_LABEL(label), material_to_string(stock_object_get_material(obj)));
}

static void bind_type_factory(GtkSignalListItemFactory *factory,
                               GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    StockObject *obj = gtk_list_item_get_item(list_item);
    gtk_label_set_text(GTK_LABEL(label),
                       stock_object_get_is_offcut(obj) ? "Chute" : "Stock");
}

/* Delete button factory */
static void on_delete_row_clicked(GtkButton *button, gpointer user_data)
{
    (void)user_data;
    int64_t *id_ptr = g_object_get_data(G_OBJECT(button), "stock_id");
    InventoryView *view = g_object_get_data(G_OBJECT(button), "view");

    if (id_ptr && *id_ptr > 0 && view) {
        db_delete_stock(*id_ptr);
        inventory_view_refresh(view);
    }
}

static void setup_delete_factory(GtkSignalListItemFactory *factory,
                                  GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    InventoryView *view = CUTTER_INVENTORY_VIEW(user_data);

    GtkWidget *button = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_add_css_class(button, "flat");
    gtk_widget_add_css_class(button, "circular");
    gtk_widget_set_tooltip_text(button, "Supprimer");
    g_object_set_data(G_OBJECT(button), "view", view);
    g_signal_connect(button, "clicked", G_CALLBACK(on_delete_row_clicked), NULL);
    gtk_list_item_set_child(list_item, button);
}

static void bind_delete_factory(GtkSignalListItemFactory *factory,
                                 GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *button = gtk_list_item_get_child(list_item);
    StockObject *obj = gtk_list_item_get_item(list_item);
    int64_t id = stock_object_get_id(obj);

    /* Store ID as allocated int64_t to avoid truncation */
    int64_t *id_ptr = g_new(int64_t, 1);
    *id_ptr = id;
    g_object_set_data_full(G_OBJECT(button), "stock_id", id_ptr, g_free);
}

/* Edit button factory */
static void on_edit_dialog_response(GtkDialog *d, int response, gpointer user_data)
{
    (void)user_data;
    if (response == GTK_RESPONSE_OK) {
        int64_t *id_ptr = g_object_get_data(G_OBJECT(d), "stock_id");
        if (!id_ptr) return;
        int64_t id = *id_ptr;
        GtkWidget *label_entry = g_object_get_data(G_OBJECT(d), "label_entry");
        GtkWidget *length_spin = g_object_get_data(G_OBJECT(d), "length_spin");
        GtkWidget *diameter_spin = g_object_get_data(G_OBJECT(d), "diameter_spin");
        GtkWidget *thickness_spin = g_object_get_data(G_OBJECT(d), "thickness_spin");
        GtkWidget *qty_spin = g_object_get_data(G_OBJECT(d), "qty_spin");
        GtkWidget *cost_spin = g_object_get_data(G_OBJECT(d), "cost_spin");
        GtkWidget *material_combo = g_object_get_data(G_OBJECT(d), "material_combo");
        InventoryView *view = g_object_get_data(G_OBJECT(d), "view");

        const char *label = gtk_editable_get_text(GTK_EDITABLE(label_entry));
        double length = gtk_spin_button_get_value(GTK_SPIN_BUTTON(length_spin));
        double diameter = gtk_spin_button_get_value(GTK_SPIN_BUTTON(diameter_spin));
        double thickness = gtk_spin_button_get_value(GTK_SPIN_BUTTON(thickness_spin));
        int qty = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(qty_spin));
        double cost = gtk_spin_button_get_value(GTK_SPIN_BUTTON(cost_spin));
        MaterialType material = (MaterialType)gtk_drop_down_get_selected(GTK_DROP_DOWN(material_combo));

        if (label[0] != '\0' && length > 0) {
            if (db_update_stock(id, label, length, diameter, thickness, qty, cost, material) == 0) {
                inventory_view_refresh(view);
            }
        }
    }
    gtk_window_destroy(GTK_WINDOW(d));
}

static void on_edit_row_clicked(GtkButton *button, gpointer user_data)
{
    (void)user_data;
    int64_t *id_ptr = g_object_get_data(G_OBJECT(button), "stock_id");
    InventoryView *view = g_object_get_data(G_OBJECT(button), "view");

    if (!id_ptr || *id_ptr <= 0 || !view) return;
    int64_t id = *id_ptr;

    /* Get current stock data */
    StockItem item;
    if (db_get_stock(id, &item) != 0) return;

    /* Create edit dialog */
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Modifier le Stock",
        GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(view))),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Annuler", GTK_RESPONSE_CANCEL,
        "Enregistrer", GTK_RESPONSE_OK,
        NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_start(content, 12);
    gtk_widget_set_margin_end(content, 12);
    gtk_widget_set_margin_top(content, 12);
    gtk_widget_set_margin_bottom(content, 12);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_append(GTK_BOX(content), grid);

    /* Label */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Label:"), 0, 0, 1, 1);
    GtkWidget *label_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(label_entry), item.label);
    gtk_grid_attach(GTK_GRID(grid), label_entry, 1, 0, 1, 1);

    /* Length */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Longueur (mm):"), 0, 1, 1, 1);
    GtkWidget *length_spin = gtk_spin_button_new_with_range(0, 100000, 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(length_spin), item.length);
    gtk_grid_attach(GTK_GRID(grid), length_spin, 1, 1, 1, 1);

    /* Diameter */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Diamètre (mm):"), 0, 2, 1, 1);
    GtkWidget *diameter_spin = gtk_spin_button_new_with_range(0, 1000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(diameter_spin), item.diameter);
    gtk_grid_attach(GTK_GRID(grid), diameter_spin, 1, 2, 1, 1);

    /* Thickness */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Epaisseur (mm):"), 0, 3, 1, 1);
    GtkWidget *thickness_spin = gtk_spin_button_new_with_range(0, 100, 0.5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(thickness_spin), item.thickness);
    gtk_grid_attach(GTK_GRID(grid), thickness_spin, 1, 3, 1, 1);

    /* Quantity (-1 = unlimited) */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Quantité:"), 0, 4, 1, 1);
    GtkWidget *qty_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *qty_spin = gtk_spin_button_new_with_range(-1, 10000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(qty_spin), item.quantity);
    gtk_box_append(GTK_BOX(qty_box), qty_spin);
    GtkWidget *qty_hint = gtk_label_new("(-1 = illimité)");
    gtk_widget_add_css_class(qty_hint, "dim-label");
    gtk_box_append(GTK_BOX(qty_box), qty_hint);
    gtk_grid_attach(GTK_GRID(grid), qty_box, 1, 4, 1, 1);

    /* Cost */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Coût:"), 0, 5, 1, 1);
    GtkWidget *cost_spin = gtk_spin_button_new_with_range(0, 100000, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(cost_spin), item.cost);
    gtk_grid_attach(GTK_GRID(grid), cost_spin, 1, 5, 1, 1);

    /* Material */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Matière:"), 0, 6, 1, 1);
    const char *material_strings[] = {"", "Alu", "Epoxy", NULL};
    GtkWidget *material_combo = gtk_drop_down_new_from_strings(material_strings);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(material_combo), (guint)item.material);
    gtk_grid_attach(GTK_GRID(grid), material_combo, 1, 6, 1, 1);

    /* Store ID as allocated int64_t to avoid truncation */
    int64_t *dialog_id_ptr = g_new(int64_t, 1);
    *dialog_id_ptr = id;
    g_object_set_data_full(G_OBJECT(dialog), "stock_id", dialog_id_ptr, g_free);
    g_object_set_data(G_OBJECT(dialog), "label_entry", label_entry);
    g_object_set_data(G_OBJECT(dialog), "length_spin", length_spin);
    g_object_set_data(G_OBJECT(dialog), "diameter_spin", diameter_spin);
    g_object_set_data(G_OBJECT(dialog), "thickness_spin", thickness_spin);
    g_object_set_data(G_OBJECT(dialog), "qty_spin", qty_spin);
    g_object_set_data(G_OBJECT(dialog), "cost_spin", cost_spin);
    g_object_set_data(G_OBJECT(dialog), "material_combo", material_combo);
    g_object_set_data(G_OBJECT(dialog), "view", view);

    g_signal_connect(dialog, "response", G_CALLBACK(on_edit_dialog_response), NULL);
    gtk_widget_set_visible(dialog, TRUE);
}

static void setup_edit_factory(GtkSignalListItemFactory *factory,
                                GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    InventoryView *view = CUTTER_INVENTORY_VIEW(user_data);

    GtkWidget *button = gtk_button_new_from_icon_name("document-edit-symbolic");
    gtk_widget_add_css_class(button, "flat");
    gtk_widget_add_css_class(button, "circular");
    gtk_widget_set_tooltip_text(button, "Modifier");
    g_object_set_data(G_OBJECT(button), "view", view);
    g_signal_connect(button, "clicked", G_CALLBACK(on_edit_row_clicked), NULL);
    gtk_list_item_set_child(list_item, button);
}

static void bind_edit_factory(GtkSignalListItemFactory *factory,
                               GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *button = gtk_list_item_get_child(list_item);
    StockObject *obj = gtk_list_item_get_item(list_item);
    int64_t id = stock_object_get_id(obj);

    /* Store ID as allocated int64_t to avoid truncation on 32-bit platforms */
    int64_t *id_ptr = g_new(int64_t, 1);
    *id_ptr = id;
    g_object_set_data_full(G_OBJECT(button), "stock_id", id_ptr, g_free);
}

static GtkListItemFactory *create_factory(void (*bind_func)(GtkSignalListItemFactory *,
                                                             GtkListItem *, gpointer))
{
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_label_factory), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_func), NULL);
    return factory;
}

static void update_status(InventoryView *self)
{
    guint count = g_list_model_get_n_items(G_LIST_MODEL(self->store));
    int total_qty = 0;
    for (guint i = 0; i < count; i++) {
        StockObject *obj = g_list_model_get_item(G_LIST_MODEL(self->store), i);
        total_qty += stock_object_get_quantity(obj);
        g_object_unref(obj);
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Total: %u type(s), %d unité(s)", count, total_qty);
    gtk_label_set_text(GTK_LABEL(self->status_label), buf);
}

void inventory_view_refresh(InventoryView *self)
{
    g_list_store_remove_all(self->store);

    StockItem *items = NULL;
    int count = db_list_stock(&items, TRUE);  /* Always show all items including qty=0 */

    for (int i = 0; i < count; i++) {
        StockObject *obj = stock_object_new(&items[i]);
        g_list_store_append(self->store, obj);
        g_object_unref(obj);
    }

    if (items) g_free(items);

    update_status(self);
}

static void on_add_dialog_response(GtkDialog *d, int response, gpointer user_data)
{
    (void)user_data;
    if (response == GTK_RESPONSE_OK) {
        GtkWidget *label_entry = g_object_get_data(G_OBJECT(d), "label_entry");
        GtkWidget *length_spin = g_object_get_data(G_OBJECT(d), "length_spin");
        GtkWidget *diameter_spin = g_object_get_data(G_OBJECT(d), "diameter_spin");
        GtkWidget *thickness_spin = g_object_get_data(G_OBJECT(d), "thickness_spin");
        GtkWidget *qty_spin = g_object_get_data(G_OBJECT(d), "qty_spin");
        GtkWidget *cost_spin = g_object_get_data(G_OBJECT(d), "cost_spin");
        GtkWidget *material_combo = g_object_get_data(G_OBJECT(d), "material_combo");
        InventoryView *view = g_object_get_data(G_OBJECT(d), "view");

        const char *label = gtk_editable_get_text(GTK_EDITABLE(label_entry));
        double length = gtk_spin_button_get_value(GTK_SPIN_BUTTON(length_spin));
        double diameter = gtk_spin_button_get_value(GTK_SPIN_BUTTON(diameter_spin));
        double thickness = gtk_spin_button_get_value(GTK_SPIN_BUTTON(thickness_spin));
        int qty = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(qty_spin));
        double cost = gtk_spin_button_get_value(GTK_SPIN_BUTTON(cost_spin));
        MaterialType material = (MaterialType)gtk_drop_down_get_selected(GTK_DROP_DOWN(material_combo));

        if (label[0] != '\0' && length > 0) {
            db_add_stock(label, length, diameter, thickness, qty, cost, material, false);
            inventory_view_refresh(view);
        }
    }
    gtk_window_destroy(GTK_WINDOW(d));
}

static void on_add_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    InventoryView *self = CUTTER_INVENTORY_VIEW(user_data);

    /* Simple dialog for adding stock */
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Ajouter du Stock",
        GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(self))),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Annuler", GTK_RESPONSE_CANCEL,
        "Ajouter", GTK_RESPONSE_OK,
        NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_start(content, 12);
    gtk_widget_set_margin_end(content, 12);
    gtk_widget_set_margin_top(content, 12);
    gtk_widget_set_margin_bottom(content, 12);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_append(GTK_BOX(content), grid);

    /* Label */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Label:"), 0, 0, 1, 1);
    GtkWidget *label_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(label_entry), "Tube_6m");
    gtk_grid_attach(GTK_GRID(grid), label_entry, 1, 0, 1, 1);

    /* Length */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Longueur (mm):"), 0, 1, 1, 1);
    GtkWidget *length_spin = gtk_spin_button_new_with_range(0, 100000, 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(length_spin), 6000);
    gtk_grid_attach(GTK_GRID(grid), length_spin, 1, 1, 1, 1);

    /* Diameter */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Diamètre (mm):"), 0, 2, 1, 1);
    GtkWidget *diameter_spin = gtk_spin_button_new_with_range(0, 1000, 1);
    gtk_grid_attach(GTK_GRID(grid), diameter_spin, 1, 2, 1, 1);

    /* Thickness */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Epaisseur (mm):"), 0, 3, 1, 1);
    GtkWidget *thickness_spin = gtk_spin_button_new_with_range(0, 100, 0.5);
    gtk_grid_attach(GTK_GRID(grid), thickness_spin, 1, 3, 1, 1);

    /* Quantity (-1 = unlimited) */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Quantité:"), 0, 4, 1, 1);
    GtkWidget *qty_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *qty_spin = gtk_spin_button_new_with_range(-1, 10000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(qty_spin), -1);
    gtk_box_append(GTK_BOX(qty_box), qty_spin);
    GtkWidget *qty_hint = gtk_label_new("(-1 = illimité)");
    gtk_widget_add_css_class(qty_hint, "dim-label");
    gtk_box_append(GTK_BOX(qty_box), qty_hint);
    gtk_grid_attach(GTK_GRID(grid), qty_box, 1, 4, 1, 1);

    /* Cost */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Coût:"), 0, 5, 1, 1);
    GtkWidget *cost_spin = gtk_spin_button_new_with_range(0, 100000, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(cost_spin), 1.0);
    gtk_grid_attach(GTK_GRID(grid), cost_spin, 1, 5, 1, 1);

    /* Material */
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Matière:"), 0, 6, 1, 1);
    const char *material_strings[] = {"", "Alu", "Epoxy", NULL};
    GtkWidget *material_combo = gtk_drop_down_new_from_strings(material_strings);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(material_combo), MATERIAL_ALU);
    gtk_grid_attach(GTK_GRID(grid), material_combo, 1, 6, 1, 1);

    g_object_set_data(G_OBJECT(dialog), "label_entry", label_entry);
    g_object_set_data(G_OBJECT(dialog), "length_spin", length_spin);
    g_object_set_data(G_OBJECT(dialog), "diameter_spin", diameter_spin);
    g_object_set_data(G_OBJECT(dialog), "thickness_spin", thickness_spin);
    g_object_set_data(G_OBJECT(dialog), "qty_spin", qty_spin);
    g_object_set_data(G_OBJECT(dialog), "cost_spin", cost_spin);
    g_object_set_data(G_OBJECT(dialog), "material_combo", material_combo);
    g_object_set_data(G_OBJECT(dialog), "view", self);

    g_signal_connect(dialog, "response", G_CALLBACK(on_add_dialog_response), NULL);
    gtk_widget_set_visible(dialog, TRUE);
}

static void on_export_pdf_response(GObject *source, GAsyncResult *result, gpointer user_data)
{
    (void)user_data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GFile *file = gtk_file_dialog_save_finish(dialog, result, NULL);

    if (file) {
        char *path = g_file_get_path(file);
        if (path) {
            /* Load stock items */
            StockItem *items = NULL;
            int count = db_list_stock(&items, TRUE);

            if (count > 0 && items) {
                pdf_export_stock(path, items, count);
            }

            if (items) g_free(items);
            g_free(path);
        }
        g_object_unref(file);
    }
}

static void on_export_pdf_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    InventoryView *self = CUTTER_INVENTORY_VIEW(user_data);

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Exporter le stock en PDF");
    gtk_file_dialog_set_initial_name(dialog, "stock.pdf");

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Fichiers PDF");
    gtk_file_filter_add_pattern(filter, "*.pdf");

    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filter);
    g_object_unref(filters);

    gtk_file_dialog_save(dialog,
                         GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(self))),
                         NULL,
                         on_export_pdf_response,
                         self);
    g_object_unref(dialog);
}

/* Helper structure for import ODS context */
typedef struct {
    InventoryView *view;
    char *path;
} ImportOdsContext;

static void on_import_confirm_response(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GtkAlertDialog *alert = GTK_ALERT_DIALOG(source);
    ImportOdsContext *ctx = (ImportOdsContext *)user_data;
    GError *error = NULL;

    int button = gtk_alert_dialog_choose_finish(alert, result, &error);
    if (error) {
        g_error_free(error);
        g_free(ctx->path);
        g_free(ctx);
        return;
    }

    /* Button 0 = Cancel, Button 1 = Continuer */
    if (button == 1) {
        int count = ods_import_stock(ctx->path);
        if (count >= 0) {
            /* Reload data */
            inventory_view_refresh(ctx->view);

            /* Show success message */
            char *filename = g_path_get_basename(ctx->path);
            char *message = g_strdup_printf("%d éléments importés depuis %s", count, filename);
            GtkAlertDialog *msg = gtk_alert_dialog_new("Import réussi");
            gtk_alert_dialog_set_detail(msg, message);
            gtk_alert_dialog_show(msg, GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(ctx->view))));
            g_object_unref(msg);
            g_free(message);
            g_free(filename);
        } else {
            /* Show error message */
            GtkAlertDialog *msg = gtk_alert_dialog_new("Erreur");
            gtk_alert_dialog_set_detail(msg, "Erreur lors de l'import du fichier ODS");
            gtk_alert_dialog_show(msg, GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(ctx->view))));
            g_object_unref(msg);
        }
    }

    g_free(ctx->path);
    g_free(ctx);
}

/* Import ODS callbacks */
static void on_import_ods_response(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    InventoryView *self = CUTTER_INVENTORY_VIEW(user_data);
    GError *error = NULL;

    GFile *file = gtk_file_dialog_open_finish(dialog, result, &error);
    if (error) {
        g_error_free(error);
        return;
    }

    if (file) {
        char *path = g_file_get_path(file);

        /* Create context for async callbacks */
        ImportOdsContext *ctx = g_new(ImportOdsContext, 1);
        ctx->view = self;
        ctx->path = g_strdup(path);

        /* Show confirmation dialog */
        GtkAlertDialog *alert = gtk_alert_dialog_new("Attention");
        gtk_alert_dialog_set_detail(alert, "L'import va REMPLACER tout le stock existant. Continuer ?");

        const char *buttons[] = {"Annuler", "Continuer", NULL};
        gtk_alert_dialog_set_buttons(alert, buttons);
        gtk_alert_dialog_set_default_button(alert, 0);
        gtk_alert_dialog_set_cancel_button(alert, 0);

        gtk_alert_dialog_choose(alert,
                                GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(self))),
                                NULL,
                                on_import_confirm_response,
                                ctx);

        g_object_unref(alert);
        g_free(path);
        g_object_unref(file);
    }
}

static void on_import_ods_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    InventoryView *self = CUTTER_INVENTORY_VIEW(user_data);

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Importer le stock depuis ODS");
    gtk_file_dialog_set_accept_label(dialog, "Importer");

    /* Add ODS filter */
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Fichiers ODS");
    gtk_file_filter_add_pattern(filter, "*.ods");
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filter);
    g_object_unref(filters);

    gtk_file_dialog_open(dialog,
                         GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(self))),
                         NULL,
                         on_import_ods_response,
                         self);
    g_object_unref(dialog);
}

/* Export ODS callbacks */
static void on_export_ods_response(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    InventoryView *self = CUTTER_INVENTORY_VIEW(user_data);
    GError *error = NULL;

    GFile *file = gtk_file_dialog_save_finish(dialog, result, &error);
    if (error) {
        g_error_free(error);
        return;
    }

    if (file) {
        char *path = g_file_get_path(file);

        if (ods_export_stock(path) == 0) {
            /* Show success message */
            char *filename = g_path_get_basename(path);
            char *message = g_strdup_printf("Stock exporté vers %s", filename);
            GtkAlertDialog *alert = gtk_alert_dialog_new("Export réussi");
            gtk_alert_dialog_set_detail(alert, message);
            gtk_alert_dialog_show(alert, GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(self))));
            g_object_unref(alert);
            g_free(message);
            g_free(filename);
        } else {
            /* Show error message */
            GtkAlertDialog *alert = gtk_alert_dialog_new("Erreur");
            gtk_alert_dialog_set_detail(alert, "Erreur lors de l'export vers ODS");
            gtk_alert_dialog_show(alert, GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(self))));
            g_object_unref(alert);
        }

        g_free(path);
        g_object_unref(file);
    }
}

static void on_export_ods_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    InventoryView *self = CUTTER_INVENTORY_VIEW(user_data);

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Exporter le stock vers ODS");
    gtk_file_dialog_set_accept_label(dialog, "Exporter");
    gtk_file_dialog_set_initial_name(dialog, "stock.ods");

    /* Add ODS filter */
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Fichiers ODS");
    gtk_file_filter_add_pattern(filter, "*.ods");
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filter);
    g_object_unref(filters);

    gtk_file_dialog_save(dialog,
                         GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(self))),
                         NULL,
                         on_export_ods_response,
                         self);
    g_object_unref(dialog);
}

static void inventory_view_class_init(InventoryViewClass *klass)
{
    (void)klass;
}

static void inventory_view_init(InventoryView *self)
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(GTK_WIDGET(self), 12);
    gtk_widget_set_margin_end(GTK_WIDGET(self), 12);
    gtk_widget_set_margin_top(GTK_WIDGET(self), 12);
    gtk_widget_set_margin_bottom(GTK_WIDGET(self), 12);

    /* Header */
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *title = gtk_label_new("Stock");
    gtk_widget_add_css_class(title, "title-2");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_box_append(GTK_BOX(header), title);

    GtkWidget *pdf_button = gtk_button_new_with_label("Exporter PDF");
    g_signal_connect(pdf_button, "clicked", G_CALLBACK(on_export_pdf_clicked), self);
    gtk_box_append(GTK_BOX(header), pdf_button);

    GtkWidget *import_ods_button = gtk_button_new_with_label("Importer ODS");
    g_signal_connect(import_ods_button, "clicked", G_CALLBACK(on_import_ods_clicked), self);
    gtk_box_append(GTK_BOX(header), import_ods_button);

    GtkWidget *export_ods_button = gtk_button_new_with_label("Exporter ODS");
    g_signal_connect(export_ods_button, "clicked", G_CALLBACK(on_export_ods_clicked), self);
    gtk_box_append(GTK_BOX(header), export_ods_button);

    self->add_button = gtk_button_new_with_label("+ Ajouter");
    gtk_widget_add_css_class(self->add_button, "suggested-action");
    g_signal_connect(self->add_button, "clicked", G_CALLBACK(on_add_clicked), self);
    gtk_box_append(GTK_BOX(header), self->add_button);

    gtk_box_append(GTK_BOX(self), header);

    /* Store and sort model */
    self->store = g_list_store_new(CUTTER_TYPE_STOCK_OBJECT);
    self->sort_model = gtk_sort_list_model_new(G_LIST_MODEL(self->store), NULL);
    self->selection = gtk_single_selection_new(G_LIST_MODEL(self->sort_model));

    /* Column view */
    self->column_view = gtk_column_view_new(GTK_SELECTION_MODEL(self->selection));
    gtk_column_view_set_enable_rubberband(GTK_COLUMN_VIEW(self->column_view), FALSE);
    gtk_widget_set_vexpand(self->column_view, TRUE);

    /* Connect sorter from column view to sort model */
    GtkSorter *sorter = gtk_column_view_get_sorter(GTK_COLUMN_VIEW(self->column_view));
    gtk_sort_list_model_set_sorter(self->sort_model, sorter);

    /* ID column */
    GtkColumnViewColumn *col_id = gtk_column_view_column_new("ID",
        create_factory(bind_id_factory));
    gtk_column_view_column_set_fixed_width(col_id, 50);
    gtk_column_view_column_set_sorter(col_id, GTK_SORTER(gtk_custom_sorter_new(compare_id, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_id);

    /* Label column */
    GtkColumnViewColumn *col_label = gtk_column_view_column_new("Label",
        create_factory(bind_label_factory));
    gtk_column_view_column_set_expand(col_label, TRUE);
    gtk_column_view_column_set_sorter(col_label, GTK_SORTER(gtk_custom_sorter_new(compare_label, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_label);

    /* Material column */
    GtkColumnViewColumn *col_mat = gtk_column_view_column_new("Matière",
        create_factory(bind_material_factory));
    gtk_column_view_column_set_fixed_width(col_mat, 70);
    gtk_column_view_column_set_sorter(col_mat, GTK_SORTER(gtk_custom_sorter_new(compare_material, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_mat);

    /* Length column */
    GtkColumnViewColumn *col_length = gtk_column_view_column_new("Longueur",
        create_factory(bind_length_factory));
    gtk_column_view_column_set_fixed_width(col_length, 100);
    gtk_column_view_column_set_sorter(col_length, GTK_SORTER(gtk_custom_sorter_new(compare_length, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_length);

    /* Diameter column */
    GtkColumnViewColumn *col_diameter = gtk_column_view_column_new("Diam.",
        create_factory(bind_diameter_factory));
    gtk_column_view_column_set_fixed_width(col_diameter, 80);
    gtk_column_view_column_set_sorter(col_diameter, GTK_SORTER(gtk_custom_sorter_new(compare_diameter, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_diameter);

    /* Thickness column */
    GtkColumnViewColumn *col_thick = gtk_column_view_column_new("Epais.",
        create_factory(bind_thickness_factory));
    gtk_column_view_column_set_fixed_width(col_thick, 80);
    gtk_column_view_column_set_sorter(col_thick, GTK_SORTER(gtk_custom_sorter_new(compare_thickness, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_thick);

    /* Quantity column */
    GtkColumnViewColumn *col_qty = gtk_column_view_column_new("Qté",
        create_factory(bind_quantity_factory));
    gtk_column_view_column_set_fixed_width(col_qty, 60);
    gtk_column_view_column_set_sorter(col_qty, GTK_SORTER(gtk_custom_sorter_new(compare_quantity, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_qty);

    /* Cost column */
    GtkColumnViewColumn *col_cost = gtk_column_view_column_new("Coût",
        create_factory(bind_cost_factory));
    gtk_column_view_column_set_fixed_width(col_cost, 70);
    gtk_column_view_column_set_sorter(col_cost, GTK_SORTER(gtk_custom_sorter_new(compare_cost, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_cost);

    /* Type column */
    GtkColumnViewColumn *col_type = gtk_column_view_column_new("Type",
        create_factory(bind_type_factory));
    gtk_column_view_column_set_fixed_width(col_type, 70);
    gtk_column_view_column_set_sorter(col_type, GTK_SORTER(gtk_custom_sorter_new(compare_type, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_type);

    /* Edit column with pencil icon */
    GtkListItemFactory *edit_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(edit_factory, "setup", G_CALLBACK(setup_edit_factory), self);
    g_signal_connect(edit_factory, "bind", G_CALLBACK(bind_edit_factory), NULL);
    GtkColumnViewColumn *col_edit = gtk_column_view_column_new("", edit_factory);
    gtk_column_view_column_set_fixed_width(col_edit, 40);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_edit);

    /* Delete column with trash icon */
    GtkListItemFactory *delete_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(delete_factory, "setup", G_CALLBACK(setup_delete_factory), self);
    g_signal_connect(delete_factory, "bind", G_CALLBACK(bind_delete_factory), NULL);
    GtkColumnViewColumn *col_delete = gtk_column_view_column_new("", delete_factory);
    gtk_column_view_column_set_fixed_width(col_delete, 40);
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_delete);

    /* Scrolled window for column view */
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), self->column_view);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(self), scroll);

    /* Footer - status only */
    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(footer, 6);

    self->status_label = gtk_label_new("Total: 0 type(s)");
    gtk_widget_set_hexpand(self->status_label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(self->status_label), 1);
    gtk_box_append(GTK_BOX(footer), self->status_label);

    gtk_box_append(GTK_BOX(self), footer);

    /* Load data */
    inventory_view_refresh(self);
}

GtkWidget *inventory_view_new(void)
{
    return g_object_new(CUTTER_TYPE_INVENTORY_VIEW, NULL);
}
