/*
 * Copyright (C) 1996 by Rob McMullen
 * GTK4 port: Barak A. Pearlmutter <bap@debian.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <gtk/gtk.h>
#include "SciPlot.h"
#include "SciPlotUtil.h"

typedef struct { int argc; char **argv; } AppData;

static void
ArgProcess(GtkWidget *parent, int argc, char *argv[])
{
  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      FILE *fd = fopen(argv[i], "r");
      if (fd) { SciPlotReadDataFile(parent, fd); fclose(fd); }
    }
  } else {
    SciPlotReadDataFile(parent, stdin);
  }
}

static void
on_activate(GtkApplication *application, gpointer user_data)
{
  AppData *d = (AppData *)user_data;
  GtkWidget *root = gtk_application_window_new(application);
  gtk_widget_set_visible(root, FALSE);
  ArgProcess(root, d->argc, d->argv);
}

int
main(int argc, char *argv[])
{
  AppData data = { argc, argv };
  GtkApplication *app = gtk_application_new("org.sciplot.xyplot",
                                            G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), &data);
  /* pass argc=1 so GTK doesn't consume our file arguments */
  int status = g_application_run(G_APPLICATION(app), 1, argv);
  g_object_unref(app);
  return status;
}
