/*
 * cutter_window.c - Main application window with sidebar navigation
 */

#include "cutter_window.h"
#include "views/inventory_view.h"
#include "views/pieces_view.h"
#include "views/optimize_view.h"
#include "views/results_view.h"
#include "views/settings_view.h"

struct _CutterWindow {
    GtkApplicationWindow parent_instance;

    /* Widgets */
    GtkWidget *sidebar;
    GtkWidget *stack;
    GtkWidget *statusbar;

    /* Views */
    GtkWidget *inventory_view;
    GtkWidget *pieces_view;
    GtkWidget *optimize_view;
    GtkWidget *results_view;
    GtkWidget *settings_view;
};

G_DEFINE_TYPE(CutterWindow, cutter_window, GTK_TYPE_APPLICATION_WINDOW)

static GtkWidget *create_sidebar_row(const char *icon_name, const char *label)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);

    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    gtk_box_append(GTK_BOX(box), icon);

    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_hexpand(lbl, TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    gtk_box_append(GTK_BOX(box), lbl);

    return box;
}

static void on_sidebar_row_selected(GtkListBox *listbox, GtkListBoxRow *row,
                                     gpointer user_data)
{
    CutterWindow *self = CUTTER_WINDOW(user_data);

    if (row == NULL) return;

    int index = gtk_list_box_row_get_index(row);
    const char *view_names[] = {"stock", "pièces", "optimiser", "résultats", "paramètres"};

    if (index >= 0 && index < 5) {
        gtk_stack_set_visible_child_name(GTK_STACK(self->stack), view_names[index]);
    }
}

static void cutter_window_class_init(CutterWindowClass *klass)
{
    (void)klass;
}

static void cutter_window_init(CutterWindow *self)
{
    gtk_window_set_title(GTK_WINDOW(self), "Cutter - Optimiseur de Decoupe");
    gtk_window_set_default_size(GTK_WINDOW(self), 1200, 800);

    /* Main vertical box */
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(self), main_box);

    /* Header bar */
    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(self), header);

    /* Main content: Paned with sidebar + stack */
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_box_append(GTK_BOX(main_box), paned);

    /* Sidebar */
    GtkWidget *sidebar_frame = gtk_frame_new(NULL);
    gtk_widget_add_css_class(sidebar_frame, "sidebar");
    gtk_widget_set_size_request(sidebar_frame, 200, -1);

    self->sidebar = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(self->sidebar), GTK_SELECTION_SINGLE);
    gtk_widget_add_css_class(self->sidebar, "navigation-sidebar");

    /* Sidebar items */
    GtkWidget *row1 = create_sidebar_row("folder-symbolic", "Stock");
    gtk_list_box_append(GTK_LIST_BOX(self->sidebar), row1);

    GtkWidget *row2 = create_sidebar_row("document-edit-symbolic", "Pièces");
    gtk_list_box_append(GTK_LIST_BOX(self->sidebar), row2);

    GtkWidget *row3 = create_sidebar_row("system-run-symbolic", "Optimiser");
    gtk_list_box_append(GTK_LIST_BOX(self->sidebar), row3);

    GtkWidget *row4 = create_sidebar_row("emblem-ok-symbolic", "Résultats");
    gtk_list_box_append(GTK_LIST_BOX(self->sidebar), row4);

    GtkWidget *row5 = create_sidebar_row("emblem-system-symbolic", "Paramètres");
    gtk_list_box_append(GTK_LIST_BOX(self->sidebar), row5);

    g_signal_connect(self->sidebar, "row-selected",
                     G_CALLBACK(on_sidebar_row_selected), self);

    GtkWidget *sidebar_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar_scroll),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sidebar_scroll), self->sidebar);
    gtk_frame_set_child(GTK_FRAME(sidebar_frame), sidebar_scroll);

    gtk_paned_set_start_child(GTK_PANED(paned), sidebar_frame);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);

    /* Content stack */
    self->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(self->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(self->stack), 200);
    gtk_widget_set_hexpand(self->stack, TRUE);

    gtk_paned_set_end_child(GTK_PANED(paned), self->stack);
    gtk_paned_set_resize_end_child(GTK_PANED(paned), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(paned), FALSE);

    /* Create views */
    self->inventory_view = inventory_view_new();
    gtk_stack_add_named(GTK_STACK(self->stack), self->inventory_view, "stock");

    self->pieces_view = pieces_view_new();
    gtk_stack_add_named(GTK_STACK(self->stack), self->pieces_view, "pièces");

    self->optimize_view = optimize_view_new(self);
    gtk_stack_add_named(GTK_STACK(self->stack), self->optimize_view, "optimiser");

    self->results_view = results_view_new();
    gtk_stack_add_named(GTK_STACK(self->stack), self->results_view, "résultats");

    self->settings_view = settings_view_new();
    gtk_stack_add_named(GTK_STACK(self->stack), self->settings_view, "paramètres");

    /* Status bar */
    self->statusbar = gtk_label_new("Pret");
    gtk_widget_set_halign(self->statusbar, GTK_ALIGN_START);
    gtk_widget_set_margin_start(self->statusbar, 12);
    gtk_widget_set_margin_end(self->statusbar, 12);
    gtk_widget_set_margin_top(self->statusbar, 6);
    gtk_widget_set_margin_bottom(self->statusbar, 6);
    gtk_box_append(GTK_BOX(main_box), self->statusbar);

    /* Select first sidebar item */
    GtkListBoxRow *first_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(self->sidebar), 0);
    gtk_list_box_select_row(GTK_LIST_BOX(self->sidebar), first_row);
}

CutterWindow *cutter_window_new(CutterApp *app)
{
    return g_object_new(CUTTER_TYPE_WINDOW,
                        "application", app,
                        NULL);
}

void cutter_window_show_view(CutterWindow *self, const char *view_name)
{
    gtk_stack_set_visible_child_name(GTK_STACK(self->stack), view_name);

    /* Update sidebar selection */
    const char *views[] = {"stock", "pièces", "optimiser", "résultats", "paramètres"};
    for (int i = 0; i < 5; i++) {
        if (g_strcmp0(view_name, views[i]) == 0) {
            GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(self->sidebar), i);
            gtk_list_box_select_row(GTK_LIST_BOX(self->sidebar), row);
            break;
        }
    }
}

void cutter_window_set_status(CutterWindow *self, const char *message)
{
    gtk_label_set_text(GTK_LABEL(self->statusbar), message);
}
