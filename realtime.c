/*
 * sciplot-realtime: stream columnar data from stdin and plot in real time.
 *
 * Copyright (C) 1995 by Rob McMullen
 * GTK4 port and rewrite: Barak A. Pearlmutter <bap@debian.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * Input format (whitespace or comma separated, one sample per line):
 *   One column:    y          — x auto-increments from 0
 *   Two columns:   x y
 *   N columns:     x y1 y2 … y(N-1)
 * Lines beginning with '#' are comments and are ignored.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "SciPlot.h"
#include "SciPlotUtil.h"

/* ---- command-line options ---- */

static gchar *opt_title  = NULL;
static gchar *opt_xlabel = NULL;
static gchar *opt_ylabel = NULL;
static gchar *opt_legend = NULL;

static const GOptionEntry options[] = {
  {"version", 'V', 0, G_OPTION_ARG_NONE,   NULL,
   "Show version information and exit",       NULL},
  {"title",   't', 0, G_OPTION_ARG_STRING, &opt_title,
   "Plot title (default: \"Real-time Data\")", "TEXT"},
  {"xlabel",  'x', 0, G_OPTION_ARG_STRING, &opt_xlabel,
   "X axis label",                            "TEXT"},
  {"ylabel",  'y', 0, G_OPTION_ARG_STRING, &opt_ylabel,
   "Y axis label",                            "TEXT"},
  {"legend",  'l', 0, G_OPTION_ARG_STRING, &opt_legend,
   "Comma-separated series names",            "NAME,..."},
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

/* ---- series colours ---- */

static const char *series_color_names[] = {
  "blue", "red", "ForestGreen", "DarkGoldenrod",
  "orange", "magenta", "SlateGrey", "purple",
};
#define N_SERIES_COLORS ((int)G_N_ELEMENTS(series_color_names))

/* ---- streaming state ---- */

typedef struct {
  GtkWidget  *plot;
  int        *list_ids;     /* list ID per y series                    */
  int        *color_ids;    /* SciPlot color index per y series        */
  int         n_series;     /* number of y series; 0 until first line  */
  gboolean    x_given;      /* TRUE if first column is x               */
  float       x_auto;       /* next x value when x_given is FALSE      */
  gchar     **legend_names; /* split opt_legend, or NULL               */
  gboolean    need_full_update; /* TRUE until first SciPlotUpdate done */
  GIOChannel *chan;
} StreamState;

/* ---- series initialisation (called on first data line) ---- */

static void
init_series(StreamState *s, int n_series, gboolean x_given)
{
  s->n_series  = n_series;
  s->x_given   = x_given;
  s->list_ids  = g_new(int, n_series);
  s->color_ids = g_new(int, n_series);

  for (int i = 0; i < n_series; i++) {
    s->color_ids[i] = SciPlotAllocNamedColor(s->plot,
                        series_color_names[i % N_SERIES_COLORS]);
  }

  for (int i = 0; i < n_series; i++) {
    const char *name;
    char auto_name[32];
    if (s->legend_names && s->legend_names[i] && s->legend_names[i][0]) {
      name = s->legend_names[i];
    } else if (n_series == 1) {
      name = opt_ylabel ? opt_ylabel : "y";
    } else {
      snprintf(auto_name, sizeof(auto_name), "y%d", i + 1);
      name = auto_name;
    }
    s->list_ids[i] = SciPlotListCreateFromFloat(s->plot, 0, NULL, NULL, name);
    SciPlotListSetStyle(s->plot, s->list_ids[i],
                        s->color_ids[i], XtMARKER_NONE,
                        s->color_ids[i], XtLINE_SOLID);
  }
}

/* ---- parse and ingest one text line ---- */

static void
process_line(StreamState *s, const gchar *line)
{
  /* skip leading whitespace then check for comment / blank */
  const gchar *p = line;
  while (*p == ' ' || *p == '\t') p++;
  if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0')
    return;

  /* parse whitespace/comma-separated floats */
  float vals[64];
  int n = 0;
  gchar *end;
  while (*p && n < 64) {
    float v = (float)g_ascii_strtod(p, &end);
    if (end == p) break;
    vals[n++] = v;
    p = end;
    while (*p == ' ' || *p == '\t' || *p == ',') p++;
  }
  if (n == 0)
    return;

  /* determine structure from first data line */
  if (s->n_series == 0) {
    gboolean x_given = (n >= 2);
    init_series(s, x_given ? n - 1 : 1, x_given);
  }

  /* extract x and y values */
  float x;
  float *ys = s->x_given ? vals + 1 : vals;
  int    n_ys = s->x_given ? n - 1 : n;

  if (s->x_given)
    x = vals[0];
  else
    x = s->x_auto++;

  /* append one point to each series */
  for (int i = 0; i < s->n_series && i < n_ys; i++)
    SciPlotListAddFloat(s->plot, s->list_ids[i], 1, &x, &ys[i]);
}

/* ---- GIOChannel callback: stdin readable or closed ---- */

static gboolean
stdin_cb(GIOChannel *chan, GIOCondition cond, gpointer user_data)
{
  StreamState *s = user_data;

  if (cond & (G_IO_HUP | G_IO_ERR)) {
    /* drain any remaining data first, then stop */
    gchar *line; gsize len;
    while (g_io_channel_read_line(chan, &line, &len, NULL, NULL)
           == G_IO_STATUS_NORMAL) {
      process_line(s, line);
      g_free(line);
    }
    if (s->n_series > 0) SciPlotUpdate(s->plot);
    return FALSE; /* remove watch; window stays open */
  }

  /* read all currently available lines */
  gchar *line; gsize len;
  GIOStatus status;
  while ((status = g_io_channel_read_line(chan, &line, &len, NULL, NULL))
         == G_IO_STATUS_NORMAL) {
    process_line(s, line);
    g_free(line);
  }

  if (status == G_IO_STATUS_EOF) {
    if (s->n_series > 0) SciPlotUpdate(s->plot);
    return FALSE;
  }

  /* update the plot after this burst of input */
  if (s->n_series > 0) {
    if (s->need_full_update) {
      SciPlotUpdate(s->plot);
      s->need_full_update = FALSE;
    } else if (SciPlotQuickUpdate(s->plot)) {
      SciPlotUpdate(s->plot);
    }
  }

  return TRUE; /* keep watching */
}

/* ---- GtkApplication activate ---- */

static void
on_activate(GtkApplication *app, gpointer user_data)
{
  StreamState *s = user_data;

  GtkWidget *root = gtk_application_window_new(app);
  gtk_widget_set_visible(root, FALSE);

  const char *title = opt_title ? opt_title : "Real-time Data";
  s->plot = SciPlotDialog(root, title);

  if (opt_xlabel) sciplot_widget_set_xlabel(s->plot, opt_xlabel);
  if (opt_ylabel) sciplot_widget_set_ylabel(s->plot, opt_ylabel);

  SciPlotDialogPopup(s->plot);

  if (opt_legend)
    s->legend_names = g_strsplit(opt_legend, ",", -1);

  s->need_full_update = TRUE;

  /* watch stdin for incoming data */
  s->chan = g_io_channel_unix_new(STDIN_FILENO);
  g_io_channel_set_flags(s->chan, G_IO_FLAG_NONBLOCK, NULL);
  g_io_add_watch(s->chan, G_IO_IN | G_IO_HUP | G_IO_ERR, stdin_cb, s);
}

/* ---- main ---- */

int
main(int argc, char *argv[])
{
  StreamState state = {0};

  GtkApplication *app = gtk_application_new("org.sciplot.realtime",
                                            G_APPLICATION_DEFAULT_FLAGS);
  g_application_set_option_context_summary(G_APPLICATION(app),
    "Read whitespace-separated numeric data from standard input and plot\n"
    "each sample as it arrives.  One column: y (x auto-increments);\n"
    "two or more columns: x y1 [y2 ...].  Lines starting with '#' are ignored.");
  g_application_add_main_option_entries(G_APPLICATION(app), options);
  g_signal_connect(app, "handle-local-options",
                   G_CALLBACK(on_handle_local_options), NULL);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), &state);

  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  g_free(state.list_ids);
  g_free(state.color_ids);
  g_strfreev(state.legend_names);
  if (state.chan)
    g_io_channel_unref(state.chan);

  return status;
}
