/*
 * settings_view.h - Settings management view
 */

#ifndef SETTINGS_VIEW_H
#define SETTINGS_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CUTTER_TYPE_SETTINGS_VIEW (settings_view_get_type())
G_DECLARE_FINAL_TYPE(SettingsView, settings_view, CUTTER, SETTINGS_VIEW, GtkBox)

GtkWidget *settings_view_new(void);

G_END_DECLS

#endif /* SETTINGS_VIEW_H */
