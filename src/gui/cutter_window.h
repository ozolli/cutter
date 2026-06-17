/*
 * cutter_window.h - Main application window
 */

#ifndef CUTTER_WINDOW_H
#define CUTTER_WINDOW_H

#include <gtk/gtk.h>
#include "cutter_app.h"

G_BEGIN_DECLS

#define CUTTER_TYPE_WINDOW (cutter_window_get_type())
G_DECLARE_FINAL_TYPE(CutterWindow, cutter_window, CUTTER, WINDOW, GtkApplicationWindow)

CutterWindow *cutter_window_new(CutterApp *app);

/* Navigation */
void cutter_window_show_view(CutterWindow *self, const char *view_name);

/* Status bar */
void cutter_window_set_status(CutterWindow *self, const char *message);

G_END_DECLS

#endif /* CUTTER_WINDOW_H */
