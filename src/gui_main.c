/*
 * gui_main.c - GTK4 GUI entry point for Cutter
 */

#include <gtk/gtk.h>
#include "gui/cutter_app.h"

int main(int argc, char *argv[])
{
    CutterApp *app = cutter_app_new();
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
