/*----------------------------------------------------------------------------
 * SciPlot	A generalized plotting widget -- private header
 *
 * Copyright (c) 1996 Robert W. McMullen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * GTK4/Cairo port: Barak A. Pearlmutter <bap@debian.org>
 */

#ifndef _SCIPLOTP_H
#define _SCIPLOTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gtk/gtk.h>
#include <pango/pango.h>
#include "SciPlot.h"

#define powi(a,i) (real)pow((double)(a), (double)((int)(i)))

#define NUMPLOTLINEALLOC  5
#define NUMPLOTDATAEXTRA  25
#define NUMPLOTITEMALLOC  256
#define NUMPLOTITEMEXTRA  64
#define DEG2RAD (3.1415926535897931160E0/180.0)

typedef enum {
    SciPlotFALSE,
    SciPlotPoint,
    SciPlotLine,
    SciPlotRect,
    SciPlotFRect,
    SciPlotCircle,
    SciPlotFCircle,
    SciPlotStartTextTypes,
    SciPlotText,
    SciPlotVText,
    SciPlotEndTextTypes,
    SciPlotPoly,
    SciPlotFPoly,
    SciPlotClipRegion,
    SciPlotClipClear,
    SciPlotENDOFLIST
} SciPlotTypeEnum;

typedef enum {
    SciPlotDrawingAny,
    SciPlotDrawingAxis,
    SciPlotDrawingLegend,
    SciPlotDrawingLine
} SciPlotDrawingEnum;

typedef struct _SciPlotItem {
    SciPlotTypeEnum  type;
    SciPlotDrawingEnum drawing_class;
    union {
        struct { short color; short style; real x, y; } pt;
        struct { short color; short style; real x1, y1, x2, y2; } line;
        struct { short color; short style; real x, y, w, h; } rect;
        struct { short color; short style; real x, y, r; } circ;
        struct { short color; short style; short count; real x[4], y[4]; } poly;
        struct { short color; short style; short font; short length;
                 real x, y; char *text; } text;
        struct { short color; short style; } any;
    } kind;
    short individually_allocated;
    struct _SciPlotItem *next;
} SciPlotItem;

typedef struct {
    int  LineStyle, LineColor, PointStyle, PointColor;
    int  number, allocated;
    realpair *data;
    char *legend;
    real markersize;
    realpair min, max;
    gboolean draw, used;
} SciPlotList;

typedef struct {
    real Origin, Size, Center, TitlePos, AxisPos, LabelPos;
    real LegendPos, LegendSize, DrawOrigin, DrawSize, DrawMax;
    real MajorInc;
    int  MajorNum, MinorNum, Precision;
} SciPlotAxis;

typedef struct {
    int id;
    PangoFontDescription *desc;
} SciPlotFont;

typedef struct {
    int        flag;
    const char *PostScript;
    const char *Pango;
    gboolean   PSUsesOblique;
    gboolean   PSUsesRoman;
} SciPlotFontDesc;

/*
 * Widget class struct (empty beyond parent)
 */
struct _SciPlotWidgetClass {
    GtkWidgetClass parent_class;
};

/*
 * Widget instance struct -- all state lives here
 */
struct _SciPlotWidget {
    GtkWidget parent_instance;

    /* Configurable properties */
    int      Margin;
    int      TitleMargin;
    int      LegendMargin;
    int      LegendLineSize;
    int      MajorTicSize;
    int      DefaultMarkerSize;
    int      ChartType;
    gboolean Degrees;
    gboolean XLog;
    gboolean YLog;
    gboolean XAutoScale;
    gboolean YAutoScale;
    gboolean XAxisNumbers;
    gboolean YAxisNumbers;
    gboolean XOrigin;
    gboolean YOrigin;
    gboolean DrawMajor;
    gboolean DrawMinor;
    gboolean DrawMajorTics;
    gboolean DrawMinorTics;
    gboolean ShowLegend;
    gboolean ShowTitle;
    gboolean ShowXLabel;
    gboolean ShowYLabel;
    gboolean YNumHorz;
    gboolean LegendThroughPlot;
    gboolean Monochrome;
    int      TitleFont;     /* encoded font flags */
    int      LabelFont;
    int      AxisFont;
    int      BackgroundColor;
    int      ForegroundColor;

    /* Axis and dimension state */
    char     *plotTitle;
    char     *xlabel;
    char     *ylabel;
    realpair  Min, Max;
    realpair  UserMin, UserMax;
    real      PolarScale;
    SciPlotAxis x, y;
    int      titleFont;    /* index into fonts[] */
    int      labelFont;
    int      axisFont;

    /* Color table */
    GdkRGBA *colors;
    int      num_colors;

    /* Font table */
    SciPlotFont *fonts;
    int          num_fonts;

    /* Plot data */
    int       alloc_plotlist;
    int       num_plotlist;
    SciPlotList *plotlist;

    /* Draw list (retained display list) */
    int       alloc_drawlist;
    int       num_drawlist;
    SciPlotItem *drawlist;
    SciPlotDrawingEnum current_id;

    gboolean  update;
    gboolean  needs_recompute;
};

#ifdef __cplusplus
}
#endif

#endif /* _SCIPLOTP_H */
