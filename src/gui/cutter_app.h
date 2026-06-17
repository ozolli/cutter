/*
 * cutter_app.h - GtkApplication subclass for Cutter
 */

#ifndef CUTTER_APP_H
#define CUTTER_APP_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CUTTER_TYPE_APP (cutter_app_get_type())
G_DECLARE_FINAL_TYPE(CutterApp, cutter_app, CUTTER, APP, GtkApplication)

CutterApp *cutter_app_new(void);

G_END_DECLS

#endif /* CUTTER_APP_H */
