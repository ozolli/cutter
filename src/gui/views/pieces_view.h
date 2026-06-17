/*
 * pieces_view.h - Pieces input view
 */

#ifndef PIECES_VIEW_H
#define PIECES_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CUTTER_TYPE_PIECES_VIEW (pieces_view_get_type())
G_DECLARE_FINAL_TYPE(PiecesView, pieces_view, CUTTER, PIECES_VIEW, GtkBox)

GtkWidget *pieces_view_new(void);
void pieces_view_clear(PiecesView *self);
int pieces_view_get_count(PiecesView *self);
void pieces_view_fill_instance(PiecesView *self, void *instance);

G_END_DECLS

#endif /* PIECES_VIEW_H */
