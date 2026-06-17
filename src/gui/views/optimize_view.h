/*
 * optimize_view.h - Optimization control view
 */

#ifndef OPTIMIZE_VIEW_H
#define OPTIMIZE_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CUTTER_TYPE_OPTIMIZE_VIEW (optimize_view_get_type())
G_DECLARE_FINAL_TYPE(OptimizeView, optimize_view, CUTTER, OPTIMIZE_VIEW, GtkBox)

GtkWidget *optimize_view_new(gpointer main_window);

G_END_DECLS

#endif /* OPTIMIZE_VIEW_H */
