/*
 * Copyright (C) 1995 by Rob McMullen
 * GTK4 port: Barak A. Pearlmutter <bap@debian.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "SciPlot.h"
#include "SciPlotUtil.h"

static int   line_id;
static float xdata[10];
static float ydata[10];

static gboolean
update_cb(gpointer user_data)
{
  GtkWidget *plot = GTK_WIDGET(user_data);
  int index = rand() % 10;
  if (index > 0) ydata[index] += 1.0;
  SciPlotListUpdateFromFloat(plot, line_id, 10, xdata, ydata);
  if (SciPlotQuickUpdate(plot))
    SciPlotUpdate(plot);
  return G_SOURCE_CONTINUE;
}

static void
on_activate(GtkApplication *application, gpointer user_data)
{
  (void)user_data;

  GtkWidget *root = gtk_application_window_new(application);
  gtk_widget_set_visible(root, FALSE);

  GtkWidget *plot = SciPlotDialog(root, "Real Time Test");

  for (int i = 0; i < 10; i++) {
    xdata[i] = i + 1.0f;
    ydata[i] = i / 2.0f;
  }
  line_id = SciPlotListCreateFromFloat(plot, 10, xdata, ydata, "race");
  SciPlotUpdate(plot);
  SciPlotDialogPopup(plot);

  srand((unsigned int)getpid());
  g_timeout_add(500, update_cb, plot);
}

int
main(int argc, char *argv[])
{
  GtkApplication *app = gtk_application_new("org.sciplot.realtime",
                                            G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
