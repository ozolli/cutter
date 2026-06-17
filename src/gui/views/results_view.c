/*
 * results_view.c - Results display with cutting diagram
 */

#include "results_view.h"
#include "inventory_view.h"
#include "../../pdf_export.h"
#include "../../csv_io.h"
#include "../../db.h"
#include <math.h>
#include <inttypes.h>

struct _ResultsView {
    GtkBox parent_instance;

    GtkWidget *bars_label;
    GtkWidget *waste_label;
    GtkWidget *efficiency_label;
    GtkWidget *optimal_label;
    GtkWidget *diagram_area;
    GtkWidget *export_pdf_button;
    GtkWidget *update_stock_button;
    GtkWidget *no_results_label;

    CSPInstance *instance;
    CSPSolution *solution;
    gboolean has_results;
    gboolean stock_updated;  /* Prevent double-apply */
};

G_DEFINE_TYPE(ResultsView, results_view, GTK_TYPE_BOX)

/* Grayscale shades for pieces (same as pdf_export.c) */
static const double GRAYS[] = {
    0.95,  /* Very light gray */
    0.80,  /* Light gray */
    0.90,  /* Lighter gray */
    0.75,  /* Medium light gray */
    0.85,  /* Light gray alt */
    0.70,  /* Medium gray */
};
static const int NUM_GRAYS = 6;

static void draw_diagram(GtkDrawingArea *area, cairo_t *cr,
                          int width, int height, gpointer user_data)
{
    ResultsView *self = CUTTER_RESULTS_VIEW(user_data);

    if (!self->has_results || !self->instance || !self->solution) {
        return;
    }

    const double margin = 20;
    const double bar_height = 36;     /* Taller for 2-line labels */
    const double bar_spacing = 52;
    const double label_width = 200;

    /* Find max stock length for scaling */
    double max_stock_length = 0;
    for (int s = 0; s < self->instance->num_stocks; s++) {
        if (self->instance->stocks[s].length > max_stock_length)
            max_stock_length = self->instance->stocks[s].length;
    }

    double draw_width = width - 2 * margin - label_width;
    if (draw_width < 100) draw_width = 100;
    double scale = draw_width / max_stock_length;

    double y = margin;
    int bar_num = 1;

    for (int j = 0; j < self->instance->num_patterns; j++) {
        int usage = self->solution->pattern_usage[j];
        if (usage <= 0) continue;

        const CuttingPattern *pattern = &self->instance->patterns[j];
        int stock_id = pattern->stock_id;
        const StockBar *stock = &self->instance->stocks[stock_id];
        double stock_len = stock->length;

        for (int k = 0; k < usage; k++) {
            double bar_x = margin + label_width;
            double bar_w = stock_len * scale;

            /* Draw bar label (above bar) */
            cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 10);

            char label[128];
            const char *mat_str = material_to_string(stock->material);
            if (mat_str[0] != '-') {
                snprintf(label, sizeof(label), "#%d: %s %s (%.0fmm)", stock->id,
                         stock->label, mat_str, stock_len);
            } else {
                snprintf(label, sizeof(label), "#%d: %s (%.0fmm)", stock->id,
                         stock->label, stock_len);
            }
            cairo_move_to(cr, margin, y - 4);
            cairo_show_text(cr, label);

            /* Draw bar outline */
            cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
            cairo_set_line_width(cr, 1);
            cairo_rectangle(cr, bar_x, y, bar_w, bar_height);
            cairo_stroke(cr);

            /* Draw pieces */
            double x = bar_x;
            int color_idx = 0;

            for (int i = 0; i < self->instance->num_pieces; i++) {
                int count = pattern->cuts[i];
                if (count <= 0) continue;

                const PieceDemand *piece = &self->instance->pieces[i];
                double piece_len = piece->length;
                double kerf = self->instance->saw_kerf;

                for (int c = 0; c < count; c++) {
                    double piece_w = piece_len * scale;

                    /* Fill piece with grayscale */
                    double gray = GRAYS[color_idx % NUM_GRAYS];
                    cairo_set_source_rgb(cr, gray, gray, gray);
                    cairo_rectangle(cr, x, y, piece_w, bar_height);
                    cairo_fill(cr);

                    /* Piece border */
                    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
                    cairo_set_line_width(cr, 0.5);
                    cairo_rectangle(cr, x, y, piece_w, bar_height);
                    cairo_stroke(cr);

                    /* Piece label: 2 lines (label + length) */
                    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
                    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
                    cairo_set_font_size(cr, 9);

                    if (piece_w > 35) {
                        /* Line 1: label */
                        cairo_move_to(cr, x + 3, y + 14);
                        cairo_show_text(cr, piece->label);

                        /* Line 2: length */
                        char len_str[32];
                        snprintf(len_str, sizeof(len_str), "%.0fmm", piece_len);
                        cairo_move_to(cr, x + 3, y + 26);
                        cairo_show_text(cr, len_str);
                    } else if (piece_w > 18) {
                        /* Narrow: length only */
                        char len_str[16];
                        snprintf(len_str, sizeof(len_str), "%.0f", piece_len);
                        cairo_set_font_size(cr, 8);
                        cairo_move_to(cr, x + 2, y + 20);
                        cairo_show_text(cr, len_str);
                    }

                    x += piece_w;

                    /* Draw kerf (dark gray) */
                    if (kerf > 0 && x < bar_x + bar_w - 5) {
                        double kerf_w = kerf * scale;
                        cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
                        cairo_rectangle(cr, x, y, kerf_w, bar_height);
                        cairo_fill(cr);
                        x += kerf_w;
                    }
                }
                color_idx++;
            }

            /* Draw waste (remaining space) */
            double waste_w = bar_x + bar_w - x;
            if (waste_w > 2 && pattern->waste > 0) {
                /* Light gray fill */
                cairo_set_source_rgb(cr, 0.92, 0.92, 0.92);
                cairo_rectangle(cr, x, y, waste_w, bar_height);
                cairo_fill(cr);

                /* Waste border */
                cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
                cairo_set_line_width(cr, 0.5);
                cairo_rectangle(cr, x, y, waste_w, bar_height);
                cairo_stroke(cr);

                /* Waste label: 2 lines (Chute #ID + length) */
                if (waste_w > 30) {
                    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
                    cairo_set_font_size(cr, 8);
                    cairo_move_to(cr, x + 3, y + 14);

                    /* Show offcut ID if available (after stock update) */
                    int64_t offcut_id = self->solution->offcut_ids[j];
                    if (offcut_id > 0 && offcut_id < 1000000) {  /* Sanity check */
                        char chute_label[32];
                        snprintf(chute_label, sizeof(chute_label), "Chute #%" PRId64, offcut_id);
                        cairo_show_text(cr, chute_label);
                    } else {
                        cairo_show_text(cr, "Chute");
                    }

                    char waste_str[32];
                    snprintf(waste_str, sizeof(waste_str), "%.0fmm", pattern->waste);
                    cairo_move_to(cr, x + 3, y + 26);
                    cairo_show_text(cr, waste_str);
                } else if (waste_w > 15) {
                    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
                    cairo_set_font_size(cr, 7);
                    char waste_str[16];
                    snprintf(waste_str, sizeof(waste_str), "%.0f", pattern->waste);
                    cairo_move_to(cr, x + 2, y + 20);
                    cairo_show_text(cr, waste_str);
                }
            }

            y += bar_spacing;
            bar_num++;
        }
    }

    /* Update size request */
    int required_height = (int)(y + margin);
    if (required_height > height) {
        gtk_widget_set_size_request(GTK_WIDGET(area), -1, required_height);
    }
}

static void on_export_pdf_response(GObject *source, GAsyncResult *result, gpointer user_data)
{
    ResultsView *self = user_data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(dialog, result, &error);

    if (error) {
        /* User cancelled or error occurred */
        g_error_free(error);
        return;
    }

    if (file && self->has_results) {
        char *path = g_file_get_path(file);
        int ret = pdf_export_solution(path, self->instance, self->solution);
        if (ret == 0) {
            g_print("PDF exporte: %s\n", path);
        } else {
            g_printerr("Erreur export PDF: %s\n", path);
        }
        g_free(path);
        g_object_unref(file);
    }
}

static void on_export_pdf_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    ResultsView *self = CUTTER_RESULTS_VIEW(user_data);

    if (!self->has_results) return;

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Exporter en PDF");
    gtk_file_dialog_set_initial_name(dialog, "plan_coupe.pdf");

    gtk_file_dialog_save(dialog,
                         GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(self))),
                         NULL,
                         on_export_pdf_response,
                         self);
}

static void on_update_stock_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    ResultsView *self = CUTTER_RESULTS_VIEW(user_data);

    if (!self->has_results || self->stock_updated) return;

    /* Update inventory: decrement used stock, save usable offcuts */
    int ret = db_record_cutting_session(self->instance, self->solution, TRUE);

    if (ret == 0) {
        self->stock_updated = TRUE;
        gtk_widget_set_sensitive(self->update_stock_button, FALSE);
        gtk_button_set_label(GTK_BUTTON(self->update_stock_button), "Stock MAJ");

        /* Enable PDF export now that offcut IDs are available */
        gtk_widget_set_sensitive(self->export_pdf_button, TRUE);

        /* Refresh stock view */
        GtkWidget *stack = gtk_widget_get_parent(GTK_WIDGET(self));
        if (stack && GTK_IS_STACK(stack)) {
            GtkWidget *stock_view = gtk_stack_get_child_by_name(GTK_STACK(stack), "stock");
            if (stock_view) {
                inventory_view_refresh(CUTTER_INVENTORY_VIEW(stock_view));
            }
        }

        /* Redraw diagram to show offcut IDs */
        gtk_widget_queue_draw(self->diagram_area);

        g_print("Stock mis a jour avec succes\n");
    } else {
        g_printerr("Erreur lors de la mise a jour du stock\n");
    }
}

void results_view_set_solution(ResultsView *self,
                                const CSPInstance *instance,
                                const CSPSolution *solution)
{
    /* Free previous data */
    g_free(self->instance);
    g_free(self->solution);

    /* Copy new data */
    self->instance = g_memdup2(instance, sizeof(CSPInstance));
    self->solution = g_memdup2(solution, sizeof(CSPSolution));
    self->has_results = TRUE;

    /* Update labels */
    char buf[64];

    snprintf(buf, sizeof(buf), "%d", solution->num_bars_used);
    gtk_label_set_text(GTK_LABEL(self->bars_label), buf);

    snprintf(buf, sizeof(buf), "%.0f mm", solution->total_waste);
    gtk_label_set_text(GTK_LABEL(self->waste_label), buf);

    snprintf(buf, sizeof(buf), "%.1f%%", solution->efficiency);
    gtk_label_set_text(GTK_LABEL(self->efficiency_label), buf);

    gtk_label_set_text(GTK_LABEL(self->optimal_label),
                       solution->is_optimal ? "Oui" : "Non");

    /* Show results, hide placeholder */
    gtk_widget_set_visible(self->no_results_label, FALSE);

    /* Reset stock update state - PDF export disabled until stock updated */
    self->stock_updated = FALSE;
    gtk_widget_set_sensitive(self->update_stock_button, TRUE);
    gtk_button_set_label(GTK_BUTTON(self->update_stock_button), "MAJ Stock");
    gtk_widget_set_sensitive(self->export_pdf_button, FALSE);

    /* Redraw diagram */
    gtk_widget_queue_draw(self->diagram_area);
}

static void results_view_finalize(GObject *object)
{
    ResultsView *self = CUTTER_RESULTS_VIEW(object);
    g_free(self->instance);
    g_free(self->solution);
    G_OBJECT_CLASS(results_view_parent_class)->finalize(object);
}

static void results_view_class_init(ResultsViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = results_view_finalize;
}

static void results_view_init(ResultsView *self)
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(GTK_WIDGET(self), 12);
    gtk_widget_set_margin_end(GTK_WIDGET(self), 12);
    gtk_widget_set_margin_top(GTK_WIDGET(self), 12);
    gtk_widget_set_margin_bottom(GTK_WIDGET(self), 12);
    gtk_box_set_spacing(GTK_BOX(self), 12);

    self->instance = NULL;
    self->solution = NULL;
    self->has_results = FALSE;
    self->stock_updated = FALSE;

    /* Header */
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *title = gtk_label_new("Résultats");
    gtk_widget_add_css_class(title, "title-2");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_box_append(GTK_BOX(header), title);

    self->update_stock_button = gtk_button_new_with_label("MAJ Stock");
    gtk_widget_add_css_class(self->update_stock_button, "suggested-action");
    gtk_widget_set_sensitive(self->update_stock_button, FALSE);
    g_signal_connect(self->update_stock_button, "clicked", G_CALLBACK(on_update_stock_clicked), self);
    gtk_box_append(GTK_BOX(header), self->update_stock_button);

    self->export_pdf_button = gtk_button_new_with_label("Exporter PDF");
    gtk_widget_set_sensitive(self->export_pdf_button, FALSE);
    g_signal_connect(self->export_pdf_button, "clicked", G_CALLBACK(on_export_pdf_clicked), self);
    gtk_box_append(GTK_BOX(header), self->export_pdf_button);

    gtk_box_append(GTK_BOX(self), header);

    /* Summary frame */
    GtkWidget *summary_frame = gtk_frame_new("Resume");
    GtkWidget *summary_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(summary_grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(summary_grid), 24);
    gtk_widget_set_margin_start(summary_grid, 12);
    gtk_widget_set_margin_end(summary_grid, 12);
    gtk_widget_set_margin_top(summary_grid, 12);
    gtk_widget_set_margin_bottom(summary_grid, 12);

    gtk_grid_attach(GTK_GRID(summary_grid), gtk_label_new("Barres utilisees:"), 0, 0, 1, 1);
    self->bars_label = gtk_label_new("-");
    gtk_label_set_xalign(GTK_LABEL(self->bars_label), 0);
    gtk_grid_attach(GTK_GRID(summary_grid), self->bars_label, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(summary_grid), gtk_label_new("Chute totale:"), 2, 0, 1, 1);
    self->waste_label = gtk_label_new("-");
    gtk_label_set_xalign(GTK_LABEL(self->waste_label), 0);
    gtk_grid_attach(GTK_GRID(summary_grid), self->waste_label, 3, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(summary_grid), gtk_label_new("Rendement:"), 0, 1, 1, 1);
    self->efficiency_label = gtk_label_new("-");
    gtk_label_set_xalign(GTK_LABEL(self->efficiency_label), 0);
    gtk_grid_attach(GTK_GRID(summary_grid), self->efficiency_label, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(summary_grid), gtk_label_new("Optimal:"), 2, 1, 1, 1);
    self->optimal_label = gtk_label_new("-");
    gtk_label_set_xalign(GTK_LABEL(self->optimal_label), 0);
    gtk_grid_attach(GTK_GRID(summary_grid), self->optimal_label, 3, 1, 1, 1);

    gtk_frame_set_child(GTK_FRAME(summary_frame), summary_grid);
    gtk_box_append(GTK_BOX(self), summary_frame);

    /* Diagram frame */
    GtkWidget *diagram_frame = gtk_frame_new("Schema de Decoupe");
    gtk_widget_set_vexpand(diagram_frame, TRUE);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    self->diagram_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(self->diagram_area, 600, 400);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(self->diagram_area),
                                    draw_diagram, self, NULL);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), self->diagram_area);
    gtk_frame_set_child(GTK_FRAME(diagram_frame), scroll);
    gtk_box_append(GTK_BOX(self), diagram_frame);

    /* No results placeholder */
    self->no_results_label = gtk_label_new("Lancez une optimisation pour voir les resultats");
    gtk_widget_add_css_class(self->no_results_label, "dim-label");
    gtk_box_append(GTK_BOX(self), self->no_results_label);
}

GtkWidget *results_view_new(void)
{
    return g_object_new(CUTTER_TYPE_RESULTS_VIEW, NULL);
}
