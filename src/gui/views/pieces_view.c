/*
 * pieces_view.c - Pieces input view
 */

#include "pieces_view.h"
#include "../models/piece_object.h"
#include "../../csp_types.h"
#include "../../csv_io.h"

/* Comparison functions for sorting */
static int compare_piece_label(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    PieceObject *obj_a = CUTTER_PIECE_OBJECT((gpointer)a);
    PieceObject *obj_b = CUTTER_PIECE_OBJECT((gpointer)b);
    return g_strcmp0(piece_object_get_label(obj_a), piece_object_get_label(obj_b));
}

static int compare_piece_material(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    PieceObject *obj_a = CUTTER_PIECE_OBJECT((gpointer)a);
    PieceObject *obj_b = CUTTER_PIECE_OBJECT((gpointer)b);
    int mat_a = (int)piece_object_get_material(obj_a);
    int mat_b = (int)piece_object_get_material(obj_b);
    return mat_a - mat_b;
}

static int compare_piece_length(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    PieceObject *obj_a = CUTTER_PIECE_OBJECT((gpointer)a);
    PieceObject *obj_b = CUTTER_PIECE_OBJECT((gpointer)b);
    double len_a = piece_object_get_length(obj_a);
    double len_b = piece_object_get_length(obj_b);
    return (len_a > len_b) - (len_a < len_b);
}

static int compare_piece_diameter(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    PieceObject *obj_a = CUTTER_PIECE_OBJECT((gpointer)a);
    PieceObject *obj_b = CUTTER_PIECE_OBJECT((gpointer)b);
    double d_a = piece_object_get_diameter(obj_a);
    double d_b = piece_object_get_diameter(obj_b);
    return (d_a > d_b) - (d_a < d_b);
}

static int compare_piece_thickness(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    PieceObject *obj_a = CUTTER_PIECE_OBJECT((gpointer)a);
    PieceObject *obj_b = CUTTER_PIECE_OBJECT((gpointer)b);
    double t_a = piece_object_get_thickness(obj_a);
    double t_b = piece_object_get_thickness(obj_b);
    return (t_a > t_b) - (t_a < t_b);
}

static int compare_piece_quantity(gconstpointer a, gconstpointer b, gpointer user_data)
{
    (void)user_data;
    PieceObject *obj_a = CUTTER_PIECE_OBJECT((gpointer)a);
    PieceObject *obj_b = CUTTER_PIECE_OBJECT((gpointer)b);
    int q_a = piece_object_get_quantity(obj_a);
    int q_b = piece_object_get_quantity(obj_b);
    return q_a - q_b;
}

struct _PiecesView {
    GtkBox parent_instance;

    GtkWidget *column_view;
    GtkWidget *add_button;
    GtkWidget *import_button;
    GtkWidget *clear_button;
    GtkWidget *status_label;

    GListStore *store;
    GtkSortListModel *sort_model;
    GtkSingleSelection *selection;
};

G_DEFINE_TYPE(PiecesView, pieces_view, GTK_TYPE_BOX)

static void setup_label_factory(GtkSignalListItemFactory *factory,
                                 GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_list_item_set_child(list_item, label);
}

static void bind_label_factory(GtkSignalListItemFactory *factory,
                                GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    PieceObject *obj = gtk_list_item_get_item(list_item);
    gtk_label_set_text(GTK_LABEL(label), piece_object_get_label(obj));
}

static void bind_length_factory(GtkSignalListItemFactory *factory,
                                 GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    PieceObject *obj = gtk_list_item_get_item(list_item);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f mm", piece_object_get_length(obj));
    gtk_label_set_text(GTK_LABEL(label), buf);
}

static void bind_diameter_factory(GtkSignalListItemFactory *factory,
                                   GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    PieceObject *obj = gtk_list_item_get_item(list_item);
    double d = piece_object_get_diameter(obj);
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
    PieceObject *obj = gtk_list_item_get_item(list_item);
    double t = piece_object_get_thickness(obj);
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
    PieceObject *obj = gtk_list_item_get_item(list_item);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", piece_object_get_quantity(obj));
    gtk_label_set_text(GTK_LABEL(label), buf);
}

static void bind_material_factory(GtkSignalListItemFactory *factory,
                                   GtkListItem *list_item, gpointer user_data)
{
    (void)factory;
    (void)user_data;
    GtkWidget *label = gtk_list_item_get_child(list_item);
    PieceObject *obj = gtk_list_item_get_item(list_item);
    gtk_label_set_text(GTK_LABEL(label), material_to_string(piece_object_get_material(obj)));
}

static GtkListItemFactory *create_factory(void (*bind_func)(GtkSignalListItemFactory *,
                                                             GtkListItem *, gpointer))
{
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup_label_factory), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(bind_func), NULL);
    return factory;
}

static void update_status(PiecesView *self)
{
    guint count = g_list_model_get_n_items(G_LIST_MODEL(self->store));
    int total_qty = 0;
    double total_length = 0;
    for (guint i = 0; i < count; i++) {
        PieceObject *obj = g_list_model_get_item(G_LIST_MODEL(self->store), i);
        int qty = piece_object_get_quantity(obj);
        total_qty += qty;
        total_length += qty * piece_object_get_length(obj);
        g_object_unref(obj);
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Pièces: %u type(s), %d unité(s) | Longueur totale: %.0f mm",
             count, total_qty, total_length);
    gtk_label_set_text(GTK_LABEL(self->status_label), buf);
}

void pieces_view_clear(PiecesView *self)
{
    g_list_store_remove_all(self->store);
    update_status(self);
}

int pieces_view_get_count(PiecesView *self)
{
    return g_list_model_get_n_items(G_LIST_MODEL(self->store));
}

void pieces_view_fill_instance(PiecesView *self, void *inst)
{
    CSPInstance *instance = (CSPInstance *)inst;
    guint count = g_list_model_get_n_items(G_LIST_MODEL(self->store));

    for (guint i = 0; i < count; i++) {
        PieceObject *obj = g_list_model_get_item(G_LIST_MODEL(self->store), i);
        PieceDemand piece;
        piece_object_to_piece(obj, &piece);

        int idx;
        if (piece.diameter > 0 || piece.thickness > 0) {
            idx = csp_add_piece_tube(instance, piece.length, piece.diameter,
                                     piece.thickness, piece.quantity,
                                     piece.material, piece.label);
        } else {
            idx = csp_add_piece(instance, piece.length, piece.quantity, piece.label);
        }
        (void)idx;  /* Merging handled by csp_add_piece_tube */
        g_object_unref(obj);
    }
}

static void on_add_piece_dialog_response(GtkDialog *d, int response, gpointer user_data)
{
    (void)user_data;
    if (response == GTK_RESPONSE_OK) {
        GtkWidget *label_entry = g_object_get_data(G_OBJECT(d), "label_entry");
        GtkWidget *length_spin = g_object_get_data(G_OBJECT(d), "length_spin");
        GtkWidget *diameter_spin = g_object_get_data(G_OBJECT(d), "diameter_spin");
        GtkWidget *thickness_spin = g_object_get_data(G_OBJECT(d), "thickness_spin");
        GtkWidget *qty_spin = g_object_get_data(G_OBJECT(d), "qty_spin");
        PiecesView *view = g_object_get_data(G_OBJECT(d), "view");

        const char *label = gtk_editable_get_text(GTK_EDITABLE(label_entry));
        double length = gtk_spin_button_get_value(GTK_SPIN_BUTTON(length_spin));
        double diameter = gtk_spin_button_get_value(GTK_SPIN_BUTTON(diameter_spin));
        double thickness = gtk_spin_button_get_value(GTK_SPIN_BUTTON(thickness_spin));
        int qty = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(qty_spin));

        if (label[0] != '\0' && length > 0) {
            PieceDemand piece = {0};
            strncpy(piece.label, label, MAX_LABEL_LEN - 1);
            piece.length = length;
            piece.diameter = diameter;
            piece.thickness = thickness;
            piece.quantity = qty;

            PieceObject *obj = piece_object_new(&piece);
            g_list_store_append(view->store, obj);
            g_object_unref(obj);
            update_status(view);
        }
    }
    gtk_window_destroy(GTK_WINDOW(d));
}

static void on_add_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    PiecesView *self = CUTTER_PIECES_VIEW(user_data);

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Ajouter une Pièce",
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

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Label:"), 0, 0, 1, 1);
    GtkWidget *label_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), label_entry, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Longueur (mm):"), 0, 1, 1, 1);
    GtkWidget *length_spin = gtk_spin_button_new_with_range(0, 100000, 10);
    gtk_grid_attach(GTK_GRID(grid), length_spin, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Diamètre (mm):"), 0, 2, 1, 1);
    GtkWidget *diameter_spin = gtk_spin_button_new_with_range(0, 1000, 1);
    gtk_grid_attach(GTK_GRID(grid), diameter_spin, 1, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Epaisseur (mm):"), 0, 3, 1, 1);
    GtkWidget *thickness_spin = gtk_spin_button_new_with_range(0, 100, 0.5);
    gtk_grid_attach(GTK_GRID(grid), thickness_spin, 1, 3, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Quantité:"), 0, 4, 1, 1);
    GtkWidget *qty_spin = gtk_spin_button_new_with_range(1, 10000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(qty_spin), 1);
    gtk_grid_attach(GTK_GRID(grid), qty_spin, 1, 4, 1, 1);

    g_object_set_data(G_OBJECT(dialog), "label_entry", label_entry);
    g_object_set_data(G_OBJECT(dialog), "length_spin", length_spin);
    g_object_set_data(G_OBJECT(dialog), "diameter_spin", diameter_spin);
    g_object_set_data(G_OBJECT(dialog), "thickness_spin", thickness_spin);
    g_object_set_data(G_OBJECT(dialog), "qty_spin", qty_spin);
    g_object_set_data(G_OBJECT(dialog), "view", self);

    g_signal_connect(dialog, "response", G_CALLBACK(on_add_piece_dialog_response), NULL);
    gtk_widget_set_visible(dialog, TRUE);
}

/* Detect if a CSV file is in Odoo format by checking the header */
static bool is_odoo_format(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    char line[256];
    bool is_odoo = false;

    if (fgets(line, sizeof(line), fp)) {
        /* Odoo format has "Source" in the header */
        if (strstr(line, "Source") || strstr(line, "Quantité à produire")) {
            is_odoo = true;
        }
    }

    fclose(fp);
    return is_odoo;
}

static void on_import_response(GObject *source, GAsyncResult *result, gpointer user_data)
{
    PiecesView *self = user_data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GFile *file = gtk_file_dialog_open_finish(dialog, result, NULL);

    if (file) {
        char *path = g_file_get_path(file);
        CSPInstance *temp = g_new0(CSPInstance, 1);
        csp_instance_init(temp);

        /* Auto-detect format and use appropriate parser */
        int loaded;
        if (is_odoo_format(path)) {
            loaded = csv_load_pieces_odoo(path, temp);
        } else {
            loaded = csv_load_pieces(path, temp);
        }

        if (loaded > 0) {
            for (int i = 0; i < temp->num_pieces; i++) {
                PieceObject *obj = piece_object_new(&temp->pieces[i]);
                g_list_store_append(self->store, obj);
                g_object_unref(obj);
            }
            update_status(self);
        }

        g_free(temp);
        g_free(path);
        g_object_unref(file);
    }
}

static void on_import_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    PiecesView *self = CUTTER_PIECES_VIEW(user_data);

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Importer des pièces CSV");

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Fichiers CSV");
    gtk_file_filter_add_pattern(filter, "*.csv");

    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));

    gtk_file_dialog_open(dialog,
                         GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(self))),
                         NULL,
                         on_import_response,
                         self);

    g_object_unref(filter);
    g_object_unref(filters);
}

static void on_clear_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    PiecesView *self = CUTTER_PIECES_VIEW(user_data);
    pieces_view_clear(self);
}

static void pieces_view_class_init(PiecesViewClass *klass)
{
    (void)klass;
}

static void pieces_view_init(PiecesView *self)
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(GTK_WIDGET(self), 12);
    gtk_widget_set_margin_end(GTK_WIDGET(self), 12);
    gtk_widget_set_margin_top(GTK_WIDGET(self), 12);
    gtk_widget_set_margin_bottom(GTK_WIDGET(self), 12);

    /* Header */
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *title = gtk_label_new("Pièces à découper");
    gtk_widget_add_css_class(title, "title-2");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_box_append(GTK_BOX(header), title);

    self->import_button = gtk_button_new_with_label("Importer CSV");
    g_signal_connect(self->import_button, "clicked", G_CALLBACK(on_import_clicked), self);
    gtk_box_append(GTK_BOX(header), self->import_button);

    self->add_button = gtk_button_new_with_label("+ Ajouter");
    gtk_widget_add_css_class(self->add_button, "suggested-action");
    g_signal_connect(self->add_button, "clicked", G_CALLBACK(on_add_clicked), self);
    gtk_box_append(GTK_BOX(header), self->add_button);

    self->clear_button = gtk_button_new_with_label("Effacer");
    g_signal_connect(self->clear_button, "clicked", G_CALLBACK(on_clear_clicked), self);
    gtk_box_append(GTK_BOX(header), self->clear_button);

    gtk_box_append(GTK_BOX(self), header);

    /* Store and sort model */
    self->store = g_list_store_new(CUTTER_TYPE_PIECE_OBJECT);
    self->sort_model = gtk_sort_list_model_new(G_LIST_MODEL(self->store), NULL);
    self->selection = gtk_single_selection_new(G_LIST_MODEL(self->sort_model));

    /* Column view */
    self->column_view = gtk_column_view_new(GTK_SELECTION_MODEL(self->selection));
    gtk_widget_set_vexpand(self->column_view, TRUE);

    /* Connect sorter from column view to sort model */
    GtkSorter *sorter = gtk_column_view_get_sorter(GTK_COLUMN_VIEW(self->column_view));
    gtk_sort_list_model_set_sorter(self->sort_model, sorter);

    GtkColumnViewColumn *col_label = gtk_column_view_column_new("Label",
        create_factory(bind_label_factory));
    gtk_column_view_column_set_expand(col_label, TRUE);
    gtk_column_view_column_set_sorter(col_label, GTK_SORTER(gtk_custom_sorter_new(compare_piece_label, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_label);

    /* Material column */
    GtkColumnViewColumn *col_mat = gtk_column_view_column_new("Matière",
        create_factory(bind_material_factory));
    gtk_column_view_column_set_fixed_width(col_mat, 70);
    gtk_column_view_column_set_sorter(col_mat, GTK_SORTER(gtk_custom_sorter_new(compare_piece_material, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_mat);

    GtkColumnViewColumn *col_length = gtk_column_view_column_new("Longueur",
        create_factory(bind_length_factory));
    gtk_column_view_column_set_fixed_width(col_length, 100);
    gtk_column_view_column_set_sorter(col_length, GTK_SORTER(gtk_custom_sorter_new(compare_piece_length, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_length);

    GtkColumnViewColumn *col_diameter = gtk_column_view_column_new("Diam.",
        create_factory(bind_diameter_factory));
    gtk_column_view_column_set_fixed_width(col_diameter, 80);
    gtk_column_view_column_set_sorter(col_diameter, GTK_SORTER(gtk_custom_sorter_new(compare_piece_diameter, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_diameter);

    GtkColumnViewColumn *col_thick = gtk_column_view_column_new("Epais.",
        create_factory(bind_thickness_factory));
    gtk_column_view_column_set_fixed_width(col_thick, 80);
    gtk_column_view_column_set_sorter(col_thick, GTK_SORTER(gtk_custom_sorter_new(compare_piece_thickness, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_thick);

    GtkColumnViewColumn *col_qty = gtk_column_view_column_new("Quantité",
        create_factory(bind_quantity_factory));
    gtk_column_view_column_set_fixed_width(col_qty, 80);
    gtk_column_view_column_set_sorter(col_qty, GTK_SORTER(gtk_custom_sorter_new(compare_piece_quantity, NULL, NULL)));
    gtk_column_view_append_column(GTK_COLUMN_VIEW(self->column_view), col_qty);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), self->column_view);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(self), scroll);

    /* Footer */
    self->status_label = gtk_label_new("Pièces: 0 type(s), 0 unite(s)");
    gtk_widget_set_margin_top(self->status_label, 6);
    gtk_label_set_xalign(GTK_LABEL(self->status_label), 0);
    gtk_box_append(GTK_BOX(self), self->status_label);
}

GtkWidget *pieces_view_new(void)
{
    return g_object_new(CUTTER_TYPE_PIECES_VIEW, NULL);
}
