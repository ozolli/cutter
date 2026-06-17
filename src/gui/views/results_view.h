/*
 * results_view.h - Results display view
 */

#ifndef RESULTS_VIEW_H
#define RESULTS_VIEW_H

#include <gtk/gtk.h>
#include "../../csp_types.h"

G_BEGIN_DECLS

#define CUTTER_TYPE_RESULTS_VIEW (results_view_get_type())
G_DECLARE_FINAL_TYPE(ResultsView, results_view, CUTTER, RESULTS_VIEW, GtkBox)

GtkWidget *results_view_new(void);
void results_view_set_solution(ResultsView *self,
                                const CSPInstance *instance,
                                const CSPSolution *solution);

G_END_DECLS

#endif /* RESULTS_VIEW_H */
