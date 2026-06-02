/*
 * Copyright (C) 1996 by Rob McMullen
 * GTK4 port: Barak A. Pearlmutter <bap@debian.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <gtk/gtk.h>
#include "SciPlot.h"
#include "SciPlotUtil.h"

static const GOptionEntry options[] = {
  {"version", 'V', 0, G_OPTION_ARG_NONE, NULL,
   "Show version information and exit", NULL},
  {NULL}
};

static gint
on_handle_local_options(GApplication *app, GVariantDict *opts, gpointer data)
{
  (void)app; (void)data;
  if (g_variant_dict_contains(opts, "version")) {
    g_print(PACKAGE_STRING "\n");
    return 0;
  }
  return -1;
}

static void
on_activate(GtkApplication *app, gpointer data)
{
  (void)data;
  GtkWidget *root = gtk_application_window_new(app);
  gtk_widget_set_visible(root, FALSE);
  SciPlotReadDataFile(root, stdin);
}

static void
on_open(GtkApplication *app, GFile **files, gint n_files,
        const gchar *hint, gpointer data)
{
  (void)hint; (void)data;
  GtkWidget *root = gtk_application_window_new(app);
  gtk_widget_set_visible(root, FALSE);
  for (int i = 0; i < n_files; i++) {
    char *path = g_file_get_path(files[i]);
    if (path) {
      FILE *fd = fopen(path, "r");
      if (fd) { SciPlotReadDataFile(root, fd); fclose(fd); }
      g_free(path);
    }
  }
}

int
main(int argc, char *argv[])
{
  GtkApplication *app = gtk_application_new("org.sciplot.xyplot",
                                            G_APPLICATION_HANDLES_OPEN);
  g_application_set_option_context_parameter_string(G_APPLICATION(app),
                                                    "[FILE...]");
  g_application_set_option_context_summary(G_APPLICATION(app),
    "Plot x/y or polar data from FILE(s), or standard input if none given.");
  g_application_add_main_option_entries(G_APPLICATION(app), options);
  g_signal_connect(app, "handle-local-options",
                   G_CALLBACK(on_handle_local_options), NULL);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  g_signal_connect(app, "open",     G_CALLBACK(on_open),     NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
