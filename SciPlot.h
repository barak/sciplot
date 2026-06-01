/*----------------------------------------------------------------------------
 * SciPlot	A generalized plotting widget
 *
 * Copyright (c) 1996 Robert W. McMullen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Rob McMullen <rwmcm@mail.ae.utexas.edu>
 * GTK4/Cairo port: Barak A. Pearlmutter <bap@debian.org>
 */

#ifndef _SCIPLOT_H
#define _SCIPLOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gtk/gtk.h>
#include <math.h>
#include <float.h>

#define _SCIPLOT_WIDGET_VERSION 1.37

#define SCIPLOT_SKIP_VAL (-FLT_MAX)

typedef float real;

typedef struct { real x, y; } realpair;

/* Chart type constants */
#define XtPOLAR     0
#define XtCARTESIAN 1

/* Marker style constants */
#define XtMARKER_NONE       0
#define XtMARKER_CIRCLE     1
#define XtMARKER_SQUARE     2
#define XtMARKER_UTRIANGLE  3
#define XtMARKER_DTRIANGLE  4
#define XtMARKER_LTRIANGLE  5
#define XtMARKER_RTRIANGLE  6
#define XtMARKER_DIAMOND    7
#define XtMARKER_HOURGLASS  8
#define XtMARKER_BOWTIE     9
#define XtMARKER_FCIRCLE    10
#define XtMARKER_FSQUARE    11
#define XtMARKER_FUTRIANGLE 12
#define XtMARKER_FDTRIANGLE 13
#define XtMARKER_FLTRIANGLE 14
#define XtMARKER_FRTRIANGLE 15
#define XtMARKER_FDIAMOND   16
#define XtMARKER_FHOURGLASS 17
#define XtMARKER_FBOWTIE    18
#define XtMARKER_DOT        19

/* Font encoding flags */
#define XtFONT_SIZE_MASK         0xff
#define XtFONT_SIZE_DEFAULT      12
#define XtFONT_NAME_MASK         0xf00
#define XtFONT_TIMES             0x000
#define XtFONT_COURIER           0x100
#define XtFONT_HELVETICA         0x200
#define XtFONT_LUCIDA            0x300
#define XtFONT_LUCIDASANS        0x400
#define XtFONT_NCSCHOOLBOOK      0x500
#define XtFONT_NAME_DEFAULT      XtFONT_TIMES
#define XtFONT_ATTRIBUTE_MASK    0xf000
#define XtFONT_BOLD              0x1000
#define XtFONT_ITALIC            0x2000
#define XtFONT_BOLD_ITALIC       0x3000
#define XtFONT_ATTRIBUTE_DEFAULT 0

/* Line style constants */
#define XtLINE_NONE    0
#define XtLINE_SOLID   1
#define XtLINE_DOTTED  2
#define XtLINE_WIDEDOT 3
#define XtLINE_USERDASH 4

/* GTK4 GObject type */
#define SCIPLOT_TYPE_WIDGET (sciplot_widget_get_type())

typedef struct _SciPlotWidget      SciPlotWidget;
typedef struct _SciPlotWidgetClass SciPlotWidgetClass;

GType sciplot_widget_get_type(void) G_GNUC_CONST;

#define SCIPLOT_WIDGET(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), SCIPLOT_TYPE_WIDGET, SciPlotWidget))
#define SCIPLOT_WIDGET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), SCIPLOT_TYPE_WIDGET, SciPlotWidgetClass))
#define SCIPLOT_IS_WIDGET(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), SCIPLOT_TYPE_WIDGET))

/* Legacy compatibility */
typedef GtkWidget *Widget;
#define XtIsSciPlot(w) SCIPLOT_IS_WIDGET(w)

/* Constructor */
GtkWidget *sciplot_widget_new(void);

/* Property setters */
void sciplot_widget_set_chart_type(GtkWidget *wi, int type);
void sciplot_widget_set_degrees(GtkWidget *wi, gboolean val);
void sciplot_widget_set_xlog(GtkWidget *wi, gboolean val);
void sciplot_widget_set_ylog(GtkWidget *wi, gboolean val);
void sciplot_widget_set_xautoscale(GtkWidget *wi, gboolean val);
void sciplot_widget_set_yautoscale(GtkWidget *wi, gboolean val);
void sciplot_widget_set_xaxis_numbers(GtkWidget *wi, gboolean val);
void sciplot_widget_set_yaxis_numbers(GtkWidget *wi, gboolean val);
void sciplot_widget_set_xorigin(GtkWidget *wi, gboolean val);
void sciplot_widget_set_yorigin(GtkWidget *wi, gboolean val);
void sciplot_widget_set_draw_major(GtkWidget *wi, gboolean val);
void sciplot_widget_set_draw_minor(GtkWidget *wi, gboolean val);
void sciplot_widget_set_draw_major_tics(GtkWidget *wi, gboolean val);
void sciplot_widget_set_draw_minor_tics(GtkWidget *wi, gboolean val);
void sciplot_widget_set_show_legend(GtkWidget *wi, gboolean val);
void sciplot_widget_set_show_title(GtkWidget *wi, gboolean val);
void sciplot_widget_set_show_xlabel(GtkWidget *wi, gboolean val);
void sciplot_widget_set_show_ylabel(GtkWidget *wi, gboolean val);
void sciplot_widget_set_monochrome(GtkWidget *wi, gboolean val);
void sciplot_widget_set_legend_through_plot(GtkWidget *wi, gboolean val);
void sciplot_widget_set_y_numbers_horizontal(GtkWidget *wi, gboolean val);
void sciplot_widget_set_plot_title(GtkWidget *wi, const char *title);
void sciplot_widget_set_xlabel(GtkWidget *wi, const char *label);
void sciplot_widget_set_ylabel(GtkWidget *wi, const char *label);
void sciplot_widget_set_title_font(GtkWidget *wi, int font_flags);
void sciplot_widget_set_label_font(GtkWidget *wi, int font_flags);
void sciplot_widget_set_axis_font(GtkWidget *wi, int font_flags);
void sciplot_widget_set_margin(GtkWidget *wi, int margin);
void sciplot_widget_set_title_margin(GtkWidget *wi, int margin);

/* Property getters */
gboolean    sciplot_widget_get_xlog(GtkWidget *wi);
gboolean    sciplot_widget_get_ylog(GtkWidget *wi);
gboolean    sciplot_widget_get_xaxis_numbers(GtkWidget *wi);
gboolean    sciplot_widget_get_yaxis_numbers(GtkWidget *wi);
gboolean    sciplot_widget_get_xorigin(GtkWidget *wi);
gboolean    sciplot_widget_get_yorigin(GtkWidget *wi);
gboolean    sciplot_widget_get_draw_major(GtkWidget *wi);
gboolean    sciplot_widget_get_draw_minor(GtkWidget *wi);
gboolean    sciplot_widget_get_show_legend(GtkWidget *wi);
gboolean    sciplot_widget_get_show_title(GtkWidget *wi);
gboolean    sciplot_widget_get_show_xlabel(GtkWidget *wi);
gboolean    sciplot_widget_get_show_ylabel(GtkWidget *wi);
const char *sciplot_widget_get_plot_title(GtkWidget *wi);
const char *sciplot_widget_get_xlabel(GtkWidget *wi);
const char *sciplot_widget_get_ylabel(GtkWidget *wi);

/*
 * Public SciPlot functions
 */
gboolean SciPlotPSCreate(GtkWidget *wi, const char *filename);
gboolean SciPlotPSCreateColor(GtkWidget *wi, const char *filename);
int  SciPlotAllocNamedColor(GtkWidget *wi, const char *name);
int  SciPlotAllocRGBColor(GtkWidget *wi, int r, int g, int b);
void SciPlotSetBackgroundColor(GtkWidget *wi, int color);
void SciPlotSetForegroundColor(GtkWidget *wi, int color);

void SciPlotListDelete(GtkWidget *wi, int idnum);
int  SciPlotListCreateFromData(GtkWidget *wi, int num, real *xlist, real *ylist,
                               const char *legend,
                               int pcolor, int pstyle, int lcolor, int lstyle);
int  SciPlotListCreateFloat(GtkWidget *wi, int num, float *xlist, float *ylist,
                            const char *legend);
void SciPlotListUpdateFloat(GtkWidget *wi, int idnum, int num,
                            float *xlist, float *ylist);
void SciPlotListAddFloat(GtkWidget *wi, int idnum, int num,
                         float *xlist, float *ylist);
int  SciPlotListCreateDouble(GtkWidget *wi, int num, double *xlist, double *ylist,
                             const char *legend);
void SciPlotListUpdateDouble(GtkWidget *wi, int idnum, int num,
                             double *xlist, double *ylist);
void SciPlotListAddDouble(GtkWidget *wi, int idnum, int num,
                          double *xlist, double *ylist);
void SciPlotListSetStyle(GtkWidget *wi, int idnum,
                         int pcolor, int pstyle, int lcolor, int lstyle);
void SciPlotListSetMarkerSize(GtkWidget *wi, int idnum, float size);

void SciPlotSetXAutoScale(GtkWidget *wi);
void SciPlotSetXUserScale(GtkWidget *wi, double min, double max);
void SciPlotSetYAutoScale(GtkWidget *wi);
void SciPlotSetYUserScale(GtkWidget *wi, double min, double max);

void     SciPlotPrintStatistics(GtkWidget *wi);
void     SciPlotExportData(GtkWidget *wi, FILE *fd);
void     SciPlotUpdate(GtkWidget *wi);
gboolean SciPlotQuickUpdate(GtkWidget *wi);

/* Compat aliases */
#define SciPlotListCreateFromFloat  SciPlotListCreateFloat
#define SciPlotListUpdateFromFloat  SciPlotListUpdateFloat
#define SciPlotListCreateFromDouble SciPlotListCreateDouble
#define SciPlotListUpdateFromDouble SciPlotListUpdateDouble

#ifdef __cplusplus
}
#endif

#endif /* _SCIPLOT_H */
