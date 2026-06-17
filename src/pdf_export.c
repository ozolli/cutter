/*
 * pdf_export.c - PDF export implementation using Cairo
 */

#include "pdf_export.h"
#include "db.h"
#include <cairo.h>
#include <cairo-pdf.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>

/* Page dimensions (A3 portrait in points: 1 point = 1/72 inch) */
#define PAGE_WIDTH  841.89   /* 297mm */
#define PAGE_HEIGHT 1190.55  /* 420mm */
#define MARGIN      30.0
#define BAR_HEIGHT  22.0
#define BAR_SPACING 42.0
#define HEADER_HEIGHT 70.0

/* Grayscale shades for pieces (alternating light/dark) */
static const double GRAYS[] = {
    0.95,  /* Very light gray */
    0.80,  /* Light gray */
    0.90,  /* Lighter gray */
    0.75,  /* Medium light gray */
    0.85,  /* Light gray alt */
    0.70,  /* Medium gray */
};
#define NUM_GRAYS 6

/* Draw header with statistics */
static void draw_header(cairo_t *cr, int page, int total_pages)
{
    char buf[256];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    /* Title */
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16);
    cairo_move_to(cr, MARGIN, MARGIN + 16);
    cairo_show_text(cr, "PLAN DE DÉCOUPE - Cutter");

    /* Date and page */
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);
    strftime(buf, sizeof(buf), "Date: %d/%m/%Y", tm);
    cairo_move_to(cr, PAGE_WIDTH - MARGIN - 150, MARGIN + 16);
    cairo_show_text(cr, buf);

    snprintf(buf, sizeof(buf), "Page %d/%d", page, total_pages);
    cairo_move_to(cr, PAGE_WIDTH - MARGIN - 60, MARGIN + 30);
    cairo_show_text(cr, buf);

    /* Separator line */
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, MARGIN, MARGIN + 55);
    cairo_line_to(cr, PAGE_WIDTH - MARGIN, MARGIN + 55);
    cairo_stroke(cr);
}

/* Draw pieces list table */
static double draw_pieces_list(cairo_t *cr, double start_y,
                               const CSPInstance *instance,
                               const CSPSolution *solution)
{
    char buf[256];
    double y = start_y;
    double col_label = MARGIN;
    double col_mat = MARGIN + 180;
    double col_dims = MARGIN + 240;
    double col_length = MARGIN + 360;
    double col_qty = MARGIN + 440;
    double col_prod = MARGIN + 510;
    double row_height = 14;

    /* Section title */
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12);
    cairo_move_to(cr, MARGIN, y);
    cairo_show_text(cr, "LISTE DES PIECES A DECOUPER");
    y += 20;

    /* Table header */
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 9);

    cairo_move_to(cr, col_label, y);
    cairo_show_text(cr, "Label");
    cairo_move_to(cr, col_mat, y);
    cairo_show_text(cr, "Matiere");
    cairo_move_to(cr, col_dims, y);
    cairo_show_text(cr, "Dimensions");
    cairo_move_to(cr, col_length, y);
    cairo_show_text(cr, "Longueur");
    cairo_move_to(cr, col_qty, y);
    cairo_show_text(cr, "Qte dem.");
    cairo_move_to(cr, col_prod, y);
    cairo_show_text(cr, "Qte prod.");

    /* Header line */
    y += 4;
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_set_line_width(cr, 0.5);
    cairo_move_to(cr, MARGIN, y);
    cairo_line_to(cr, PAGE_WIDTH - MARGIN, y);
    cairo_stroke(cr);
    y += row_height;

    /* Table rows */
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 8);

    for (int i = 0; i < instance->num_pieces; i++) {
        const PieceDemand *piece = &instance->pieces[i];

        /* Calculate produced quantity */
        int produced = 0;
        for (int j = 0; j < instance->num_patterns; j++) {
            produced += solution->pattern_usage[j] * instance->patterns[j].cuts[i];
        }

        /* Alternate row background */
        if (i % 2 == 0) {
            cairo_set_source_rgb(cr, 0.96, 0.96, 0.96);
            cairo_rectangle(cr, MARGIN, y - row_height + 4, PAGE_WIDTH - 2 * MARGIN, row_height);
            cairo_fill(cr);
        }

        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

        /* Label */
        cairo_move_to(cr, col_label, y);
        cairo_show_text(cr, piece->label);

        /* Material */
        cairo_move_to(cr, col_mat, y);
        cairo_show_text(cr, material_to_string(piece->material));

        /* Dimensions */
        if (piece->diameter > 0) {
            snprintf(buf, sizeof(buf), "D%.0f x %.1f mm", piece->diameter, piece->thickness);
        } else {
            snprintf(buf, sizeof(buf), "-");
        }
        cairo_move_to(cr, col_dims, y);
        cairo_show_text(cr, buf);

        /* Length */
        snprintf(buf, sizeof(buf), "%.0f mm", piece->length);
        cairo_move_to(cr, col_length, y);
        cairo_show_text(cr, buf);

        /* Quantity demanded */
        snprintf(buf, sizeof(buf), "%d", piece->quantity);
        cairo_move_to(cr, col_qty, y);
        cairo_show_text(cr, buf);

        /* Quantity produced with status indicator */
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        int diff = produced - piece->quantity;
        if (diff == 0) {
            snprintf(buf, sizeof(buf), "%d", produced);
        } else if (diff > 0) {
            snprintf(buf, sizeof(buf), "%d (+%d)", produced, diff);
        } else {
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            snprintf(buf, sizeof(buf), "%d (%d) !", produced, diff);
        }
        cairo_move_to(cr, col_prod, y);
        cairo_show_text(cr, buf);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

        y += row_height;
    }

    return y;
}

/* Draw a single bar with its cuts */
static void draw_bar(cairo_t *cr, double y,
                     const CSPInstance *instance,
                     const CuttingPattern *pattern,
                     double max_stock_length,
                     int64_t offcut_id)
{
    char buf[128];
    const StockBar *stock = &instance->stocks[pattern->stock_id];
    double draw_width = PAGE_WIDTH - 2 * MARGIN;
    double scale = draw_width / max_stock_length;
    double bar_width = stock->length * scale;

    /* Bar label */
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 7);
    const char *mat_str = material_to_string(stock->material);
    if (mat_str[0] != '-') {
        snprintf(buf, sizeof(buf), "#%d: %s %s (%.0fmm)", stock->id, stock->label, mat_str, stock->length);
    } else {
        snprintf(buf, sizeof(buf), "#%d: %s (%.0fmm)", stock->id, stock->label, stock->length);
    }
    cairo_move_to(cr, MARGIN, y - 4);
    cairo_show_text(cr, buf);

    /* Pattern ID on the right */
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 6);
    snprintf(buf, sizeof(buf), "P%d", pattern->pattern_id);
    cairo_move_to(cr, PAGE_WIDTH - MARGIN - 20, y - 4);
    cairo_show_text(cr, buf);

    /* Draw bar outline */
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, MARGIN, y, bar_width, BAR_HEIGHT);
    cairo_stroke(cr);

    /* Draw pieces */
    double x = MARGIN;
    int color_idx = 0;

    for (int i = 0; i < instance->num_pieces; i++) {
        int count = pattern->cuts[i];
        if (count <= 0) continue;

        const PieceDemand *piece = &instance->pieces[i];
        double piece_width = piece->length * scale;

        for (int k = 0; k < count; k++) {
            /* Fill piece with grayscale */
            double gray = GRAYS[color_idx % NUM_GRAYS];
            cairo_set_source_rgb(cr, gray, gray, gray);
            cairo_rectangle(cr, x, y, piece_width, BAR_HEIGHT);
            cairo_fill(cr);

            /* Piece border */
            cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
            cairo_set_line_width(cr, 0.5);
            cairo_rectangle(cr, x, y, piece_width, BAR_HEIGHT);
            cairo_stroke(cr);

            /* Piece label: 2 lines (label + length) */
            cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 6);

            if (piece_width > 25) {
                /* Line 1: label */
                cairo_move_to(cr, x + 2, y + 9);
                cairo_show_text(cr, piece->label);

                /* Line 2: length */
                snprintf(buf, sizeof(buf), "%.0fmm", piece->length);
                cairo_move_to(cr, x + 2, y + 18);
                cairo_show_text(cr, buf);
            } else if (piece_width > 12) {
                /* Very narrow: length only */
                snprintf(buf, sizeof(buf), "%.0f", piece->length);
                cairo_move_to(cr, x + 1, y + 14);
                cairo_show_text(cr, buf);
            }

            x += piece_width;

            /* Draw kerf (if not last piece) - dark gray */
            if (x < MARGIN + bar_width - 5) {
                double kerf_width = instance->saw_kerf * scale;
                cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
                cairo_rectangle(cr, x, y, kerf_width, BAR_HEIGHT);
                cairo_fill(cr);
                x += kerf_width;
            }
        }
        color_idx++;
    }

    /* Draw waste (remaining space) - diagonal hatch pattern */
    if (pattern->waste > 0 && x < MARGIN + bar_width) {
        double waste_width = MARGIN + bar_width - x;

        /* Light gray fill */
        cairo_set_source_rgb(cr, 0.92, 0.92, 0.92);
        cairo_rectangle(cr, x, y, waste_width, BAR_HEIGHT);
        cairo_fill(cr);

        /* Waste border */
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_set_line_width(cr, 0.5);
        cairo_rectangle(cr, x, y, waste_width, BAR_HEIGHT);
        cairo_stroke(cr);

        /* Waste label: 2 lines (Chute #ID + length) */
        if (waste_width > 20) {
            cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
            cairo_set_font_size(cr, 5);
            cairo_move_to(cr, x + 2, y + 9);
            if (offcut_id > 0 && offcut_id < 1000000) {  /* Sanity check */
                snprintf(buf, sizeof(buf), "Chute #%" PRId64, offcut_id);
                cairo_show_text(cr, buf);
            } else {
                cairo_show_text(cr, "Chute");
            }
            snprintf(buf, sizeof(buf), "%.0fmm", pattern->waste);
            cairo_move_to(cr, x + 2, y + 17);
            cairo_show_text(cr, buf);
        }
    }
}

int pdf_export_solution(const char *filename,
                        const CSPInstance *instance,
                        const CSPSolution *solution)
{
    /* Count total bars to draw */
    int total_bars = 0;
    for (int j = 0; j < instance->num_patterns; j++) {
        total_bars += solution->pattern_usage[j];
    }

    if (total_bars == 0) {
        fprintf(stderr, "Erreur: Aucune barre a dessiner\n");
        return -1;
    }

    /* Find maximum stock length for scaling */
    double max_stock_length = 0;
    for (int s = 0; s < instance->num_stocks; s++) {
        if (instance->stocks[s].length > max_stock_length) {
            max_stock_length = instance->stocks[s].length;
        }
    }

    /* Calculate bars per page */
    double usable_height = PAGE_HEIGHT - MARGIN - HEADER_HEIGHT - MARGIN;
    int bars_per_page = (int)(usable_height / BAR_SPACING);
    if (bars_per_page < 1) bars_per_page = 1;

    int total_pages = (total_bars + bars_per_page - 1) / bars_per_page;

    /* Create PDF surface */
    cairo_surface_t *surface = cairo_pdf_surface_create(filename, PAGE_WIDTH, PAGE_HEIGHT);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Erreur: Impossible de creer le PDF %s\n", filename);
        return -1;
    }

    cairo_t *cr = cairo_create(surface);

    /* Draw each bar */
    int bar_num = 1;
    int bar_on_page = 0;
    int current_page = 1;

    /* Start first page */
    draw_header(cr, current_page, total_pages);

    for (int j = 0; j < instance->num_patterns; j++) {
        int usage = solution->pattern_usage[j];
        if (usage <= 0) continue;

        const CuttingPattern *pattern = &instance->patterns[j];

        for (int k = 0; k < usage; k++) {
            /* Check if we need a new page */
            if (bar_on_page >= bars_per_page) {
                cairo_show_page(cr);
                current_page++;
                draw_header(cr, current_page, total_pages);
                bar_on_page = 0;
            }

            /* Calculate Y position */
            double y = MARGIN + HEADER_HEIGHT + bar_on_page * BAR_SPACING;

            /* Draw the bar */
            draw_bar(cr, y, instance, pattern, max_stock_length, solution->offcut_ids[j]);

            bar_num++;
            bar_on_page++;
        }
    }

    /* Add pieces list after the bars */
    double current_y = MARGIN + HEADER_HEIGHT + bar_on_page * BAR_SPACING;
    double remaining_space = PAGE_HEIGHT - MARGIN - current_y;

    /* Start new page if not enough space (need at least header + 5 rows) */
    if (remaining_space < 120) {
        cairo_show_page(cr);
        current_page++;
        draw_header(cr, current_page, total_pages);
        current_y = MARGIN + HEADER_HEIGHT;
    }

    /* Draw pieces list */
    draw_pieces_list(cr, current_y + 20, instance, solution);

    /* Finalize */
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    printf("PDF exporte: %s (%d pages)\n", filename, total_pages);
    return 0;
}

/* A4 page dimensions in points */
#define A4_WIDTH  595.28   /* 210mm */
#define A4_HEIGHT 841.89   /* 297mm */
#define STOCK_MARGIN 30.0
#define STOCK_ROW_HEIGHT 16.0

int pdf_export_stock(const char *filename, const StockItem *items, int count)
{
    if (count == 0) {
        fprintf(stderr, "Erreur: Aucun stock a exporter\n");
        return -1;
    }

    /* Create PDF surface (A4) */
    cairo_surface_t *surface = cairo_pdf_surface_create(filename, A4_WIDTH, A4_HEIGHT);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Erreur: Impossible de creer le PDF %s\n", filename);
        return -1;
    }

    cairo_t *cr = cairo_create(surface);
    char buf[256];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    /* Calculate rows per page */
    double usable_height = A4_HEIGHT - 2 * STOCK_MARGIN - 60;  /* header space */
    int rows_per_page = (int)(usable_height / STOCK_ROW_HEIGHT) - 2;  /* -2 for header row */
    int total_pages = (count + rows_per_page - 1) / rows_per_page;

    /* Column positions */
    double col_id = STOCK_MARGIN;
    double col_label = STOCK_MARGIN + 30;
    double col_mat = STOCK_MARGIN + 150;
    double col_dims = STOCK_MARGIN + 200;
    double col_length = STOCK_MARGIN + 290;
    double col_qty = STOCK_MARGIN + 360;
    double col_type = STOCK_MARGIN + 400;

    int current_page = 1;
    int item_idx = 0;

    while (item_idx < count) {
        /* Page header */
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 14);
        cairo_move_to(cr, STOCK_MARGIN, STOCK_MARGIN + 14);
        cairo_show_text(cr, "INVENTAIRE STOCK - Cutter");

        /* Date and page */
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 9);
        strftime(buf, sizeof(buf), "Date: %d/%m/%Y", tm);
        cairo_move_to(cr, A4_WIDTH - STOCK_MARGIN - 120, STOCK_MARGIN + 14);
        cairo_show_text(cr, buf);

        snprintf(buf, sizeof(buf), "Page %d/%d", current_page, total_pages);
        cairo_move_to(cr, A4_WIDTH - STOCK_MARGIN - 50, STOCK_MARGIN + 28);
        cairo_show_text(cr, buf);

        /* Separator line */
        cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, STOCK_MARGIN, STOCK_MARGIN + 40);
        cairo_line_to(cr, A4_WIDTH - STOCK_MARGIN, STOCK_MARGIN + 40);
        cairo_stroke(cr);

        double y = STOCK_MARGIN + 55;

        /* Table header */
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 8);

        cairo_move_to(cr, col_id, y);
        cairo_show_text(cr, "ID");
        cairo_move_to(cr, col_label, y);
        cairo_show_text(cr, "Label");
        cairo_move_to(cr, col_mat, y);
        cairo_show_text(cr, "Matiere");
        cairo_move_to(cr, col_dims, y);
        cairo_show_text(cr, "Dimensions");
        cairo_move_to(cr, col_length, y);
        cairo_show_text(cr, "Longueur");
        cairo_move_to(cr, col_qty, y);
        cairo_show_text(cr, "Qte");
        cairo_move_to(cr, col_type, y);
        cairo_show_text(cr, "Type");

        /* Header underline */
        y += 4;
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_set_line_width(cr, 0.5);
        cairo_move_to(cr, STOCK_MARGIN, y);
        cairo_line_to(cr, A4_WIDTH - STOCK_MARGIN, y);
        cairo_stroke(cr);
        y += STOCK_ROW_HEIGHT;

        /* Table rows */
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 8);

        int rows_on_page = 0;
        while (item_idx < count && rows_on_page < rows_per_page) {
            const StockItem *item = &items[item_idx];

            /* Alternate row background */
            if (rows_on_page % 2 == 0) {
                cairo_set_source_rgb(cr, 0.96, 0.96, 0.96);
                cairo_rectangle(cr, STOCK_MARGIN, y - STOCK_ROW_HEIGHT + 4,
                                A4_WIDTH - 2 * STOCK_MARGIN, STOCK_ROW_HEIGHT);
                cairo_fill(cr);
            }

            cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

            /* ID */
            snprintf(buf, sizeof(buf), "%ld", (long)item->id);
            cairo_move_to(cr, col_id, y);
            cairo_show_text(cr, buf);

            /* Label */
            cairo_move_to(cr, col_label, y);
            cairo_show_text(cr, item->label);

            /* Material */
            cairo_move_to(cr, col_mat, y);
            cairo_show_text(cr, material_to_string(item->material));

            /* Dimensions */
            if (item->diameter > 0) {
                double thick = item->thickness;
                if (thick == (int)thick) {
                    snprintf(buf, sizeof(buf), "%.0fx%.0f", item->diameter, thick);
                } else {
                    snprintf(buf, sizeof(buf), "%.0fx%.1f", item->diameter, thick);
                }
            } else {
                snprintf(buf, sizeof(buf), "-");
            }
            cairo_move_to(cr, col_dims, y);
            cairo_show_text(cr, buf);

            /* Length */
            snprintf(buf, sizeof(buf), "%.0f mm", item->length);
            cairo_move_to(cr, col_length, y);
            cairo_show_text(cr, buf);

            /* Quantity */
            if (item->quantity < 0) {
                snprintf(buf, sizeof(buf), "illim.");
            } else {
                snprintf(buf, sizeof(buf), "%d", item->quantity);
            }
            cairo_move_to(cr, col_qty, y);
            cairo_show_text(cr, buf);

            /* Type */
            cairo_move_to(cr, col_type, y);
            cairo_show_text(cr, item->is_offcut ? "Chute" : "Stock");

            y += STOCK_ROW_HEIGHT;
            item_idx++;
            rows_on_page++;
        }

        /* New page if more items */
        if (item_idx < count) {
            cairo_show_page(cr);
            current_page++;
        }
    }

    /* Finalize */
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    printf("PDF stock exporte: %s (%d pages)\n", filename, total_pages);
    return 0;
}
