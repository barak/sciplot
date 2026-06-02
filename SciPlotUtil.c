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
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "SciPlot.h"
#include "SciPlotUtil.h"

/* Color / marker tables */

typedef struct { const char *text; int num; } textnumpair;

static textnumpair colors_list[] = {
  {"blue",0},{"red",0},{"ForestGreen",0},{"DarkGoldenrod",0},
  {"orange",0},{"magenta",0},{"grey",0},{"purple",0},
};
#define NUM_COLORS 8

static textnumpair marker_list[] = {
  {"SQUARE",XtMARKER_SQUARE},{"CIRCLE",XtMARKER_CIRCLE},
  {"UTRIANGLE",XtMARKER_UTRIANGLE},{"DTRIANGLE",XtMARKER_DTRIANGLE},
  {"DIAMOND",XtMARKER_DIAMOND},{"HOURGLASS",XtMARKER_HOURGLASS},
  {"BOWTIE",XtMARKER_BOWTIE},{"LTRIANGLE",XtMARKER_LTRIANGLE},
  {"RTRIANGLE",XtMARKER_RTRIANGLE},{"FCIRCLE",XtMARKER_FCIRCLE},
  {"FSQUARE",XtMARKER_FSQUARE},{"FUTRIANGLE",XtMARKER_FUTRIANGLE},
  {"FDTRIANGLE",XtMARKER_FDTRIANGLE},{"FDIAMOND",XtMARKER_FDIAMOND},
  {"FHOURGLASS",XtMARKER_FHOURGLASS},{"FBOWTIE",XtMARKER_FBOWTIE},
  {"FLTRIANGLE",XtMARKER_FLTRIANGLE},{"FRTRIANGLE",XtMARKER_FRTRIANGLE},
};
#define NUM_MARKERS 18

/* String parsing */

#define MAXFIELD 40
#define STRING_MAX 63
#define PARSE_SEPARATORS ",= \t\n"

static char  field[MAXFIELD][STRING_MAX+1];
static float ffield[MAXFIELD];

static void
upper(char *str)
{
  char *dest = str;
  for (int i = strlen(str); i > 0; i--) {
    if (*str != ' ') *dest++ = toupper(*str);
    str++;
  }
  *dest = '\0';
}

static char *
strparse(char *str, char *primary)
{
  static int start, len;
  static char *save;
  int skip, i, plen;
  char c, *begin;
  if (primary) {
    if (str) { save = str; len = strlen(str); start = 0; }
    if (start >= len) return NULL;
    plen = strlen(primary);
    skip = 1;
    while (start < len && skip) {
      c = save[start]; skip = 0;
      for (i = 0; i < plen; i++) if (c == primary[i]) { skip = 1; break; }
      if (skip) start++;
    }
    if (start >= len) return NULL;
    begin = &save[start];
    if (*begin == '"') {
      begin++; start++;
      while (start < len) {
        c = save[start];
        if (c == '"') { save[start] = '\0'; start++; break; }
        else if (c == '\0') break;
        start++;
      }
    } else {
      skip = 0;
      while (start < len && !skip) {
        c = save[start]; skip = 0;
        for (i = 0; i < plen; i++) if (c == primary[i]) { skip = 1; break; }
        if (skip) { save[start] = '\0'; start++; break; } else start++;
      }
    }
    return begin;
  }
  return NULL;
}

static int
sepfield(char *str, int count)
{
  char tmpval[1024];
  strncpy(tmpval, str, sizeof(tmpval)-1); tmpval[sizeof(tmpval)-1] = '\0';
  char *ptr = strchr(tmpval, '#');
  if (ptr) *ptr = '\0';
  ptr = strparse(tmpval, PARSE_SEPARATORS);
  while (ptr) {
    strncpy(field[count], ptr, STRING_MAX-1);
    field[count][STRING_MAX-1] = '\0';
    count++;
    ptr = strparse(NULL, PARSE_SEPARATORS);
  }
  return count;
}

static void
tofloat(int count)
{
  for (int i = 0; i < count; i++) ffield[i] = (float)atof(field[i]);
  for (int i = count; i < MAXFIELD; i++) ffield[i] = 0.0;
}

#define isfloat(c)    (isdigit(c)||(c)=='.'||(c)=='-')
#define isfloatany(c) (isdigit(c)||(c)=='.'||(c)=='e'||(c)=='E'||(c)=='-'||(c)=='+')

static int
checkfloat(int loc)
{
  char *c = field[loc];
  if (*c && isfloat(*c)) { c++; while (*c) { if (!isfloatany(*c)) return 0; c++; } }
  else return 0;
  return 1;
}

static int
reads(int fd, char *buf, int num)
{
  int count = 0;
  while (count < num) {
    if (read(fd, buf, 1) == 1) {
      count++;
      if (*buf == '\n') break;
      buf++;
    } else if (count > 0) break;
    else { count = -1; break; }
  }
  *buf = '\0';
  return count;
}

static int
getfields(FILE *fd)
{
  static char cmdline[1024];
  int count;
  do {
    count = reads(fileno(fd), cmdline, 1000);
    if (count <= 0) return -1;
    count = sepfield(cmdline, 0);
  } while (count == 0);
  return count;
}

/* Per-dialog data */

typedef struct {
  GtkWidget *title_entry;
  GtkWidget *xlabel_entry;
  GtkWidget *ylabel_entry;
  GtkWidget *filename_entry;
  GtkWidget *window;
  GtkWidget *plot;
} PlotDialogData;

static int DialogCount = 0;

/* Callbacks */

static void on_quit_clicked(GtkButton *b, gpointer d) { (void)b;(void)d; exit(0); }

static void
on_dismiss_clicked(GtkButton *b, gpointer ud)
{
  (void)b; PlotDialogData *pd = ud;
  gtk_window_close(GTK_WINDOW(pd->window));
  if (--DialogCount == 0) exit(0);
}

static void
on_make_ps_clicked(GtkButton *b, gpointer ud)
{
  (void)b; PlotDialogData *pd = ud;
  const char *f = gtk_editable_get_text(GTK_EDITABLE(pd->filename_entry));
  if (f && *f) SciPlotPSCreateColor(pd->plot, f);
}

#define TOGGLE_AND_UPDATE(fn_get, fn_set) \
  PlotDialogData *pd = ud; \
  fn_set(pd->plot, !fn_get(pd->plot)); \
  SciPlotUpdate(pd->plot);

static void on_log_x(GtkButton *b, gpointer ud) { (void)b; TOGGLE_AND_UPDATE(sciplot_widget_get_xlog, sciplot_widget_set_xlog) }
static void on_log_y(GtkButton *b, gpointer ud) { (void)b; TOGGLE_AND_UPDATE(sciplot_widget_get_ylog, sciplot_widget_set_ylog) }
static void on_origin_x(GtkButton *b, gpointer ud) { (void)b; TOGGLE_AND_UPDATE(sciplot_widget_get_xorigin, sciplot_widget_set_xorigin) }
static void on_origin_y(GtkButton *b, gpointer ud) { (void)b; TOGGLE_AND_UPDATE(sciplot_widget_get_yorigin, sciplot_widget_set_yorigin) }
static void on_nums_x(GtkButton *b, gpointer ud) { (void)b; TOGGLE_AND_UPDATE(sciplot_widget_get_xaxis_numbers, sciplot_widget_set_xaxis_numbers) }
static void on_nums_y(GtkButton *b, gpointer ud) { (void)b; TOGGLE_AND_UPDATE(sciplot_widget_get_yaxis_numbers, sciplot_widget_set_yaxis_numbers) }
static void on_major(GtkButton *b, gpointer ud) { (void)b; TOGGLE_AND_UPDATE(sciplot_widget_get_draw_major, sciplot_widget_set_draw_major) }
static void on_minor(GtkButton *b, gpointer ud) { (void)b; TOGGLE_AND_UPDATE(sciplot_widget_get_draw_minor, sciplot_widget_set_draw_minor) }
static void on_legend(GtkButton *b, gpointer ud) { (void)b; TOGGLE_AND_UPDATE(sciplot_widget_get_show_legend, sciplot_widget_set_show_legend) }
static void on_show_title(GtkButton *b, gpointer ud) { (void)b; TOGGLE_AND_UPDATE(sciplot_widget_get_show_title, sciplot_widget_set_show_title) }
static void on_show_xlabel(GtkButton *b, gpointer ud) { (void)b; TOGGLE_AND_UPDATE(sciplot_widget_get_show_xlabel, sciplot_widget_set_show_xlabel) }
static void on_show_ylabel(GtkButton *b, gpointer ud) { (void)b; TOGGLE_AND_UPDATE(sciplot_widget_get_show_ylabel, sciplot_widget_set_show_ylabel) }

static void
on_title_activate(GtkEntry *e, gpointer ud) {
  PlotDialogData *pd = ud;
  sciplot_widget_set_plot_title(pd->plot, gtk_editable_get_text(GTK_EDITABLE(e)));
  SciPlotUpdate(pd->plot);
}
static void
on_xlabel_activate(GtkEntry *e, gpointer ud) {
  PlotDialogData *pd = ud;
  sciplot_widget_set_xlabel(pd->plot, gtk_editable_get_text(GTK_EDITABLE(e)));
  SciPlotUpdate(pd->plot);
}
static void
on_ylabel_activate(GtkEntry *e, gpointer ud) {
  PlotDialogData *pd = ud;
  sciplot_widget_set_ylabel(pd->plot, gtk_editable_get_text(GTK_EDITABLE(e)));
  SciPlotUpdate(pd->plot);
}

static PlotDialogData *
SciPlotDialogInternal(GtkWidget *parent_win, const char *title)
{
  PlotDialogData *pd = g_new0(PlotDialogData, 1);
  DialogCount++;

  char *wt = g_strdup_printf("Plot #%d: %s", DialogCount, title);
  GtkWidget *win = gtk_window_new();
  gtk_window_set_title(GTK_WINDOW(win), wt);
  gtk_window_set_default_size(GTK_WINDOW(win), 550, 700);
  gtk_window_set_hide_on_close(GTK_WINDOW(win), FALSE);
  g_free(wt);
  if (parent_win && GTK_IS_WINDOW(parent_win)) {
    gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(parent_win));
    GtkApplication *app = gtk_window_get_application(GTK_WINDOW(parent_win));
    if (app) gtk_window_set_application(GTK_WINDOW(win), app);
  }
  pd->window = win;

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_margin_start(vbox,6); gtk_widget_set_margin_end(vbox,6);
  gtk_widget_set_margin_top(vbox,6);  gtk_widget_set_margin_bottom(vbox,6);
  gtk_window_set_child(GTK_WINDOW(win), vbox);

  /* Label/title fields */
  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
  gtk_box_append(GTK_BOX(vbox), grid);

  gtk_grid_attach(GTK_GRID(grid), gtk_label_new("X Label:"), 0, 0, 1, 1);
  pd->xlabel_entry = gtk_entry_new();
  gtk_widget_set_hexpand(pd->xlabel_entry, TRUE);
  gtk_grid_attach(GTK_GRID(grid), pd->xlabel_entry, 1, 0, 1, 1);

  gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Y Label:"), 0, 1, 1, 1);
  pd->ylabel_entry = gtk_entry_new();
  gtk_widget_set_hexpand(pd->ylabel_entry, TRUE);
  gtk_grid_attach(GTK_GRID(grid), pd->ylabel_entry, 1, 1, 1, 1);

  gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Title:"), 0, 2, 1, 1);
  pd->title_entry = gtk_entry_new();
  gtk_editable_set_text(GTK_EDITABLE(pd->title_entry), title);
  gtk_widget_set_hexpand(pd->title_entry, TRUE);
  gtk_grid_attach(GTK_GRID(grid), pd->title_entry, 1, 2, 1, 1);

  /* Plot widget */
  pd->plot = sciplot_widget_new();
  gtk_widget_set_vexpand(pd->plot, TRUE);
  gtk_widget_set_hexpand(pd->plot, TRUE);
  sciplot_widget_set_plot_title(pd->plot, title);
  gtk_box_append(GTK_BOX(vbox), pd->plot);

  /* Toggle buttons */
  GtkWidget *tb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_set_halign(tb, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(vbox), tb);
#define ADDBT(label, cb) { GtkWidget *b = gtk_button_new_with_label(label); g_signal_connect(b,"clicked",G_CALLBACK(cb),pd); gtk_box_append(GTK_BOX(tb),b); }
  ADDBT("Log X",   on_log_x)
  ADDBT("Log Y",   on_log_y)
  ADDBT("Orig X",  on_origin_x)
  ADDBT("Orig Y",  on_origin_y)
  ADDBT("Nums X",  on_nums_x)
  ADDBT("Nums Y",  on_nums_y)
  ADDBT("Major",   on_major)
  ADDBT("Minor",   on_minor)
  ADDBT("Title",   on_show_title)
  ADDBT("X Label", on_show_xlabel)
  ADDBT("Y Label", on_show_ylabel)
  ADDBT("Legend",  on_legend)
#undef ADDBT

  /* Action buttons */
  GtkWidget *ab = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_set_halign(ab, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(vbox), ab);
  { GtkWidget *b;
    b = gtk_button_new_with_label("Quit");
    g_signal_connect(b,"clicked",G_CALLBACK(on_quit_clicked),pd);
    gtk_box_append(GTK_BOX(ab),b);
    b = gtk_button_new_with_label("Dismiss");
    g_signal_connect(b,"clicked",G_CALLBACK(on_dismiss_clicked),pd);
    gtk_box_append(GTK_BOX(ab),b);
    b = gtk_button_new_with_label("Make PostScript");
    g_signal_connect(b,"clicked",G_CALLBACK(on_make_ps_clicked),pd);
    gtk_box_append(GTK_BOX(ab),b);
    char psf[64]; snprintf(psf,sizeof(psf),"plot%d.ps",DialogCount);
    pd->filename_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(pd->filename_entry), psf);
    gtk_widget_set_size_request(pd->filename_entry, 180, -1);
    gtk_box_append(GTK_BOX(ab), pd->filename_entry);
  }

  g_signal_connect(pd->title_entry,  "activate", G_CALLBACK(on_title_activate),  pd);
  g_signal_connect(pd->xlabel_entry, "activate", G_CALLBACK(on_xlabel_activate), pd);
  g_signal_connect(pd->ylabel_entry, "activate", G_CALLBACK(on_ylabel_activate), pd);

  return pd;
}

static void
SciPlotDialogInternalPopup(PlotDialogData *pd)
{
  const char *t = sciplot_widget_get_plot_title(pd->plot);
  const char *x = sciplot_widget_get_xlabel(pd->plot);
  const char *y = sciplot_widget_get_ylabel(pd->plot);
  if (t) gtk_editable_set_text(GTK_EDITABLE(pd->title_entry),  t);
  if (x) gtk_editable_set_text(GTK_EDITABLE(pd->xlabel_entry), x);
  if (y) gtk_editable_set_text(GTK_EDITABLE(pd->ylabel_entry), y);
  gtk_window_present(GTK_WINDOW(pd->window));
}

GtkWidget *
SciPlotDialog(GtkWidget *parent, const char *title)
{
  PlotDialogData *pd = SciPlotDialogInternal(parent, title);
  return pd->plot;
}

void
SciPlotDialogPopup(GtkWidget *plot)
{
  GtkWidget *win = gtk_widget_get_ancestor(plot, GTK_TYPE_WINDOW);
  if (win) gtk_window_present(GTK_WINDOW(win));
}

void
SciPlotReadDataFile(GtkWidget *parent, FILE *fd)
{
  int count;
  PlotDialogData *working = NULL;
  float xlist[10], ylist[10];
  int line[256], linecount;
  gboolean readnext;
  int num, i;

  count = getfields(fd);
  while (count > 0) {
    readnext = TRUE;
    upper(field[0]);

    if (strcmp(field[0],"TITLE")==0 || strcmp(field[0],"NEW")==0) {
      if (working) SciPlotDialogInternalPopup(working);
      working = SciPlotDialogInternal(parent, field[1]);
      for (i = 0; i < NUM_COLORS; i++)
        colors_list[i].num = SciPlotAllocNamedColor(working->plot, colors_list[i].text);
      linecount = 0;
    }
    else if (strcmp(field[0],"POLAR")==0) {
      gboolean deg = TRUE;
      if (count>1) { upper(field[1]); if (strncmp(field[1],"RAD",3)==0) deg=FALSE; }
      if (working) { sciplot_widget_set_chart_type(working->plot,XtPOLAR); sciplot_widget_set_degrees(working->plot,deg); }
    }
    else if (strcmp(field[0],"XAXIS")==0) {
      if (working) {
        if (count>1) sciplot_widget_set_xlabel(working->plot, field[1]);
        for (i=2; i<count; i++) { upper(field[i]);
          if (strcmp(field[i],"LOG")==0) sciplot_widget_set_xlog(working->plot,TRUE);
          else if (strcmp(field[i],"NOZERO")==0) sciplot_widget_set_xorigin(working->plot,FALSE); }
      }
    }
    else if (strcmp(field[0],"YAXIS")==0) {
      if (working) {
        if (count>1) sciplot_widget_set_ylabel(working->plot, field[1]);
        for (i=2; i<count; i++) { upper(field[i]);
          if (strcmp(field[i],"LOG")==0) sciplot_widget_set_ylog(working->plot,TRUE);
          else if (strcmp(field[i],"NOZERO")==0) sciplot_widget_set_yorigin(working->plot,FALSE); }
      }
    }
    else if (strcmp(field[0],"LEGEND")==0) {
      if (working && count>1) {
        if (count>=4) { sciplot_widget_set_xlabel(working->plot,field[2]); sciplot_widget_set_ylabel(working->plot,field[3]); }
        line[0] = SciPlotListCreateFromFloat(working->plot,0,NULL,NULL,field[1]);
        do {
          count = getfields(fd); readnext = FALSE;
          num = checkfloat(0);
          if (count>0 && num) { tofloat(count); xlist[0]=ffield[0]; ylist[0]=ffield[1]; SciPlotListAddFloat(working->plot,line[0],1,xlist,ylist); }
        } while (count>0 && num);
        SciPlotListSetStyle(working->plot,line[0], colors_list[linecount%NUM_COLORS].num, marker_list[linecount%NUM_MARKERS].num, colors_list[linecount%NUM_COLORS].num, -1);
        linecount++;
      }
    }
    else if (strcmp(field[0],"LINE")==0) {
      if (working && count>1) {
        int maxlines = count;
        gboolean skip;
        for (i=1; i<count; i++) line[i] = SciPlotListCreateFromFloat(working->plot,0,NULL,NULL,field[i]);
        do {
          count = getfields(fd); readnext = FALSE;
          num = checkfloat(0); skip = (strcmp(field[0],"skip")==0);
          if (count>0 && (num||skip)) {
            if (skip) { for (i=1;i<maxlines;i++) { xlist[0]=ylist[0]=SCIPLOT_SKIP_VAL; SciPlotListAddFloat(working->plot,line[i],1,xlist,ylist); } }
            else { tofloat(count); xlist[0]=ffield[0]; for (i=1;i<maxlines;i++) { ylist[0]=ffield[i]; SciPlotListAddFloat(working->plot,line[i],1,xlist,ylist); } }
          }
        } while (count>0 && (num||skip));
        for (i=1; i<maxlines; i++) {
          linecount++;
          SciPlotListSetStyle(working->plot,line[i], colors_list[linecount%NUM_COLORS].num, marker_list[linecount%NUM_MARKERS].num, colors_list[linecount%NUM_COLORS].num, -1);
        }
      }
    }
    if (readnext) count = getfields(fd);
  }
  if (working) SciPlotDialogInternalPopup(working);
}
