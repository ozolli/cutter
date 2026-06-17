/*
 * cutter_app.c - GtkApplication subclass for Cutter
 */

#include "cutter_app.h"
#include "cutter_window.h"
#include "../db.h"
#include "../settings.h"

struct _CutterApp {
    GtkApplication parent_instance;
};

G_DEFINE_TYPE(CutterApp, cutter_app, GTK_TYPE_APPLICATION)

static void cutter_app_activate(GApplication *app)
{
    CutterWindow *window;

    /* Load settings and initialize database */
    AppSettings settings;
    settings_load(&settings);
    db_init(settings.db_path);

    window = cutter_window_new(CUTTER_APP(app));
    gtk_window_present(GTK_WINDOW(window));
}

static void cutter_app_shutdown(GApplication *app)
{
    /* Close database */
    db_close();

    G_APPLICATION_CLASS(cutter_app_parent_class)->shutdown(app);
}

static void cutter_app_class_init(CutterAppClass *klass)
{
    GApplicationClass *app_class = G_APPLICATION_CLASS(klass);

    app_class->activate = cutter_app_activate;
    app_class->shutdown = cutter_app_shutdown;
}

static void cutter_app_init(CutterApp *self)
{
    (void)self;
}

CutterApp *cutter_app_new(void)
{
    return g_object_new(CUTTER_TYPE_APP,
                        "application-id", "com.github.cutter",
                        "flags", G_APPLICATION_DEFAULT_FLAGS,
                        NULL);
}
