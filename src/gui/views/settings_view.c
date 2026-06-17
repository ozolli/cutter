/*
 * settings_view.c - Settings management view
 */

#include "settings_view.h"
#include "../../db.h"
#include "../../settings.h"

struct _SettingsView {
    GtkBox parent_instance;

    GtkWidget *db_path_entry;
    GtkWidget *db_path_button;
    GtkWidget *db_network_check;
    GtkWidget *min_offcut_spin;
    GtkWidget *saw_kerf_spin;
    GtkWidget *save_button;
    GtkWidget *status_label;

    AppSettings settings;
};

G_DEFINE_TYPE(SettingsView, settings_view, GTK_TYPE_BOX)

static void update_status(SettingsView *self, const char *message)
{
    gtk_label_set_text(GTK_LABEL(self->status_label), message);
}

static void on_browse_response(GObject *source, GAsyncResult *result, gpointer user_data)
{
    SettingsView *self = CUTTER_SETTINGS_VIEW(user_data);
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GFile *file = gtk_file_dialog_open_finish(dialog, result, NULL);

    if (file) {
        char *path = g_file_get_path(file);
        if (path) {
            gtk_editable_set_text(GTK_EDITABLE(self->db_path_entry), path);
            g_free(path);
        }
        g_object_unref(file);
    }
}

static void on_browse_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    SettingsView *self = CUTTER_SETTINGS_VIEW(user_data);

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Choisir la base de donnees");

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Bases SQLite (*.db)");
    gtk_file_filter_add_pattern(filter, "*.db");

    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filter);
    g_object_unref(filters);

    /* Set initial folder to current db path's directory */
    const char *current_path = gtk_editable_get_text(GTK_EDITABLE(self->db_path_entry));
    if (current_path && current_path[0]) {
        GFile *current_file = g_file_new_for_path(current_path);
        GFile *parent = g_file_get_parent(current_file);
        if (parent) {
        }
        g_object_unref(current_file);
    }

    gtk_file_dialog_open(dialog,
                         GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(self))),
                         NULL,
                         on_browse_response,
                         self);
    g_object_unref(dialog);
}

static void on_save_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    SettingsView *self = CUTTER_SETTINGS_VIEW(user_data);

    /* Get values from UI */
    const char *db_path = gtk_editable_get_text(GTK_EDITABLE(self->db_path_entry));
    strncpy(self->settings.db_path, db_path, SETTINGS_MAX_PATH - 1);
    self->settings.db_path[SETTINGS_MAX_PATH - 1] = '\0';

    self->settings.min_usable_offcut = gtk_spin_button_get_value(
        GTK_SPIN_BUTTON(self->min_offcut_spin));
    self->settings.saw_kerf = gtk_spin_button_get_value(
        GTK_SPIN_BUTTON(self->saw_kerf_spin));
    self->settings.db_network = gtk_check_button_get_active(
        GTK_CHECK_BUTTON(self->db_network_check));

    /* Save to file */
    if (settings_save(&self->settings) == 0) {
        update_status(self, "Configuration sauvegardee");

        /* Reinitialize database with new path and locking mode */
        db_set_network_mode(self->settings.db_network);
        db_close();
        if (db_init(self->settings.db_path) == 0) {
            update_status(self, "Configuration sauvegardee - Base de donnees rechargee");
        } else {
            update_status(self, "Erreur: Impossible d'ouvrir la base de donnees");
        }
    } else {
        update_status(self, "Erreur lors de la sauvegarde");
    }
}

static void settings_view_class_init(SettingsViewClass *klass)
{
    (void)klass;
}

static void settings_view_init(SettingsView *self)
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(GTK_WIDGET(self), 12);
    gtk_widget_set_margin_end(GTK_WIDGET(self), 12);
    gtk_widget_set_margin_top(GTK_WIDGET(self), 12);
    gtk_widget_set_margin_bottom(GTK_WIDGET(self), 12);
    gtk_box_set_spacing(GTK_BOX(self), 12);

    /* Load current settings */
    settings_load(&self->settings);

    /* Header */
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *title = gtk_label_new("Parametres");
    gtk_widget_add_css_class(title, "title-2");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_box_append(GTK_BOX(header), title);
    gtk_box_append(GTK_BOX(self), header);

    /* Settings frame */
    GtkWidget *frame = gtk_frame_new("Configuration");
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_margin_start(grid, 12);
    gtk_widget_set_margin_end(grid, 12);
    gtk_widget_set_margin_top(grid, 12);
    gtk_widget_set_margin_bottom(grid, 12);

    int row = 0;

    /* Database path */
    GtkWidget *db_label = gtk_label_new("Base de donnees:");
    gtk_label_set_xalign(GTK_LABEL(db_label), 0);
    gtk_grid_attach(GTK_GRID(grid), db_label, 0, row, 1, 1);

    GtkWidget *db_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    self->db_path_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(self->db_path_entry), self->settings.db_path);
    gtk_widget_set_hexpand(self->db_path_entry, TRUE);
    gtk_editable_set_width_chars(GTK_EDITABLE(self->db_path_entry), 50);
    gtk_box_append(GTK_BOX(db_box), self->db_path_entry);

    self->db_path_button = gtk_button_new_with_label("Parcourir...");
    g_signal_connect(self->db_path_button, "clicked", G_CALLBACK(on_browse_clicked), self);
    gtk_box_append(GTK_BOX(db_box), self->db_path_button);

    gtk_grid_attach(GTK_GRID(grid), db_box, 1, row, 2, 1);
    row++;

    /* Network-share mode (SMB/NFS) */
    GtkWidget *net_label = gtk_label_new("Base sur le reseau:");
    gtk_label_set_xalign(GTK_LABEL(net_label), 0);
    gtk_grid_attach(GTK_GRID(grid), net_label, 0, row, 1, 1);

    self->db_network_check = gtk_check_button_new_with_label(
        "Partage reseau (SMB/NFS) - verrouillage par fichier .lock");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(self->db_network_check),
                                self->settings.db_network);
    gtk_widget_set_tooltip_text(self->db_network_check,
        "A activer si la base est sur un serveur de fichiers (Synology SMB, NAS, NFS). "
        "Necessaire pour pouvoir ecrire ; les postes doivent eviter d'ecrire exactement en meme temps.");
    gtk_grid_attach(GTK_GRID(grid), self->db_network_check, 1, row, 2, 1);
    row++;

    /* Config file path (read-only info) */
    GtkWidget *conf_label = gtk_label_new("Fichier de configuration:");
    gtk_label_set_xalign(GTK_LABEL(conf_label), 0);
    gtk_grid_attach(GTK_GRID(grid), conf_label, 0, row, 1, 1);

    GtkWidget *conf_path = gtk_label_new(settings_file_path());
    gtk_label_set_xalign(GTK_LABEL(conf_path), 0);
    gtk_widget_add_css_class(conf_path, "dim-label");
    gtk_label_set_selectable(GTK_LABEL(conf_path), TRUE);
    gtk_grid_attach(GTK_GRID(grid), conf_path, 1, row, 2, 1);
    row++;

    /* Separator */
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_grid_attach(GTK_GRID(grid), sep, 0, row, 3, 1);
    row++;

    /* Min usable offcut */
    GtkWidget *offcut_label = gtk_label_new("Chute minimale utilisable:");
    gtk_label_set_xalign(GTK_LABEL(offcut_label), 0);
    gtk_grid_attach(GTK_GRID(grid), offcut_label, 0, row, 1, 1);

    self->min_offcut_spin = gtk_spin_button_new_with_range(0, 5000, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(self->min_offcut_spin),
                              self->settings.min_usable_offcut);
    gtk_grid_attach(GTK_GRID(grid), self->min_offcut_spin, 1, row, 1, 1);

    GtkWidget *offcut_unit = gtk_label_new("mm");
    gtk_label_set_xalign(GTK_LABEL(offcut_unit), 0);
    gtk_grid_attach(GTK_GRID(grid), offcut_unit, 2, row, 1, 1);
    row++;

    /* Saw kerf */
    GtkWidget *kerf_label = gtk_label_new("Trait de scie:");
    gtk_label_set_xalign(GTK_LABEL(kerf_label), 0);
    gtk_grid_attach(GTK_GRID(grid), kerf_label, 0, row, 1, 1);

    self->saw_kerf_spin = gtk_spin_button_new_with_range(0, 20, 0.5);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(self->saw_kerf_spin), 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(self->saw_kerf_spin),
                              self->settings.saw_kerf);
    gtk_grid_attach(GTK_GRID(grid), self->saw_kerf_spin, 1, row, 1, 1);

    GtkWidget *kerf_unit = gtk_label_new("mm");
    gtk_label_set_xalign(GTK_LABEL(kerf_unit), 0);
    gtk_grid_attach(GTK_GRID(grid), kerf_unit, 2, row, 1, 1);
    row++;

    gtk_frame_set_child(GTK_FRAME(frame), grid);
    gtk_box_append(GTK_BOX(self), frame);

    /* Save button */
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);

    self->status_label = gtk_label_new("");
    gtk_widget_add_css_class(self->status_label, "dim-label");
    gtk_widget_set_hexpand(self->status_label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(self->status_label), 0);
    gtk_box_append(GTK_BOX(button_box), self->status_label);

    self->save_button = gtk_button_new_with_label("Enregistrer");
    gtk_widget_add_css_class(self->save_button, "suggested-action");
    g_signal_connect(self->save_button, "clicked", G_CALLBACK(on_save_clicked), self);
    gtk_box_append(GTK_BOX(button_box), self->save_button);

    gtk_box_append(GTK_BOX(self), button_box);
}

GtkWidget *settings_view_new(void)
{
    return g_object_new(CUTTER_TYPE_SETTINGS_VIEW, NULL);
}
