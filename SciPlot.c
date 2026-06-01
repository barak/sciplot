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
 * Author: Rob McMullen <rwmcm@mail.ae.utexas.edu>
 * GTK4/Cairo port: Barak A. Pearlmutter <bap@debian.org>
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "SciPlotP.h"
#include <cairo/cairo-ps.h>

/* --------------------------------------------------------------------------
 * Forward declarations
 */
static void  ComputeAll(SciPlotWidget *w);
static void  ComputeAllDimensions(SciPlotWidget *w);
static void  DrawAll(SciPlotWidget *w);
static void  ItemDrawAll(SciPlotWidget *w, cairo_t *cr);
static void  ItemDraw(SciPlotWidget *w, SciPlotItem *item, cairo_t *cr);
static void  EraseAll(SciPlotWidget *w);
static void  EraseAllItems(SciPlotWidget *w);
static int   ColorStore(SciPlotWidget *w, const GdkRGBA *color);
static int   FontStore(SciPlotWidget *w, int flag);
static int   FontnumReplace(SciPlotWidget *w, int fontnum, int flag);
static void  FontnumStore(SciPlotWidget *w, int fontnum, int flag);
static real  FontnumHeight(SciPlotWidget *w, int fontnum);
static real  FontnumAscent(SciPlotWidget *w, int fontnum);
static real  FontnumDescent(SciPlotWidget *w, int fontnum);
static real  FontnumTextWidth(SciPlotWidget *w, int fontnum, const char *text);

/* --------------------------------------------------------------------------
 * Font description table
 */
static SciPlotFontDesc font_desc_table[] =
{
  {XtFONT_TIMES,        "Times",            "Times New Roman",      FALSE, TRUE},
  {XtFONT_COURIER,      "Courier",          "Courier New",          TRUE,  FALSE},
  {XtFONT_HELVETICA,    "Helvetica",        "Helvetica",            TRUE,  FALSE},
  {XtFONT_LUCIDA,       "Lucida",           "Lucida Bright",        FALSE, FALSE},
  {XtFONT_LUCIDASANS,   "LucidaSans",       "Lucida Sans",          FALSE, FALSE},
  {XtFONT_NCSCHOOLBOOK, "NewCenturySchlbk", "Century Schoolbook L", FALSE, TRUE},
  {-1, NULL, NULL, FALSE, FALSE},
};

/* ==========================================================================
 * GObject / GTK4 widget boilerplate
 */

G_DEFINE_TYPE(SciPlotWidget, sciplot_widget, GTK_TYPE_WIDGET)

static void
sciplot_finalize(GObject *object)
{
  SciPlotWidget *w = SCIPLOT_WIDGET(object);
  int i;
  SciPlotFont *pf;
  SciPlotList *p;

  g_free(w->xlabel);
  g_free(w->ylabel);
  g_free(w->plotTitle);

  for (i = 0; i < w->num_fonts; i++) {
    pf = &w->fonts[i];
    if (pf->desc)
      pango_font_description_free(pf->desc);
  }
  g_free(w->fonts);
  g_free(w->colors);

  for (i = 0; i < w->alloc_plotlist; i++) {
    p = w->plotlist + i;
    if (p->allocated > 0)
      g_free(p->data);
    g_free(p->legend);
  }
  if (w->alloc_plotlist > 0)
    g_free(w->plotlist);

  EraseAllItems(w);
  g_free(w->drawlist);

  G_OBJECT_CLASS(sciplot_widget_parent_class)->finalize(object);
}

static void
sciplot_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
  SciPlotWidget *w = SCIPLOT_WIDGET(widget);
  int width  = gtk_widget_get_width(widget);
  int height = gtk_widget_get_height(widget);

  if (width <= 0 || height <= 0)
    return;

  graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, (float)width, (float)height);
  cairo_t *cr = gtk_snapshot_append_cairo(snapshot, &bounds);

  /* Paint background */
  if (w->num_colors > w->BackgroundColor) {
    GdkRGBA *bg = &w->colors[w->BackgroundColor];
    cairo_set_source_rgba(cr, bg->red, bg->green, bg->blue, bg->alpha);
  } else {
    cairo_set_source_rgb(cr, 1, 1, 1);
  }
  cairo_paint(cr);

  ItemDrawAll(w, cr);

  cairo_destroy(cr);
}

static void
sciplot_size_allocate(GtkWidget *widget, int width, int height, int baseline)
{
  GTK_WIDGET_CLASS(sciplot_widget_parent_class)->size_allocate(
    widget, width, height, baseline);

  SciPlotWidget *w = SCIPLOT_WIDGET(widget);
  if (gtk_widget_get_realized(widget)) {
    EraseAll(w);
    ComputeAll(w);
    DrawAll(w);
  }
}

static void
sciplot_measure(GtkWidget *widget, GtkOrientation orientation, int for_size,
                int *minimum, int *natural,
                int *minimum_baseline, int *natural_baseline)
{
  (void)widget; (void)for_size;
  *minimum = (orientation == GTK_ORIENTATION_HORIZONTAL) ? 200 : 150;
  *natural = (orientation == GTK_ORIENTATION_HORIZONTAL) ? 500 : 400;
  *minimum_baseline = *natural_baseline = -1;
}

static void
sciplot_widget_class_init(SciPlotWidgetClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass *widget_class  = GTK_WIDGET_CLASS(klass);

  gobject_class->finalize      = sciplot_finalize;
  widget_class->snapshot       = sciplot_snapshot;
  widget_class->size_allocate  = sciplot_size_allocate;
  widget_class->measure        = sciplot_measure;

  gtk_widget_class_set_css_name(widget_class, "sciplot");
}

static void
sciplot_widget_init(SciPlotWidget *w)
{
  /* Default property values */
  w->ChartType         = XtCARTESIAN;
  w->Degrees           = TRUE;
  w->DrawMajor         = TRUE;
  w->DrawMajorTics     = TRUE;
  w->DrawMinor         = TRUE;
  w->DrawMinorTics     = TRUE;
  w->Monochrome        = FALSE;
  w->ShowLegend        = TRUE;
  w->ShowTitle         = TRUE;
  w->ShowXLabel        = TRUE;
  w->ShowYLabel        = TRUE;
  w->XAutoScale        = TRUE;
  w->YAutoScale        = TRUE;
  w->XAxisNumbers      = TRUE;
  w->YAxisNumbers      = TRUE;
  w->XLog              = FALSE;
  w->YLog              = FALSE;
  w->XOrigin           = FALSE;
  w->YOrigin           = FALSE;
  w->YNumHorz          = TRUE;
  w->LegendThroughPlot = FALSE;
  w->Margin            = 5;
  w->TitleMargin       = 16;
  w->LegendLineSize    = 16;
  w->DefaultMarkerSize = 3;
  w->LegendMargin      = 3;
  w->MajorTicSize      = 5;
  w->TitleFont         = XtFONT_HELVETICA | 24;
  w->LabelFont         = XtFONT_TIMES     | 18;
  w->AxisFont          = XtFONT_TIMES     | 10;

  w->plotTitle = g_strdup("Plot");
  w->xlabel    = g_strdup("X Axis");
  w->ylabel    = g_strdup("Y Axis");

  w->colors     = NULL;
  w->num_colors = 0;
  w->fonts      = NULL;
  w->num_fonts  = 0;

  w->plotlist       = NULL;
  w->alloc_plotlist = 0;
  w->num_plotlist   = 0;

  w->alloc_drawlist = NUMPLOTITEMALLOC;
  w->drawlist       = g_new0(SciPlotItem, w->alloc_drawlist);
  w->num_drawlist   = 0;

  w->update          = FALSE;
  w->needs_recompute = FALSE;
  w->UserMin.x = w->UserMin.y = 0.0;
  w->UserMax.x = w->UserMax.y = 10.0;

  /* Default background (white=0) and foreground (black=1) */
  {
    GdkRGBA white = {1.0, 1.0, 1.0, 1.0};
    GdkRGBA black = {0.0, 0.0, 0.0, 1.0};
    w->BackgroundColor = ColorStore(w, &white);
    w->ForegroundColor = ColorStore(w, &black);
  }

  w->titleFont = FontStore(w, w->TitleFont);
  w->labelFont = FontStore(w, w->LabelFont);
  w->axisFont  = FontStore(w, w->AxisFont);
}

GtkWidget *
sciplot_widget_new(void)
{
  return GTK_WIDGET(g_object_new(SCIPLOT_TYPE_WIDGET, NULL));
}

/* ==========================================================================
 * Color management
 */

static int
ColorStore(SciPlotWidget *w, const GdkRGBA *color)
{
  w->num_colors++;
  w->colors = g_realloc(w->colors, sizeof(GdkRGBA) * w->num_colors);
  w->colors[w->num_colors - 1] = *color;
  return w->num_colors - 1;
}

/* ==========================================================================
 * Font management (Pango-based)
 */

static SciPlotFontDesc *
FontDescLookup(int flag)
{
  SciPlotFontDesc *pfd = font_desc_table;
  while (pfd->flag >= 0) {
    if ((flag & XtFONT_NAME_MASK) == pfd->flag)
      return pfd;
    pfd++;
  }
  return NULL;
}

static void
FontnumStore(SciPlotWidget *w, int fontnum, int flag)
{
  SciPlotFont *pf = &w->fonts[fontnum];
  int fontflag = flag & XtFONT_NAME_MASK;
  int sizeflag = flag & XtFONT_SIZE_MASK;
  int attrflag = flag & XtFONT_ATTRIBUTE_MASK;

  switch (fontflag) {
  case XtFONT_TIMES:
  case XtFONT_COURIER:
  case XtFONT_HELVETICA:
  case XtFONT_LUCIDA:
  case XtFONT_LUCIDASANS:
  case XtFONT_NCSCHOOLBOOK:
    break;
  default:
    fontflag = XtFONT_NAME_DEFAULT;
    break;
  }
  if (sizeflag < 1)
    sizeflag = XtFONT_SIZE_DEFAULT;
  switch (attrflag) {
  case XtFONT_BOLD:
  case XtFONT_ITALIC:
  case XtFONT_BOLD_ITALIC:
    break;
  default:
    attrflag = XtFONT_ATTRIBUTE_DEFAULT;
    break;
  }

  SciPlotFontDesc *pfd = FontDescLookup(fontflag);
  const char *family = pfd ? pfd->Pango : "serif";

  PangoFontDescription *desc = pango_font_description_new();
  pango_font_description_set_family(desc, family);
  pango_font_description_set_size(desc, sizeflag * PANGO_SCALE);
  if (attrflag & XtFONT_BOLD)
    pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
  if (attrflag & XtFONT_ITALIC)
    pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);

  if (pf->desc)
    pango_font_description_free(pf->desc);
  pf->id   = flag;
  pf->desc = desc;
}

static int
FontStore(SciPlotWidget *w, int flag)
{
  w->num_fonts++;
  w->fonts = g_realloc(w->fonts, sizeof(SciPlotFont) * w->num_fonts);
  int fontnum = w->num_fonts - 1;
  w->fonts[fontnum].id   = 0;
  w->fonts[fontnum].desc = NULL;
  FontnumStore(w, fontnum, flag);
  return fontnum;
}

static int
FontnumReplace(SciPlotWidget *w, int fontnum, int flag)
{
  FontnumStore(w, fontnum, flag);
  return fontnum;
}

static PangoFontMetrics *
get_font_metrics(SciPlotWidget *w, int fontnum)
{
  if (fontnum < 0 || fontnum >= w->num_fonts)
    fontnum = 0;
  PangoContext *ctx = gtk_widget_get_pango_context(GTK_WIDGET(w));
  return pango_context_get_metrics(ctx, w->fonts[fontnum].desc, NULL);
}

static real
FontnumHeight(SciPlotWidget *w, int fontnum)
{
  PangoFontMetrics *m = get_font_metrics(w, fontnum);
  real h = (real)PANGO_PIXELS(pango_font_metrics_get_ascent(m) +
                               pango_font_metrics_get_descent(m));
  pango_font_metrics_unref(m);
  return h;
}

static real
FontnumAscent(SciPlotWidget *w, int fontnum)
{
  PangoFontMetrics *m = get_font_metrics(w, fontnum);
  real a = (real)PANGO_PIXELS(pango_font_metrics_get_ascent(m));
  pango_font_metrics_unref(m);
  return a;
}

static real
FontnumDescent(SciPlotWidget *w, int fontnum)
{
  PangoFontMetrics *m = get_font_metrics(w, fontnum);
  real d = (real)PANGO_PIXELS(pango_font_metrics_get_descent(m));
  pango_font_metrics_unref(m);
  return d;
}

static real
FontnumTextWidth(SciPlotWidget *w, int fontnum, const char *text)
{
  if (!text || !*text)
    return 0.0;
  if (fontnum < 0 || fontnum >= w->num_fonts)
    fontnum = 0;
  PangoContext *ctx = gtk_widget_get_pango_context(GTK_WIDGET(w));
  PangoLayout *layout = pango_layout_new(ctx);
  pango_layout_set_font_description(layout, w->fonts[fontnum].desc);
  pango_layout_set_text(layout, text, -1);
  int width, height;
  pango_layout_get_pixel_size(layout, &width, &height);
  g_object_unref(layout);
  return (real)width;
}

/* ==========================================================================
 * Drawing utilities -- Cairo-based
 */

static void
SetItemColor(SciPlotWidget *w, SciPlotItem *item, cairo_t *cr)
{
  int color;
  short c = item->kind.any.color;
  if (w->Monochrome)
    color = (c > 0) ? w->ForegroundColor : w->BackgroundColor;
  else if (c >= w->num_colors)
    color = w->ForegroundColor;
  else if (c <= 0)
    color = w->BackgroundColor;
  else
    color = c;
  GdkRGBA *rgba = &w->colors[color];
  cairo_set_source_rgba(cr, rgba->red, rgba->green, rgba->blue, rgba->alpha);
}

static void
SetItemLineStyle(SciPlotItem *item, cairo_t *cr)
{
  switch (item->kind.any.style) {
  case XtLINE_SOLID:
    cairo_set_dash(cr, NULL, 0, 0);
    break;
  case XtLINE_DOTTED: {
    double d[] = {1.0, 2.0};
    cairo_set_dash(cr, d, 2, 0);
    break;
  }
  case XtLINE_WIDEDOT: {
    double d[] = {1.0, 8.0};
    cairo_set_dash(cr, d, 2, 0);
    break;
  }
  default:
    cairo_set_dash(cr, NULL, 0, 0);
    break;
  }
}

static void
DrawTextAt(SciPlotWidget *w, cairo_t *cr, int fontnum,
           const char *text, int length, double x, double y)
{
  if (fontnum < 0 || fontnum >= w->num_fonts)
    fontnum = 0;
  PangoLayout *layout = pango_cairo_create_layout(cr);
  pango_layout_set_font_description(layout, w->fonts[fontnum].desc);
  pango_layout_set_text(layout, text, length);
  /* y is the X11-style baseline; convert to pango top-left */
  int baseline = PANGO_PIXELS(pango_layout_get_baseline(layout));
  cairo_move_to(cr, x, y - baseline);
  pango_cairo_show_layout(cr, layout);
  g_object_unref(layout);
}

static void
DrawVTextAt(SciPlotWidget *w, cairo_t *cr, int fontnum,
            const char *text, int length, double x, double y)
{
  /* Vertical text rotated 90 deg CCW.
   * (x,y): x = screen-X of text baseline, y = screen-Y of text bottom.
   * After translate(x,y) + rotate(-pi/2):
   *   user +X maps screen-UP, user +Y maps screen-RIGHT.
   * Place text at user (0, -ascent) so baseline lands at screen_x=x
   * and text occupies screen_y in [y-textwidth, y].
   */
  if (fontnum < 0 || fontnum >= w->num_fonts)
    fontnum = 0;
  PangoLayout *layout = pango_cairo_create_layout(cr);
  pango_layout_set_font_description(layout, w->fonts[fontnum].desc);
  pango_layout_set_text(layout, text, length);
  int baseline = PANGO_PIXELS(pango_layout_get_baseline(layout));
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, -G_PI / 2.0);
  cairo_move_to(cr, 0.0, (double)-baseline);
  pango_cairo_show_layout(cr, layout);
  cairo_restore(cr);
  g_object_unref(layout);
}

static void
ItemDraw(SciPlotWidget *w, SciPlotItem *item, cairo_t *cr)
{
  if (!cr)
    return;

  switch (item->type) {
  case SciPlotLine:
    SetItemColor(w, item, cr);
    SetItemLineStyle(item, cr);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, item->kind.line.x1, item->kind.line.y1);
    cairo_line_to(cr, item->kind.line.x2, item->kind.line.y2);
    cairo_stroke(cr);
    break;

  case SciPlotRect:
    SetItemColor(w, item, cr);
    SetItemLineStyle(item, cr);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, item->kind.rect.x, item->kind.rect.y,
                    item->kind.rect.w, item->kind.rect.h);
    cairo_stroke(cr);
    break;

  case SciPlotFRect:
    SetItemColor(w, item, cr);
    cairo_set_dash(cr, NULL, 0, 0);
    cairo_rectangle(cr, item->kind.rect.x, item->kind.rect.y,
                    item->kind.rect.w, item->kind.rect.h);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    break;

  case SciPlotPoly: {
    int i = item->kind.poly.count;
    if (i < 2) break;
    SetItemColor(w, item, cr);
    SetItemLineStyle(item, cr);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, item->kind.poly.x[0], item->kind.poly.y[0]);
    for (int j = 1; j < i; j++)
      cairo_line_to(cr, item->kind.poly.x[j], item->kind.poly.y[j]);
    cairo_close_path(cr);
    cairo_stroke(cr);
    break;
  }

  case SciPlotFPoly: {
    int i = item->kind.poly.count;
    if (i < 2) break;
    SetItemColor(w, item, cr);
    cairo_set_dash(cr, NULL, 0, 0);
    cairo_move_to(cr, item->kind.poly.x[0], item->kind.poly.y[0]);
    for (int j = 1; j < i; j++)
      cairo_line_to(cr, item->kind.poly.x[j], item->kind.poly.y[j]);
    cairo_close_path(cr);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    break;
  }

  case SciPlotCircle:
    SetItemColor(w, item, cr);
    SetItemLineStyle(item, cr);
    cairo_set_line_width(cr, 1.0);
    cairo_arc(cr, item->kind.circ.x, item->kind.circ.y,
              item->kind.circ.r, 0.0, 2.0 * G_PI);
    cairo_stroke(cr);
    break;

  case SciPlotFCircle:
    SetItemColor(w, item, cr);
    cairo_set_dash(cr, NULL, 0, 0);
    cairo_arc(cr, item->kind.circ.x, item->kind.circ.y,
              item->kind.circ.r, 0.0, 2.0 * G_PI);
    cairo_fill_preserve(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    break;

  case SciPlotText:
    SetItemColor(w, item, cr);
    DrawTextAt(w, cr, item->kind.text.font,
               item->kind.text.text, item->kind.text.length,
               item->kind.text.x, item->kind.text.y);
    break;

  case SciPlotVText:
    SetItemColor(w, item, cr);
    DrawVTextAt(w, cr, item->kind.text.font,
                item->kind.text.text, item->kind.text.length,
                item->kind.text.x, item->kind.text.y);
    break;

  case SciPlotClipRegion:
    /* line.x1,y1 = origin; line.x2,y2 = size */
    cairo_save(cr);
    cairo_rectangle(cr, item->kind.line.x1, item->kind.line.y1,
                    item->kind.line.x2, item->kind.line.y2);
    cairo_clip(cr);
    break;

  case SciPlotClipClear:
    cairo_restore(cr);
    break;

  default:
    break;
  }
}

static void
ItemDrawAll(SciPlotWidget *w, cairo_t *cr)
{
  SciPlotItem *item = w->drawlist;
  for (int i = 0; i < w->num_drawlist; i++, item++)
    ItemDraw(w, item, cr);
}

/* ==========================================================================
 * PostScript output via Cairo PS surface
 */

static gboolean
SciPlotPSInternal(GtkWidget *wi, const char *filename)
{
  SciPlotWidget *w;
  cairo_surface_t *surface;
  cairo_t *cr;
  int width, height;

  if (!SCIPLOT_IS_WIDGET(wi)) {
    g_warning("SciPlotPSCreate: not a SciPlot widget.");
    return FALSE;
  }
  w = SCIPLOT_WIDGET(wi);
  if (!gtk_widget_get_realized(wi)) {
    g_warning("SciPlotPSCreate: widget not realized.");
    return FALSE;
  }
  width  = gtk_widget_get_width(wi);
  height = gtk_widget_get_height(wi);
  if (width <= 0 || height <= 0)
    return FALSE;

  if (w->num_drawlist == 0) {
    ComputeAll(w);
    DrawAll(w);
  }

  surface = cairo_ps_surface_create(filename, (double)width, (double)height);
  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
    g_warning("SciPlotPSCreate: cannot create PS file %s", filename);
    cairo_surface_destroy(surface);
    return FALSE;
  }
  cr = cairo_create(surface);
  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_paint(cr);
  ItemDrawAll(w, cr);
  cairo_show_page(cr);
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  return TRUE;
}

gboolean
SciPlotPSCreate(GtkWidget *wi, const char *filename)
{
  return SciPlotPSInternal(wi, filename);
}

gboolean
SciPlotPSCreateColor(GtkWidget *wi, const char *filename)
{
  return SciPlotPSInternal(wi, filename);
}

/* ==========================================================================
 * Erase utilities
 */

static void
EraseAllItems(SciPlotWidget *w)
{
  SciPlotItem *item = w->drawlist;
  for (int i = 0; i < w->num_drawlist; i++, item++) {
    if (item->type > SciPlotStartTextTypes &&
        item->type < SciPlotEndTextTypes)
      g_free(item->kind.text.text);
  }
  w->num_drawlist = 0;
}

static void
EraseAll(SciPlotWidget *w)
{
  EraseAllItems(w);
}

/* ==========================================================================
 * Display-list item allocation
 */

static SciPlotItem *
ItemGetNew(SciPlotWidget *w)
{
  w->num_drawlist++;
  if (w->num_drawlist >= w->alloc_drawlist) {
    w->alloc_drawlist += NUMPLOTITEMEXTRA;
    w->drawlist = g_realloc(w->drawlist,
                            w->alloc_drawlist * sizeof(SciPlotItem));
    if (!w->drawlist) {
      fprintf(stderr, "Can't realloc memory for SciPlotItem list\n");
      exit(1);
    }
  }
  SciPlotItem *item = w->drawlist + (w->num_drawlist - 1);
  item->type          = SciPlotFALSE;
  item->drawing_class = w->current_id;
  return item;
}

/* ==========================================================================
 * Primitive setters (add to display list only, no immediate draw)
 */

static void
LineSet(SciPlotWidget *w, real x1, real y1, real x2, real y2,
        int color, int style)
{
  SciPlotItem *item = ItemGetNew(w);
  item->kind.any.color = (short)color;
  item->kind.any.style = (short)style;
  item->kind.line.x1   = x1; item->kind.line.y1 = y1;
  item->kind.line.x2   = x2; item->kind.line.y2 = y2;
  item->type           = SciPlotLine;
}

static void
RectSet(SciPlotWidget *w, real x1, real y1, real x2, real y2,
        int color, int style)
{
  real x, y, width, height;
  if (x1 < x2) x = x1, width  = x2 - x1 + 1;
  else          x = x2, width  = x1 - x2 + 1;
  if (y1 < y2) y = y1, height = y2 - y1 + 1;
  else          y = y2, height = y1 - y2 + 1;
  SciPlotItem *item = ItemGetNew(w);
  item->kind.any.color = (short)color; item->kind.any.style = (short)style;
  item->kind.rect.x = x; item->kind.rect.y = y;
  item->kind.rect.w = width; item->kind.rect.h = height;
  item->type = SciPlotRect;
}

static void
FilledRectSet(SciPlotWidget *w, real x1, real y1, real x2, real y2,
              int color, int style)
{
  real x, y, width, height;
  if (x1 < x2) x = x1, width  = x2 - x1 + 1;
  else          x = x2, width  = x1 - x2 + 1;
  if (y1 < y2) y = y1, height = y2 - y1 + 1;
  else          y = y2, height = y1 - y2 + 1;
  SciPlotItem *item = ItemGetNew(w);
  item->kind.any.color = (short)color; item->kind.any.style = (short)style;
  item->kind.rect.x = x; item->kind.rect.y = y;
  item->kind.rect.w = width; item->kind.rect.h = height;
  item->type = SciPlotFRect;
}

static void
TriSet(SciPlotWidget *w, real x1, real y1, real x2, real y2, real x3, real y3,
       int color, int style)
{
  SciPlotItem *item = ItemGetNew(w);
  item->kind.any.color   = (short)color; item->kind.any.style = (short)style;
  item->kind.poly.count  = 3;
  item->kind.poly.x[0]   = x1; item->kind.poly.y[0] = y1;
  item->kind.poly.x[1]   = x2; item->kind.poly.y[1] = y2;
  item->kind.poly.x[2]   = x3; item->kind.poly.y[2] = y3;
  item->type             = SciPlotPoly;
}

static void
FilledTriSet(SciPlotWidget *w, real x1, real y1, real x2, real y2, real x3, real y3,
             int color, int style)
{
  SciPlotItem *item = ItemGetNew(w);
  item->kind.any.color   = (short)color; item->kind.any.style = (short)style;
  item->kind.poly.count  = 3;
  item->kind.poly.x[0]   = x1; item->kind.poly.y[0] = y1;
  item->kind.poly.x[1]   = x2; item->kind.poly.y[1] = y2;
  item->kind.poly.x[2]   = x3; item->kind.poly.y[2] = y3;
  item->type             = SciPlotFPoly;
}

static void
QuadSet(SciPlotWidget *w,
        real x1, real y1, real x2, real y2,
        real x3, real y3, real x4, real y4,
        int color, int style)
{
  SciPlotItem *item = ItemGetNew(w);
  item->kind.any.color   = (short)color; item->kind.any.style = (short)style;
  item->kind.poly.count  = 4;
  item->kind.poly.x[0]   = x1; item->kind.poly.y[0] = y1;
  item->kind.poly.x[1]   = x2; item->kind.poly.y[1] = y2;
  item->kind.poly.x[2]   = x3; item->kind.poly.y[2] = y3;
  item->kind.poly.x[3]   = x4; item->kind.poly.y[3] = y4;
  item->type             = SciPlotPoly;
}

static void
FilledQuadSet(SciPlotWidget *w,
              real x1, real y1, real x2, real y2,
              real x3, real y3, real x4, real y4,
              int color, int style)
{
  SciPlotItem *item = ItemGetNew(w);
  item->kind.any.color   = (short)color; item->kind.any.style = (short)style;
  item->kind.poly.count  = 4;
  item->kind.poly.x[0]   = x1; item->kind.poly.y[0] = y1;
  item->kind.poly.x[1]   = x2; item->kind.poly.y[1] = y2;
  item->kind.poly.x[2]   = x3; item->kind.poly.y[2] = y3;
  item->kind.poly.x[3]   = x4; item->kind.poly.y[3] = y4;
  item->type             = SciPlotFPoly;
}

static void
CircleSet(SciPlotWidget *w, real x, real y, real r, int color, int style)
{
  SciPlotItem *item = ItemGetNew(w);
  item->kind.any.color = (short)color; item->kind.any.style = (short)style;
  item->kind.circ.x = x; item->kind.circ.y = y; item->kind.circ.r = r;
  item->type = SciPlotCircle;
}

static void
FilledCircleSet(SciPlotWidget *w, real x, real y, real r, int color, int style)
{
  SciPlotItem *item = ItemGetNew(w);
  item->kind.any.color = (short)color; item->kind.any.style = (short)style;
  item->kind.circ.x = x; item->kind.circ.y = y; item->kind.circ.r = r;
  item->type = SciPlotFCircle;
}

static void
TextSet(SciPlotWidget *w, real x, real y, const char *text, int color, int font)
{
  SciPlotItem *item = ItemGetNew(w);
  item->kind.any.color   = (short)color;
  item->kind.any.style   = 0;
  item->kind.text.x      = x;
  item->kind.text.y      = y;
  item->kind.text.length = (short)strlen(text);
  item->kind.text.text   = g_strdup(text);
  item->kind.text.font   = (short)font;
  item->type             = SciPlotText;
}

static void
TextCenter(SciPlotWidget *w, real x, real y, const char *text, int color, int font)
{
  x -= FontnumTextWidth(w, font, text) / 2.0;
  y += FontnumHeight(w, font) / 2.0 - FontnumDescent(w, font);
  TextSet(w, x, y, text, color, font);
}

static void
VTextSet(SciPlotWidget *w, real x, real y, const char *text, int color, int font)
{
  SciPlotItem *item = ItemGetNew(w);
  item->kind.any.color   = (short)color;
  item->kind.any.style   = 0;
  item->kind.text.x      = x;
  item->kind.text.y      = y;
  item->kind.text.length = (short)strlen(text);
  item->kind.text.text   = g_strdup(text);
  item->kind.text.font   = (short)font;
  item->type             = SciPlotVText;
}

static void
VTextCenter(SciPlotWidget *w, real x, real y, const char *text, int color, int font)
{
  x += FontnumHeight(w, font) / 2.0 - FontnumDescent(w, font);
  y += FontnumTextWidth(w, font, text) / 2.0;
  VTextSet(w, x, y, text, color, font);
}

static void
ClipSet(SciPlotWidget *w)
{
  if (w->ChartType == XtCARTESIAN) {
    SciPlotItem *item = ItemGetNew(w);
    item->kind.any.style = XtLINE_SOLID;
    item->kind.any.color = 1;
    item->kind.line.x1   = w->x.Origin;
    item->kind.line.x2   = w->x.Size;
    item->kind.line.y1   = w->y.Origin;
    item->kind.line.y2   = w->y.Size;
    item->type           = SciPlotClipRegion;
  }
}

static void
ClipClear(SciPlotWidget *w)
{
  if (w->ChartType == XtCARTESIAN) {
    SciPlotItem *item = ItemGetNew(w);
    item->kind.any.style = XtLINE_SOLID;
    item->kind.any.color = 1;
    item->type           = SciPlotClipClear;
  }
}

/* ==========================================================================
 * Data coordinate to screen coordinate converters
 */

static real
PlotX(SciPlotWidget *w, real xin)
{
  if (w->XLog)
    return w->x.Origin +
      ((log10(xin) - log10(w->x.DrawOrigin)) *
       (w->x.Size / w->x.DrawSize));
  else
    return w->x.Origin +
      ((xin - w->x.DrawOrigin) * (w->x.Size / w->x.DrawSize));
}

static real
PlotY(SciPlotWidget *w, real yin)
{
  if (w->YLog)
    return w->y.Origin + w->y.Size -
      ((log10(yin) - log10(w->y.DrawOrigin)) *
       (w->y.Size / w->y.DrawSize));
  else
    return w->y.Origin + w->y.Size -
      ((yin - w->y.DrawOrigin) * (w->y.Size / w->y.DrawSize));
}

static void
PlotRTRadians(SciPlotWidget *w, real r, real t, real *xout, real *yout)
{
  *xout = w->x.Center + (r * (real)cos(t) / w->PolarScale * w->x.Size / 2.0);
  *yout = w->y.Center + (-r * (real)sin(t) / w->PolarScale * w->x.Size / 2.0);
}

static void
PlotRTDegrees(SciPlotWidget *w, real r, real t, real *xout, real *yout)
{
  PlotRTRadians(w, r, t * DEG2RAD, xout, yout);
}

static void
PlotRT(SciPlotWidget *w, real r, real t, real *xout, real *yout)
{
  if (w->Degrees) t *= DEG2RAD;
  PlotRTRadians(w, r, t, xout, yout);
}

/* ==========================================================================
 * Axis calculation utilities
 */

#define NUMBER_MINOR 8
#define MAX_MAJOR    8
static float CAdeltas[8]   = {0.1, 0.2, 0.25, 0.5, 1.0, 2.0, 2.5, 5.0};
static int   CAdecimals[8] = {0, 0, 1, 0, 0, 0, 1, 0};
static int   CAminors[8]   = {4, 4, 4, 5, 4, 4, 4, 5};

static void
ComputeAxis(SciPlotAxis *axis, real min, real max, gboolean log)
{
  real range, rnorm, delta, calcmin, calcmax;
  int nexp, majornum, minornum, majordecimals, decimals, i;

  range = max - min;
  if (log) {
    if (range == 0.0) {
      calcmin = powi(10.0, (int)floor(log10(min)));
      calcmax = 10.0 * calcmin;
    } else {
      calcmin = powi(10.0, (int)floor(log10(min)));
      calcmax = powi(10.0, (int)ceil(log10(max)));
    }
    delta = 10.0;
    axis->DrawOrigin = calcmin;
    axis->DrawMax    = calcmax;
    axis->DrawSize   = log10(calcmax) - log10(calcmin);
    axis->MajorInc   = delta;
    axis->MajorNum   = (int)(log10(calcmax) - log10(calcmin)) + 1;
    axis->MinorNum   = 10;
    axis->Precision  = -(int)(log10(calcmin) * 1.0001);
    if (axis->Precision < 0) axis->Precision = 0;
  } else {
    if (range == 0.0) nexp = 0;
    else              nexp = (int)floor(log10(range));
    rnorm = range / powi(10.0, nexp);
    for (i = 0; i < NUMBER_MINOR; i++) {
      delta         = CAdeltas[i];
      minornum      = CAminors[i];
      majornum      = (int)((rnorm + 0.9999 * delta) / delta);
      majordecimals = CAdecimals[i];
      if (majornum <= MAX_MAJOR) break;
    }
    delta *= powi(10.0, nexp);

    if (min < 0.0)
      calcmin = ((float)((int)((min - .9999 * delta) / delta))) * delta;
    else if ((min > 0.0) && (min < 1.0))
      calcmin = ((float)((int)((1.0001 * min) / delta))) * delta;
    else if (min >= 1.0)
      calcmin = ((float)((int)((.9999 * min) / delta))) * delta;
    else
      calcmin = min;
    if (max < 0.0)
      calcmax = ((float)((int)((.9999 * max) / delta))) * delta;
    else if (max > 0.0)
      calcmax = ((float)((int)((max + .9999 * delta) / delta))) * delta;
    else
      calcmax = max;

    axis->DrawOrigin = calcmin;
    axis->DrawMax    = calcmax;
    axis->DrawSize   = calcmax - calcmin;
    axis->MajorInc   = delta;
    axis->MajorNum   = majornum;
    axis->MinorNum   = minornum;

    delta = log10(axis->MajorInc);
    if (delta > 0.0) decimals = -(int)floor(delta) + majordecimals;
    else             decimals = (int)ceil(-delta)   + majordecimals;
    if (decimals < 0) decimals = 0;
    axis->Precision = decimals;
  }
}

static void
ComputeDrawingRange(SciPlotWidget *w)
{
  if (w->ChartType == XtCARTESIAN) {
    ComputeAxis(&w->x, w->Min.x, w->Max.x, w->XLog);
    ComputeAxis(&w->y, w->Min.y, w->Max.y, w->YLog);
  } else {
    ComputeAxis(&w->x, (real)0.0, w->Max.x, FALSE);
    w->PolarScale = w->x.DrawMax;
  }
}

static gboolean
CheckMinMax(SciPlotWidget *w)
{
  int i, j;
  SciPlotList *p;
  real val;

  if (w->ChartType == XtCARTESIAN) {
    for (i = 0; i < w->num_plotlist; i++) {
      p = w->plotlist + i;
      if (p->draw) {
        for (j = 0; j < p->number; j++) {
          if (p->data[j].x > SCIPLOT_SKIP_VAL && p->data[j].y > SCIPLOT_SKIP_VAL) {
            val = p->data[j].x;
            if (val > w->x.DrawMax || val < w->x.DrawOrigin) return TRUE;
            val = p->data[j].y;
            if (val > w->y.DrawMax || val < w->y.DrawOrigin) return TRUE;
          }
        }
      }
    }
  } else {
    for (i = 0; i < w->num_plotlist; i++) {
      p = w->plotlist + i;
      if (p->draw) {
        for (j = 0; j < p->number; j++) {
          val = p->data[j].x;
          if (val > w->Max.x || val < w->Min.x) return TRUE;
        }
      }
    }
  }
  return FALSE;
}

static void
ComputeMinMax(SciPlotWidget *w)
{
  int i, j;
  SciPlotList *p;
  real val;
  gboolean firstx = TRUE, firsty = TRUE;

  w->Min.x = w->Min.y = w->Max.x = w->Max.y = 1.0;

  for (i = 0; i < w->num_plotlist; i++) {
    p = w->plotlist + i;
    if (p->draw) {
      for (j = 0; j < p->number; j++) {
        if (p->data[j].x > SCIPLOT_SKIP_VAL && p->data[j].y > SCIPLOT_SKIP_VAL) {
          val = p->data[j].x;
          if (!w->XLog || val > 0.0) {
            if (firstx) { w->Min.x = w->Max.x = val; firstx = FALSE; }
            else { if (val > w->Max.x) w->Max.x = val; else if (val < w->Min.x) w->Min.x = val; }
          }
          val = p->data[j].y;
          if (!w->YLog || val > 0.0) {
            if (firsty) { w->Min.y = w->Max.y = val; firsty = FALSE; }
            else { if (val > w->Max.y) w->Max.y = val; else if (val < w->Min.y) w->Min.y = val; }
          }
        }
      }
    }
  }

  if (firstx) { w->Min.x = w->XLog ? 1.0 : 0.0; w->Max.x = 10.0; }
  if (firsty) { w->Min.y = w->YLog ? 1.0 : 0.0; w->Max.y = 10.0; }

  if (w->ChartType == XtCARTESIAN) {
    if (!w->XLog) {
      if (!w->XAutoScale) { w->Min.x = w->UserMin.x; w->Max.x = w->UserMax.x; }
      else if (w->XOrigin) {
        if (w->Min.x > 0.0) w->Min.x = 0.0;
        if (w->Max.x < 0.0) w->Max.x = 0.0;
      }
      if (fabs(w->Min.x - w->Max.x) < 1.e-10) { w->Min.x -= .5; w->Max.x += .5; }
    }
    if (!w->YLog) {
      if (!w->YAutoScale) { w->Min.y = w->UserMin.y; w->Max.y = w->UserMax.y; }
      else if (w->YOrigin) {
        if (w->Min.y > 0.0) w->Min.y = 0.0;
        if (w->Max.y < 0.0) w->Max.y = 0.0;
      }
      if (fabs(w->Min.y - w->Max.y) < 1.e-10) { w->Min.y -= .5; w->Max.y += .5; }
    }
  } else {
    if (fabs(w->Min.x) > fabs(w->Max.x)) w->Max.x = fabs(w->Min.x);
  }
}

static void
ComputeLegendDimensions(SciPlotWidget *w)
{
  real current, xmax, ymax;
  int i;
  SciPlotList *p;

  if (w->ShowLegend) {
    xmax = 0.0; ymax = 2.0 * (real)w->LegendMargin;
    for (i = 0; i < w->num_plotlist; i++) {
      p = w->plotlist + i;
      if (p->draw) {
        current = (real)w->Margin + (real)w->LegendMargin * 3.0 +
                  (real)w->LegendLineSize +
                  FontnumTextWidth(w, w->axisFont, p->legend);
        if (current > xmax) xmax = current;
        ymax += FontnumHeight(w, w->axisFont);
      }
    }
    w->x.LegendSize = xmax;  w->x.LegendPos = (real)w->Margin;
    w->y.LegendSize = ymax;  w->y.LegendPos = 0.0;
  } else {
    w->x.LegendSize = w->x.LegendPos = w->y.LegendSize = w->y.LegendPos = 0.0;
  }
}

static void
ComputeDimensions(SciPlotWidget *w)
{
  real x, y, width, height;

  x = (real)w->Margin;  y = (real)w->Margin;
  width  = (real)gtk_widget_get_width(GTK_WIDGET(w))  - (real)w->Margin - x - w->x.LegendSize;
  height = (real)gtk_widget_get_height(GTK_WIDGET(w)) - (real)w->Margin - y;

  w->x.Origin = x;  w->y.Origin = y;

  if (w->ShowTitle)
    height -= (real)w->TitleMargin + FontnumHeight(w, w->titleFont);

  if (w->ChartType == XtCARTESIAN) {
    real axisnumbersize = (real)w->Margin + FontnumHeight(w, w->axisFont);
    if (w->XAxisNumbers) height -= axisnumbersize;
    if (w->YAxisNumbers) { width -= axisnumbersize; w->x.Origin += axisnumbersize; }
    if (w->ShowXLabel) height -= (real)w->Margin + FontnumHeight(w, w->labelFont);
    if (w->ShowYLabel) {
      real s = (real)w->Margin + FontnumHeight(w, w->labelFont);
      width -= s;  w->x.Origin += s;
    }
  }

  w->x.Size = width;  w->y.Size = height;
  if (w->ChartType == XtPOLAR && height < width) w->x.Size = height;
  w->x.Center = w->x.Origin + (width / 2.0);
  w->y.Center = w->y.Origin + (height / 2.0);
}

static void
AdjustDimensionsCartesian(SciPlotWidget *w)
{
  real xextra, yextra, val, xhorz;
  real x, y, width, height, axisnumbersize, axisXlabelsize, axisYlabelsize;
  char numberformat[16], label[16];
  int precision;

  xextra = yextra = 0.0;
  if (w->XAxisNumbers) {
    precision = w->x.Precision;
    if (w->XLog) {
      val = w->x.DrawMax;
      precision -= w->x.MajorNum;
      if (precision < 0) precision = 0;
    } else
      val = w->x.DrawOrigin + floor(w->x.DrawSize / w->x.MajorInc) * w->x.MajorInc;
    x = PlotX(w, val);
    snprintf(numberformat, sizeof(numberformat), "%%.%df", precision);
    snprintf(label, sizeof(label), numberformat, val);
    x += FontnumTextWidth(w, w->axisFont, label);
    if ((int)x > gtk_widget_get_width(GTK_WIDGET(w))) {
      xextra = ceil(x - gtk_widget_get_width(GTK_WIDGET(w)) + w->Margin);
      if (xextra < 0.0) xextra = 0.0;
    }
  }

  yextra = xhorz = 0.0;
  if (w->YAxisNumbers) {
    precision = w->y.Precision;
    if (w->YLog) {
      int p1, p2;
      p1 = precision;  if (p1 > 0) p1--;
      p2 = precision - w->y.MajorNum;  if (p2 < 0) p2 = 0;
      precision = (p1 > p2) ? p1 : p2;
      val = w->y.DrawMax;
    } else
      val = w->y.DrawOrigin + floor(w->y.DrawSize / w->y.MajorInc * 1.0001) * w->y.MajorInc;
    y = PlotY(w, val);
    snprintf(numberformat, sizeof(numberformat), "%%.%df", precision);
    snprintf(label, sizeof(label), numberformat, val);
    if (w->YNumHorz) {
      yextra = FontnumHeight(w, w->axisFont) / 2.0;
      xhorz  = FontnumTextWidth(w, w->axisFont, label) + (real)w->Margin;
    } else {
      y -= FontnumTextWidth(w, w->axisFont, label);
      if ((int)y <= 0) {
        yextra = ceil(w->Margin - y);
        if (yextra < 0.0) yextra = 0.0;
      }
    }
  }

  x = (real)w->Margin + xhorz;  y = (real)w->Margin + yextra;
  width  = (real)gtk_widget_get_width(GTK_WIDGET(w))  - (real)w->Margin - x - xextra;
  height = (real)gtk_widget_get_height(GTK_WIDGET(w)) - (real)w->Margin - y;
  w->x.Origin = x;  w->y.Origin = y;

  if (w->ShowTitle)
    height -= (real)w->TitleMargin + FontnumHeight(w, w->titleFont);

  axisXlabelsize = 0.0; axisYlabelsize = 0.0;
  axisnumbersize = (real)w->Margin + FontnumHeight(w, w->axisFont);
  if (w->XAxisNumbers) height -= axisnumbersize;
  if (w->YAxisNumbers && !w->YNumHorz) { width -= axisnumbersize; w->x.Origin += axisnumbersize; }
  if (w->ShowXLabel) { axisXlabelsize = (real)w->Margin + FontnumHeight(w, w->labelFont); height -= axisXlabelsize; }
  if (w->ShowYLabel) {
    axisYlabelsize = (real)w->Margin + FontnumHeight(w, w->labelFont);
    width -= axisYlabelsize;  w->x.Origin += axisYlabelsize;
  }

  if (w->LegendThroughPlot) {
    w->x.LegendPos += w->x.Origin + width - w->x.LegendSize;
    w->y.LegendPos += w->y.Origin;
  } else {
    width -= w->x.LegendSize;
    w->x.LegendPos += w->x.Origin + width;
    w->y.LegendPos += w->y.Origin;
  }

  w->x.Size = width;  w->y.Size = height;

  w->y.AxisPos = w->y.Origin + w->y.Size + (real)w->Margin + FontnumAscent(w, w->axisFont);
  if (w->YNumHorz)
    w->x.AxisPos = w->x.Origin - (real)w->Margin;
  else
    w->x.AxisPos = w->x.Origin - (real)w->Margin - FontnumDescent(w, w->axisFont);

  w->y.LabelPos = w->y.Origin + w->y.Size + (real)w->Margin +
                  (FontnumHeight(w, w->labelFont) / 2.0);
  if (w->XAxisNumbers) w->y.LabelPos += axisnumbersize;
  if (w->YAxisNumbers) {
    if (w->YNumHorz)
      w->x.LabelPos = w->x.Origin - xhorz - (real)w->Margin -
                      (FontnumHeight(w, w->labelFont) / 2.0);
    else
      w->x.LabelPos = w->x.Origin - axisnumbersize - (real)w->Margin -
                      (FontnumHeight(w, w->labelFont) / 2.0);
  } else {
    w->x.LabelPos = w->x.Origin - (real)w->Margin -
                    (FontnumHeight(w, w->labelFont) / 2.0);
  }

  w->y.TitlePos = (real)gtk_widget_get_height(GTK_WIDGET(w)) - (real)w->Margin;
  w->x.TitlePos = (real)w->Margin;
}

static void
AdjustDimensionsPolar(SciPlotWidget *w)
{
  real x, y, xextra, yextra, val, width, height, size;
  char numberformat[16], label[16];

  xextra = yextra = 0.0;
  val = w->PolarScale;
  PlotRTDegrees(w, val, 0.0, &x, &y);
  snprintf(numberformat, sizeof(numberformat), "%%.%df", w->x.Precision);
  snprintf(label, sizeof(label), numberformat, val);
  x += FontnumTextWidth(w, w->axisFont, label);
  if ((int)x > gtk_widget_get_width(GTK_WIDGET(w))) {
    xextra = x - gtk_widget_get_width(GTK_WIDGET(w)) + w->Margin;
    if (xextra < 0.0) xextra = 0.0;
  }

  w->x.Origin = (real)w->Margin;  w->y.Origin = (real)w->Margin;
  width  = (real)gtk_widget_get_width(GTK_WIDGET(w))  - (real)w->Margin - w->x.Origin - xextra;
  height = (real)gtk_widget_get_height(GTK_WIDGET(w)) - (real)w->Margin - w->y.Origin - yextra;
  if (w->ShowTitle) height -= (real)w->TitleMargin + FontnumHeight(w, w->titleFont);

  size = (height < width) ? height : width;
  w->x.Center = w->x.Origin + (width / 2.0);
  w->y.Center = w->y.Origin + (height / 2.0);
  w->x.LegendPos += width - w->x.LegendSize;
  w->y.LegendPos += w->y.Origin;

  if (!w->LegendThroughPlot) {
    real radius = size / 2.0;
    x = w->x.LegendPos - w->x.Center;
    y = (w->y.LegendPos + w->y.LegendSize) - w->y.Center;
    if (sqrt(x*x + y*y) < radius) {
      width  -= w->x.LegendSize;  height -= w->y.LegendSize;
      w->x.Center = w->x.Origin + width / 2.0;
      w->y.Center = w->y.Origin + w->y.LegendSize + height / 2.0;
      size = (height < width) ? height : width;
    }
  }

  w->x.Size = w->y.Size = size;
  w->y.TitlePos = w->y.Center + w->y.Size / 2.0 +
                  (real)w->TitleMargin + FontnumAscent(w, w->titleFont);
  w->x.TitlePos = w->x.Origin;
}

static void
AdjustDimensions(SciPlotWidget *w)
{
  if (w->ChartType == XtCARTESIAN) AdjustDimensionsCartesian(w);
  else                              AdjustDimensionsPolar(w);
}

static void
ComputeAllDimensions(SciPlotWidget *w)
{
  ComputeLegendDimensions(w);
  ComputeDimensions(w);
  ComputeDrawingRange(w);
  AdjustDimensions(w);
}

static void
ComputeAll(SciPlotWidget *w)
{
  ComputeMinMax(w);
  ComputeAllDimensions(w);
}

/* ==========================================================================
 * Drawing routines (fill display list)
 */

static void
DrawMarker(SciPlotWidget *w, real xp, real yp, real size, int color, int style)
{
  real sizex, sizey;
  switch (style) {
  case XtMARKER_CIRCLE:
    CircleSet(w, xp, yp, size, color, XtLINE_SOLID); break;
  case XtMARKER_FCIRCLE:
    FilledCircleSet(w, xp, yp, size, color, XtLINE_SOLID); break;
  case XtMARKER_SQUARE:
    size -= .5;
    RectSet(w, xp-size, yp-size, xp+size, yp+size, color, XtLINE_SOLID); break;
  case XtMARKER_FSQUARE:
    size -= .5;
    FilledRectSet(w, xp-size, yp-size, xp+size, yp+size, color, XtLINE_SOLID); break;
  case XtMARKER_UTRIANGLE:
    sizex = size*.866; sizey = size/2.0;
    TriSet(w, xp, yp-size, xp+sizex, yp+sizey, xp-sizex, yp+sizey, color, XtLINE_SOLID); break;
  case XtMARKER_FUTRIANGLE:
    sizex = size*.866; sizey = size/2.0;
    FilledTriSet(w, xp, yp-size, xp+sizex, yp+sizey, xp-sizex, yp+sizey, color, XtLINE_SOLID); break;
  case XtMARKER_DTRIANGLE:
    sizex = size*.866; sizey = size/2.0;
    TriSet(w, xp, yp+size, xp+sizex, yp-sizey, xp-sizex, yp-sizey, color, XtLINE_SOLID); break;
  case XtMARKER_FDTRIANGLE:
    sizex = size*.866; sizey = size/2.0;
    FilledTriSet(w, xp, yp+size, xp+sizex, yp-sizey, xp-sizex, yp-sizey, color, XtLINE_SOLID); break;
  case XtMARKER_RTRIANGLE:
    sizey = size*.866; sizex = size/2.0;
    TriSet(w, xp+size, yp, xp-sizex, yp+sizey, xp-sizex, yp-sizey, color, XtLINE_SOLID); break;
  case XtMARKER_FRTRIANGLE:
    sizey = size*.866; sizex = size/2.0;
    FilledTriSet(w, xp+size, yp, xp-sizex, yp+sizey, xp-sizex, yp-sizey, color, XtLINE_SOLID); break;
  case XtMARKER_LTRIANGLE:
    sizey = size*.866; sizex = size/2.0;
    TriSet(w, xp-size, yp, xp+sizex, yp+sizey, xp+sizex, yp-sizey, color, XtLINE_SOLID); break;
  case XtMARKER_FLTRIANGLE:
    sizey = size*.866; sizex = size/2.0;
    FilledTriSet(w, xp-size, yp, xp+sizex, yp+sizey, xp+sizex, yp-sizey, color, XtLINE_SOLID); break;
  case XtMARKER_DIAMOND:
    QuadSet(w, xp, yp-size, xp+size, yp, xp, yp+size, xp-size, yp, color, XtLINE_SOLID); break;
  case XtMARKER_FDIAMOND:
    FilledQuadSet(w, xp, yp-size, xp+size, yp, xp, yp+size, xp-size, yp, color, XtLINE_SOLID); break;
  case XtMARKER_HOURGLASS:
    QuadSet(w, xp-size, yp-size, xp+size, yp-size, xp-size, yp+size, xp+size, yp+size, color, XtLINE_SOLID); break;
  case XtMARKER_FHOURGLASS:
    FilledQuadSet(w, xp-size, yp-size, xp+size, yp-size, xp-size, yp+size, xp+size, yp+size, color, XtLINE_SOLID); break;
  case XtMARKER_BOWTIE:
    QuadSet(w, xp-size, yp-size, xp-size, yp+size, xp+size, yp-size, xp+size, yp+size, color, XtLINE_SOLID); break;
  case XtMARKER_FBOWTIE:
    FilledQuadSet(w, xp-size, yp-size, xp-size, yp+size, xp+size, yp-size, xp+size, yp+size, color, XtLINE_SOLID); break;
  case XtMARKER_DOT:
    FilledCircleSet(w, xp, yp, 1.5, color, XtLINE_SOLID); break;
  default: break;
  }
}

static void
DrawLegend(SciPlotWidget *w)
{
  real x, y, len, height, height2, len2, ascent;
  int i;
  SciPlotList *p;

  w->current_id = SciPlotDrawingLegend;
  if (!w->ShowLegend) return;

  x = w->x.LegendPos;  y = w->y.LegendPos;
  len = (real)w->LegendLineSize;  len2 = len / 2.0;
  height = FontnumHeight(w, w->axisFont);  height2 = height / 2.0;
  ascent = FontnumAscent(w, w->axisFont);
  RectSet(w, x, y,
          x + w->x.LegendSize - 1.0 - (real)w->Margin,
          y + w->y.LegendSize - 1.0,
          w->ForegroundColor, XtLINE_SOLID);
  x += (real)w->LegendMargin;  y += (real)w->LegendMargin;

  for (i = 0; i < w->num_plotlist; i++) {
    p = w->plotlist + i;
    if (p->draw) {
      LineSet(w, x, y+height2, x+len, y+height2, p->LineColor, p->LineStyle);
      DrawMarker(w, x+len2, y+height2, p->markersize, p->PointColor, p->PointStyle);
      TextSet(w, x+len+(real)w->LegendMargin, y+ascent,
              p->legend, w->ForegroundColor, w->axisFont);
      y += height;
    }
  }
}

static void
DrawCartesianAxes(SciPlotWidget *w)
{
  real x, y, x1, y1, x2, y2, tic, val, majorval;
  int j, precision;
  char numberformat[16], label[16];

  w->current_id = SciPlotDrawingAxis;
  x1 = PlotX(w, w->x.DrawOrigin);  y1 = PlotY(w, w->y.DrawOrigin);
  x2 = PlotX(w, w->x.DrawMax);     y2 = PlotY(w, w->y.DrawMax);
  LineSet(w, x1, y1, x2, y1, w->ForegroundColor, XtLINE_SOLID);
  LineSet(w, x1, y1, x1, y2, w->ForegroundColor, XtLINE_SOLID);

  precision = w->x.Precision;
  snprintf(numberformat, sizeof(numberformat), "%%.%df", precision);
  val = w->x.DrawOrigin;
  if (w->XLog && precision > 0) precision--;
  x = PlotX(w, val);
  if (w->DrawMajorTics) LineSet(w, x, y1+5, x, y1-5, w->ForegroundColor, XtLINE_SOLID);
  if (w->XAxisNumbers) {
    snprintf(label, sizeof(label), numberformat, val);
    TextSet(w, x, w->y.AxisPos, label, w->ForegroundColor, w->axisFont);
  }
  majorval = val;
  while ((majorval * 1.0001) < w->x.DrawMax) {
    if (w->XLog) {
      if (majorval * 1.1 > w->x.DrawMax) break;
      tic = majorval;
      if (w->DrawMinor || w->DrawMinorTics) {
        for (j = 2; j < w->x.MinorNum; j++) {
          val = tic * (real)j;  x = PlotX(w, val);
          if (w->DrawMinor) LineSet(w, x, y1, x, y2, w->ForegroundColor, XtLINE_WIDEDOT);
          if (w->DrawMinorTics) LineSet(w, x, y1, x, y1-3, w->ForegroundColor, XtLINE_SOLID);
        }
      }
      val = tic * (real)w->x.MinorNum;
      snprintf(numberformat, sizeof(numberformat), "%%.%df", precision);
      if (precision > 0) precision--;
    } else {
      tic = majorval;
      if (w->DrawMinor || w->DrawMinorTics) {
        for (j = 1; j < w->x.MinorNum; j++) {
          val = tic + w->x.MajorInc * (real)j / w->x.MinorNum;  x = PlotX(w, val);
          if (w->DrawMinor) LineSet(w, x, y1, x, y2, w->ForegroundColor, XtLINE_WIDEDOT);
          if (w->DrawMinorTics) LineSet(w, x, y1, x, y1-3, w->ForegroundColor, XtLINE_SOLID);
        }
      }
      val = tic + w->x.MajorInc;
    }
    x = PlotX(w, val);
    if (w->DrawMajor) LineSet(w, x, y1, x, y2, w->ForegroundColor, XtLINE_DOTTED);
    else if (w->DrawMinor) LineSet(w, x, y1, x, y2, w->ForegroundColor, XtLINE_WIDEDOT);
    if (w->DrawMajorTics) LineSet(w, x, y1+5, x, y1-5, w->ForegroundColor, XtLINE_SOLID);
    if (w->XAxisNumbers) {
      snprintf(label, sizeof(label), numberformat, val);
      TextSet(w, x, w->y.AxisPos, label, w->ForegroundColor, w->axisFont);
    }
    majorval = val;
  }

  precision = w->y.Precision;
  snprintf(numberformat, sizeof(numberformat), "%%.%df", precision);
  val = w->y.DrawOrigin;
  if (w->YLog && precision > 0) precision--;
  y = PlotY(w, val);
  if (w->DrawMajorTics) LineSet(w, x1+5, y, x1-5, y, w->ForegroundColor, XtLINE_SOLID);
  if (w->YAxisNumbers) {
    snprintf(label, sizeof(label), numberformat, val);
    if (w->YNumHorz) {
      y += FontnumHeight(w, w->axisFont)/2.0 - FontnumDescent(w, w->axisFont);
      TextSet(w, w->x.AxisPos - FontnumTextWidth(w, w->axisFont, label),
              y, label, w->ForegroundColor, w->axisFont);
    } else {
      VTextSet(w, w->x.AxisPos, y, label, w->ForegroundColor, w->axisFont);
    }
  }
  majorval = val;
  while ((majorval * 1.0001) < w->y.DrawMax) {
    if (w->YLog) {
      if (majorval * 1.1 > w->y.DrawMax) break;
      tic = majorval;
      if (w->DrawMinor || w->DrawMinorTics) {
        for (j = 2; j < w->y.MinorNum; j++) {
          val = tic * (real)j;  y = PlotY(w, val);
          if (w->DrawMinor) LineSet(w, x1, y, x2, y, w->ForegroundColor, XtLINE_WIDEDOT);
          if (w->DrawMinorTics) LineSet(w, x1, y, x1+3, y, w->ForegroundColor, XtLINE_SOLID);
        }
      }
      val = tic * (real)w->y.MinorNum;
      snprintf(numberformat, sizeof(numberformat), "%%.%df", precision);
      if (precision > 0) precision--;
    } else {
      tic = majorval;
      if (w->DrawMinor || w->DrawMinorTics) {
        for (j = 1; j < w->y.MinorNum; j++) {
          val = tic + w->y.MajorInc * (real)j / w->y.MinorNum;  y = PlotY(w, val);
          if (w->DrawMinor) LineSet(w, x1, y, x2, y, w->ForegroundColor, XtLINE_WIDEDOT);
          if (w->DrawMinorTics) LineSet(w, x1, y, x1+3, y, w->ForegroundColor, XtLINE_SOLID);
        }
      }
      val = tic + w->y.MajorInc;
    }
    y = PlotY(w, val);
    if (w->DrawMajor) LineSet(w, x1, y, x2, y, w->ForegroundColor, XtLINE_DOTTED);
    else if (w->DrawMinor) LineSet(w, x1, y, x2, y, w->ForegroundColor, XtLINE_WIDEDOT);
    if (w->DrawMajorTics) LineSet(w, x1-5, y, x1+5, y, w->ForegroundColor, XtLINE_SOLID);
    if (w->YAxisNumbers) {
      snprintf(label, sizeof(label), numberformat, val);
      if (w->YNumHorz) {
        y += FontnumHeight(w, w->axisFont)/2.0 - FontnumDescent(w, w->axisFont);
        TextSet(w, w->x.AxisPos - FontnumTextWidth(w, w->axisFont, label),
                y, label, w->ForegroundColor, w->axisFont);
      } else {
        VTextSet(w, w->x.AxisPos, y, label, w->ForegroundColor, w->axisFont);
      }
    }
    majorval = val;
  }

  if (w->ShowTitle)
    TextSet(w, w->x.TitlePos, w->y.TitlePos, w->plotTitle, w->ForegroundColor, w->titleFont);
  if (w->ShowXLabel)
    TextCenter(w, w->x.Origin + (w->x.Size / 2.0), w->y.LabelPos,
               w->xlabel, w->ForegroundColor, w->labelFont);
  if (w->ShowYLabel)
    VTextCenter(w, w->x.LabelPos, w->y.Origin + (w->y.Size / 2.0),
                w->ylabel, w->ForegroundColor, w->labelFont);
}

static void
DrawCartesianPlot(SciPlotWidget *w)
{
  int i, j, jstart;
  SciPlotList *p;

  w->current_id = SciPlotDrawingAny;
  ClipSet(w);
  w->current_id = SciPlotDrawingLine;
  for (i = 0; i < w->num_plotlist; i++) {
    p = w->plotlist + i;
    if (p->draw) {
      real x1, y1, x2, y2;
      gboolean skipnext = FALSE;
      jstart = 0;
      while (jstart < p->number &&
             (p->data[jstart].x <= SCIPLOT_SKIP_VAL || p->data[jstart].y <= SCIPLOT_SKIP_VAL ||
              (w->XLog && p->data[jstart].x <= 0.0) || (w->YLog && p->data[jstart].y <= 0.0)))
        jstart++;
      if (jstart < p->number) {
        x1 = PlotX(w, p->data[jstart].x);
        y1 = PlotY(w, p->data[jstart].y);
      }
      for (j = jstart; j < p->number; j++) {
        if (p->data[j].x <= SCIPLOT_SKIP_VAL || p->data[j].y <= SCIPLOT_SKIP_VAL) {
          skipnext = TRUE; continue;
        }
        if (!((w->XLog && p->data[j].x <= 0.0) || (w->YLog && p->data[j].y <= 0.0))) {
          x2 = PlotX(w, p->data[j].x);  y2 = PlotY(w, p->data[j].y);
          if (!skipnext) LineSet(w, x1, y1, x2, y2, p->LineColor, p->LineStyle);
          x1 = x2;  y1 = y2;
        }
        skipnext = FALSE;
      }
    }
  }
  w->current_id = SciPlotDrawingAny;
  ClipClear(w);
  w->current_id = SciPlotDrawingLine;
  for (i = 0; i < w->num_plotlist; i++) {
    p = w->plotlist + i;
    if (p->draw) {
      real x2, y2;
      for (j = 0; j < p->number; j++) {
        if (!((w->XLog && p->data[j].x <= 0.0) || (w->YLog && p->data[j].y <= 0.0) ||
              p->data[j].x <= SCIPLOT_SKIP_VAL || p->data[j].y <= SCIPLOT_SKIP_VAL)) {
          x2 = PlotX(w, p->data[j].x);  y2 = PlotY(w, p->data[j].y);
          if (x2 >= w->x.Origin && x2 <= w->x.Origin + w->x.Size &&
              y2 >= w->y.Origin && y2 <= w->y.Origin + w->y.Size)
            DrawMarker(w, x2, y2, p->markersize, p->PointColor, p->PointStyle);
        }
      }
    }
  }
}

static void
DrawPolarAxes(SciPlotWidget *w)
{
  real x1, y1, x2, y2, max, tic, val, height;
  int i, j;
  char numberformat[16], label[16];

  w->current_id = SciPlotDrawingAxis;
  snprintf(numberformat, sizeof(numberformat), "%%.%df", w->x.Precision);
  height = FontnumHeight(w, w->labelFont);
  max = w->PolarScale;
  PlotRTDegrees(w, 0.0, 0.0, &x1, &y1);
  PlotRTDegrees(w, max, 0.0, &x2, &y2);
  LineSet(w, x1, y1, x2, y2, 1, XtLINE_SOLID);
  for (i = 45; i < 360; i += 45) {
    PlotRTDegrees(w, max, (real)i, &x2, &y2);
    LineSet(w, x1, y1, x2, y2, w->ForegroundColor, XtLINE_DOTTED);
  }
  for (i = 1; i <= w->x.MajorNum; i++) {
    tic = w->PolarScale * (real)i / (real)w->x.MajorNum;
    if (w->DrawMinor || w->DrawMinorTics) {
      for (j = 1; j < w->x.MinorNum; j++) {
        val = tic - w->x.MajorInc * (real)j / w->x.MinorNum;
        PlotRTDegrees(w, val, 0.0, &x2, &y2);
        if (w->DrawMinor)     CircleSet(w, x1, y1, x2-x1, w->ForegroundColor, XtLINE_WIDEDOT);
        if (w->DrawMinorTics) LineSet(w, x2, y2-2.5, x2, y2+2.5, w->ForegroundColor, XtLINE_SOLID);
      }
    }
    PlotRTDegrees(w, tic, 0.0, &x2, &y2);
    if (w->DrawMajor)    CircleSet(w, x1, y1, x2-x1, w->ForegroundColor, XtLINE_DOTTED);
    if (w->DrawMajorTics) LineSet(w, x2, y2-5.0, x2, y2+5.0, w->ForegroundColor, XtLINE_SOLID);
    if (w->XAxisNumbers) {
      snprintf(label, sizeof(label), numberformat, tic);
      TextSet(w, x2, y2+height, label, w->ForegroundColor, w->axisFont);
    }
  }
  if (w->ShowTitle)
    TextSet(w, w->x.TitlePos, w->y.TitlePos, w->plotTitle, w->ForegroundColor, w->titleFont);
}

static void
DrawPolarPlot(SciPlotWidget *w)
{
  int i, j;
  SciPlotList *p;

  w->current_id = SciPlotDrawingLine;
  for (i = 0; i < w->num_plotlist; i++) {
    p = w->plotlist + i;
    if (p->draw) {
      int jstart;
      real x1, y1, x2, y2;
      gboolean skipnext = FALSE;
      jstart = 0;
      while (jstart < p->number &&
             (p->data[jstart].x <= SCIPLOT_SKIP_VAL || p->data[jstart].y <= SCIPLOT_SKIP_VAL))
        jstart++;
      if (jstart < p->number)
        PlotRT(w, p->data[0].x, p->data[0].y, &x1, &y1);
      for (j = jstart; j < p->number; j++) {
        if (p->data[j].x <= SCIPLOT_SKIP_VAL || p->data[j].y <= SCIPLOT_SKIP_VAL) {
          skipnext = TRUE; continue;
        }
        PlotRT(w, p->data[j].x, p->data[j].y, &x2, &y2);
        if (!skipnext) {
          LineSet(w, x1, y1, x2, y2, p->LineColor, p->LineStyle);
          DrawMarker(w, x1, y1, p->markersize, p->PointColor, p->PointStyle);
          DrawMarker(w, x2, y2, p->markersize, p->PointColor, p->PointStyle);
        }
        x1 = x2;  y1 = y2;
        skipnext = FALSE;
      }
    }
  }
}

static void
DrawAll(SciPlotWidget *w)
{
  if (w->ChartType == XtCARTESIAN) {
    DrawCartesianAxes(w);
    DrawLegend(w);
    DrawCartesianPlot(w);
  } else {
    DrawPolarAxes(w);
    DrawLegend(w);
    DrawPolarPlot(w);
  }
}

static gboolean
DrawQuick(SciPlotWidget *w)
{
  gboolean range_check = CheckMinMax(w);
  EraseAllItems(w);
  DrawAll(w);
  return range_check;
}

/* ==========================================================================
 * Private list management functions
 */

static int
_ListNew(SciPlotWidget *w)
{
  int index;
  SciPlotList *p;
  gboolean found = FALSE;

  for (index = 0; index < w->num_plotlist; index++) {
    p = w->plotlist + index;
    if (!p->used) { found = TRUE; break; }
  }
  if (!found) {
    w->num_plotlist++;
    if (w->alloc_plotlist == 0) {
      w->alloc_plotlist = NUMPLOTLINEALLOC;
      w->plotlist = g_new0(SciPlotList, w->alloc_plotlist);
    } else if (w->num_plotlist > w->alloc_plotlist) {
      w->alloc_plotlist += NUMPLOTLINEALLOC;
      w->plotlist = g_realloc(w->plotlist, w->alloc_plotlist * sizeof(SciPlotList));
    }
    index = w->num_plotlist - 1;
    p = w->plotlist + index;
  }
  p->LineStyle = p->LineColor = p->PointStyle = p->PointColor = 0;
  p->number = p->allocated = 0;
  p->data = NULL;  p->legend = NULL;
  p->draw = p->used = TRUE;
  p->markersize = (real)w->DefaultMarkerSize;
  return index;
}

static void
_ListDelete(SciPlotList *p)
{
  p->draw = p->used = FALSE;
  p->number = p->allocated = 0;
  g_free(p->data);   p->data   = NULL;
  g_free(p->legend); p->legend = NULL;
}

static SciPlotList *
_ListFind(SciPlotWidget *w, int id)
{
  if (id >= 0 && id < w->num_plotlist) {
    SciPlotList *p = w->plotlist + id;
    if (p->used) return p;
  }
  return NULL;
}

static void
_ListSetStyle(SciPlotList *p, int pcolor, int pstyle, int lcolor, int lstyle)
{
  if (lstyle >= 0) p->LineStyle  = lstyle;
  if (lcolor >= 0) p->LineColor  = lcolor;
  if (pstyle >= 0) p->PointStyle = pstyle;
  if (pcolor >= 0) p->PointColor = pcolor;
}

static void
_ListSetLegend(SciPlotList *p, const char *legend)
{
  g_free(p->legend);
  p->legend = g_strdup(legend ? legend : "");
}

static void
_ListAllocData(SciPlotList *p, int num)
{
  g_free(p->data);
  p->allocated = num + NUMPLOTDATAEXTRA;
  p->data = g_new0(realpair, p->allocated);
  if (!p->data) p->number = p->allocated = 0;
}

static void
_ListReallocData(SciPlotList *p, int more)
{
  if (!p->data) {
    _ListAllocData(p, more);
  } else if (p->number + more > p->allocated) {
    p->allocated += more + NUMPLOTDATAEXTRA;
    p->data = g_realloc(p->data, p->allocated * sizeof(realpair));
    if (!p->data) p->number = p->allocated = 0;
  }
}

static void
_ListAddFloat(SciPlotList *p, int num, float *xlist, float *ylist)
{
  _ListReallocData(p, num);
  if (p->data) {
    for (int i = 0; i < num; i++) {
      p->data[i + p->number].x = xlist[i];
      p->data[i + p->number].y = ylist[i];
    }
    p->number += num;
  }
}

static void
_ListAddDouble(SciPlotList *p, int num, double *xlist, double *ylist)
{
  _ListReallocData(p, num);
  if (p->data) {
    for (int i = 0; i < num; i++) {
      p->data[i + p->number].x = (real)xlist[i];
      p->data[i + p->number].y = (real)ylist[i];
    }
    p->number += num;
  }
}

static void
_ListAddReal(SciPlotList *p, int num, real *xlist, real *ylist)
{
  _ListReallocData(p, num);
  if (p->data) {
    for (int i = 0; i < num; i++) {
      p->data[i + p->number].x = xlist[i];
      p->data[i + p->number].y = ylist[i];
    }
    p->number += num;
  }
}

static void _ListSetFloat(SciPlotList *p, int num, float *xlist, float *ylist)
  { if (!p->data || p->allocated < num) _ListAllocData(p, num); p->number = 0; _ListAddFloat(p, num, xlist, ylist); }
static void _ListSetDouble(SciPlotList *p, int num, double *xlist, double *ylist)
  { if (!p->data || p->allocated < num) _ListAllocData(p, num); p->number = 0; _ListAddDouble(p, num, xlist, ylist); }
static void _ListSetReal(SciPlotList *p, int num, real *xlist, real *ylist)
  { if (!p->data || p->allocated < num) _ListAllocData(p, num); p->number = 0; _ListAddReal(p, num, xlist, ylist); }

/* ==========================================================================
 * Property setters / getters
 */

#define CHECK(wi)  do { if (!SCIPLOT_IS_WIDGET(wi)) return;   } while(0)
#define CHECKB(wi) do { if (!SCIPLOT_IS_WIDGET(wi)) return FALSE; } while(0)
#define CHECKW(wi) SCIPLOT_WIDGET(wi)

void sciplot_widget_set_chart_type(GtkWidget *wi, int v)          { CHECK(wi); CHECKW(wi)->ChartType = v; }
void sciplot_widget_set_degrees(GtkWidget *wi, gboolean v)        { CHECK(wi); CHECKW(wi)->Degrees = v; }
void sciplot_widget_set_xlog(GtkWidget *wi, gboolean v)           { CHECK(wi); CHECKW(wi)->XLog = v; }
void sciplot_widget_set_ylog(GtkWidget *wi, gboolean v)           { CHECK(wi); CHECKW(wi)->YLog = v; }
void sciplot_widget_set_xautoscale(GtkWidget *wi, gboolean v)     { CHECK(wi); CHECKW(wi)->XAutoScale = v; }
void sciplot_widget_set_yautoscale(GtkWidget *wi, gboolean v)     { CHECK(wi); CHECKW(wi)->YAutoScale = v; }
void sciplot_widget_set_xaxis_numbers(GtkWidget *wi, gboolean v)  { CHECK(wi); CHECKW(wi)->XAxisNumbers = v; }
void sciplot_widget_set_yaxis_numbers(GtkWidget *wi, gboolean v)  { CHECK(wi); CHECKW(wi)->YAxisNumbers = v; }
void sciplot_widget_set_xorigin(GtkWidget *wi, gboolean v)        { CHECK(wi); CHECKW(wi)->XOrigin = v; }
void sciplot_widget_set_yorigin(GtkWidget *wi, gboolean v)        { CHECK(wi); CHECKW(wi)->YOrigin = v; }
void sciplot_widget_set_draw_major(GtkWidget *wi, gboolean v)     { CHECK(wi); CHECKW(wi)->DrawMajor = v; }
void sciplot_widget_set_draw_minor(GtkWidget *wi, gboolean v)     { CHECK(wi); CHECKW(wi)->DrawMinor = v; }
void sciplot_widget_set_draw_major_tics(GtkWidget *wi, gboolean v){ CHECK(wi); CHECKW(wi)->DrawMajorTics = v; }
void sciplot_widget_set_draw_minor_tics(GtkWidget *wi, gboolean v){ CHECK(wi); CHECKW(wi)->DrawMinorTics = v; }
void sciplot_widget_set_show_legend(GtkWidget *wi, gboolean v)    { CHECK(wi); CHECKW(wi)->ShowLegend = v; }
void sciplot_widget_set_show_title(GtkWidget *wi, gboolean v)     { CHECK(wi); CHECKW(wi)->ShowTitle = v; }
void sciplot_widget_set_show_xlabel(GtkWidget *wi, gboolean v)    { CHECK(wi); CHECKW(wi)->ShowXLabel = v; }
void sciplot_widget_set_show_ylabel(GtkWidget *wi, gboolean v)    { CHECK(wi); CHECKW(wi)->ShowYLabel = v; }
void sciplot_widget_set_monochrome(GtkWidget *wi, gboolean v)     { CHECK(wi); CHECKW(wi)->Monochrome = v; }
void sciplot_widget_set_legend_through_plot(GtkWidget *wi, gboolean v) { CHECK(wi); CHECKW(wi)->LegendThroughPlot = v; }
void sciplot_widget_set_y_numbers_horizontal(GtkWidget *wi, gboolean v){ CHECK(wi); CHECKW(wi)->YNumHorz = v; }
void sciplot_widget_set_margin(GtkWidget *wi, int v)              { CHECK(wi); CHECKW(wi)->Margin = v; }
void sciplot_widget_set_title_margin(GtkWidget *wi, int v)        { CHECK(wi); CHECKW(wi)->TitleMargin = v; }

void sciplot_widget_set_plot_title(GtkWidget *wi, const char *t)
{ CHECK(wi); SciPlotWidget *w = CHECKW(wi); g_free(w->plotTitle); w->plotTitle = g_strdup(t ? t : ""); }
void sciplot_widget_set_xlabel(GtkWidget *wi, const char *t)
{ CHECK(wi); SciPlotWidget *w = CHECKW(wi); g_free(w->xlabel); w->xlabel = g_strdup(t ? t : ""); }
void sciplot_widget_set_ylabel(GtkWidget *wi, const char *t)
{ CHECK(wi); SciPlotWidget *w = CHECKW(wi); g_free(w->ylabel); w->ylabel = g_strdup(t ? t : ""); }
void sciplot_widget_set_title_font(GtkWidget *wi, int f)
{ CHECK(wi); SciPlotWidget *w = CHECKW(wi); w->TitleFont = f; FontnumReplace(w, w->titleFont, f); }
void sciplot_widget_set_label_font(GtkWidget *wi, int f)
{ CHECK(wi); SciPlotWidget *w = CHECKW(wi); w->LabelFont = f; FontnumReplace(w, w->labelFont, f); }
void sciplot_widget_set_axis_font(GtkWidget *wi, int f)
{ CHECK(wi); SciPlotWidget *w = CHECKW(wi); w->AxisFont = f; FontnumReplace(w, w->axisFont, f); }

gboolean    sciplot_widget_get_xlog(GtkWidget *wi)         { return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->XLog         : FALSE; }
gboolean    sciplot_widget_get_ylog(GtkWidget *wi)         { return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->YLog         : FALSE; }
gboolean    sciplot_widget_get_xaxis_numbers(GtkWidget *wi){ return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->XAxisNumbers : FALSE; }
gboolean    sciplot_widget_get_yaxis_numbers(GtkWidget *wi){ return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->YAxisNumbers : FALSE; }
gboolean    sciplot_widget_get_xorigin(GtkWidget *wi)      { return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->XOrigin      : FALSE; }
gboolean    sciplot_widget_get_yorigin(GtkWidget *wi)      { return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->YOrigin      : FALSE; }
gboolean    sciplot_widget_get_draw_major(GtkWidget *wi)   { return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->DrawMajor    : FALSE; }
gboolean    sciplot_widget_get_draw_minor(GtkWidget *wi)   { return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->DrawMinor    : FALSE; }
gboolean    sciplot_widget_get_show_legend(GtkWidget *wi)  { return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->ShowLegend   : FALSE; }
gboolean    sciplot_widget_get_show_title(GtkWidget *wi)   { return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->ShowTitle    : FALSE; }
gboolean    sciplot_widget_get_show_xlabel(GtkWidget *wi)  { return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->ShowXLabel   : FALSE; }
gboolean    sciplot_widget_get_show_ylabel(GtkWidget *wi)  { return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->ShowYLabel   : FALSE; }
const char *sciplot_widget_get_plot_title(GtkWidget *wi)   { return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->plotTitle    : NULL;  }
const char *sciplot_widget_get_xlabel(GtkWidget *wi)       { return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->xlabel       : NULL;  }
const char *sciplot_widget_get_ylabel(GtkWidget *wi)       { return SCIPLOT_IS_WIDGET(wi) ? CHECKW(wi)->ylabel       : NULL;  }

/* ==========================================================================
 * Public SciPlot API
 */

int
SciPlotAllocNamedColor(GtkWidget *wi, const char *name)
{
  GdkRGBA color;
  if (!SCIPLOT_IS_WIDGET(wi)) return -1;
  if (!gdk_rgba_parse(&color, name)) return 1;
  return ColorStore(SCIPLOT_WIDGET(wi), &color);
}

int
SciPlotAllocRGBColor(GtkWidget *wi, int r, int g, int b)
{
  GdkRGBA color;
  if (!SCIPLOT_IS_WIDGET(wi)) return -1;
  color.red   = CLAMP(r / 255.0, 0.0, 1.0);
  color.green = CLAMP(g / 255.0, 0.0, 1.0);
  color.blue  = CLAMP(b / 255.0, 0.0, 1.0);
  color.alpha = 1.0;
  return ColorStore(SCIPLOT_WIDGET(wi), &color);
}

void SciPlotSetBackgroundColor(GtkWidget *wi, int color)
{ if (!SCIPLOT_IS_WIDGET(wi)) return; SciPlotWidget *w = CHECKW(wi); if (color < w->num_colors) w->BackgroundColor = color; }
void SciPlotSetForegroundColor(GtkWidget *wi, int color)
{ if (!SCIPLOT_IS_WIDGET(wi)) return; SciPlotWidget *w = CHECKW(wi); if (color < w->num_colors) w->ForegroundColor = color; }

void SciPlotListDelete(GtkWidget *wi, int idnum)
{ if (!SCIPLOT_IS_WIDGET(wi)) return; SciPlotList *p = _ListFind(SCIPLOT_WIDGET(wi), idnum); if (p) _ListDelete(p); }

int
SciPlotListCreateFromData(GtkWidget *wi, int num, real *xlist, real *ylist,
                          const char *legend,
                          int pcolor, int pstyle, int lcolor, int lstyle)
{
  if (!SCIPLOT_IS_WIDGET(wi)) return -1;
  SciPlotWidget *w = CHECKW(wi);
  int idnum = _ListNew(w);
  SciPlotList *p = w->plotlist + idnum;
  _ListSetReal(p, num, xlist, ylist);
  _ListSetLegend(p, legend);
  _ListSetStyle(p, pcolor, pstyle, lcolor, lstyle);
  return idnum;
}

int
SciPlotListCreateFloat(GtkWidget *wi, int num, float *xlist, float *ylist, const char *legend)
{
  if (!SCIPLOT_IS_WIDGET(wi)) return -1;
  SciPlotWidget *w = CHECKW(wi);
  int idnum = _ListNew(w);
  SciPlotList *p = w->plotlist + idnum;
  _ListSetFloat(p, num, xlist, ylist);
  _ListSetLegend(p, legend);
  _ListSetStyle(p, 1, XtMARKER_CIRCLE, 1, XtLINE_SOLID);
  return idnum;
}

void SciPlotListUpdateFloat(GtkWidget *wi, int idnum, int num, float *xlist, float *ylist)
{ if (!SCIPLOT_IS_WIDGET(wi)) return; SciPlotList *p = _ListFind(CHECKW(wi), idnum); if (p) _ListSetFloat(p, num, xlist, ylist); }
void SciPlotListAddFloat(GtkWidget *wi, int idnum, int num, float *xlist, float *ylist)
{ if (!SCIPLOT_IS_WIDGET(wi)) return; SciPlotList *p = _ListFind(CHECKW(wi), idnum); if (p) _ListAddFloat(p, num, xlist, ylist); }

int
SciPlotListCreateDouble(GtkWidget *wi, int num, double *xlist, double *ylist, const char *legend)
{
  if (!SCIPLOT_IS_WIDGET(wi)) return -1;
  SciPlotWidget *w = CHECKW(wi);
  int idnum = _ListNew(w);
  SciPlotList *p = w->plotlist + idnum;
  _ListSetDouble(p, num, xlist, ylist);
  _ListSetLegend(p, legend);
  _ListSetStyle(p, 1, XtMARKER_CIRCLE, 1, XtLINE_SOLID);
  return idnum;
}

void SciPlotListUpdateDouble(GtkWidget *wi, int idnum, int num, double *xlist, double *ylist)
{ if (!SCIPLOT_IS_WIDGET(wi)) return; SciPlotList *p = _ListFind(CHECKW(wi), idnum); if (p) _ListSetDouble(p, num, xlist, ylist); }
void SciPlotListAddDouble(GtkWidget *wi, int idnum, int num, double *xlist, double *ylist)
{ if (!SCIPLOT_IS_WIDGET(wi)) return; SciPlotList *p = _ListFind(CHECKW(wi), idnum); if (p) _ListAddDouble(p, num, xlist, ylist); }

void SciPlotListSetStyle(GtkWidget *wi, int idnum, int pcolor, int pstyle, int lcolor, int lstyle)
{ if (!SCIPLOT_IS_WIDGET(wi)) return; SciPlotList *p = _ListFind(CHECKW(wi), idnum); if (p) _ListSetStyle(p, pcolor, pstyle, lcolor, lstyle); }
void SciPlotListSetMarkerSize(GtkWidget *wi, int idnum, float size)
{ if (!SCIPLOT_IS_WIDGET(wi)) return; SciPlotList *p = _ListFind(CHECKW(wi), idnum); if (p) p->markersize = (real)size; }

void SciPlotSetXAutoScale(GtkWidget *wi)
{ if (!SCIPLOT_IS_WIDGET(wi)) return; CHECKW(wi)->XAutoScale = TRUE; }
void SciPlotSetXUserScale(GtkWidget *wi, double min, double max)
{ if (!SCIPLOT_IS_WIDGET(wi)) return; SciPlotWidget *w = CHECKW(wi); if (min < max) { w->XAutoScale = FALSE; w->UserMin.x = (real)min; w->UserMax.x = (real)max; } }
void SciPlotSetYAutoScale(GtkWidget *wi)
{ if (!SCIPLOT_IS_WIDGET(wi)) return; CHECKW(wi)->YAutoScale = TRUE; }
void SciPlotSetYUserScale(GtkWidget *wi, double min, double max)
{ if (!SCIPLOT_IS_WIDGET(wi)) return; SciPlotWidget *w = CHECKW(wi); if (min < max) { w->YAutoScale = FALSE; w->UserMin.y = (real)min; w->UserMax.y = (real)max; } }

void
SciPlotPrintStatistics(GtkWidget *wi)
{
  if (!SCIPLOT_IS_WIDGET(wi)) return;
  SciPlotWidget *w = CHECKW(wi);
  int i, j;
  SciPlotList *p;
  printf("Title=%s\nxlabel=%s\tylabel=%s\n", w->plotTitle, w->xlabel, w->ylabel);
  printf("ChartType=%d\nDegrees=%d\nXLog=%d\tYLog=%d\nXAutoScale=%d\tYAutoScale=%d\n",
         w->ChartType, w->Degrees, w->XLog, w->YLog, w->XAutoScale, w->YAutoScale);
  for (i = 0; i < w->num_plotlist; i++) {
    p = w->plotlist + i;
    if (p->draw) {
      printf("\nLegend=%s\nStyles: point=%d line=%d  Color: point=%d line=%d\n",
             p->legend, p->PointStyle, p->LineStyle, p->PointColor, p->LineColor);
      for (j = 0; j < p->number; j++)
        printf("%f\t%f\n", (double)p->data[j].x, (double)p->data[j].y);
      printf("\n");
    }
  }
}

void
SciPlotExportData(GtkWidget *wi, FILE *fd)
{
  if (!SCIPLOT_IS_WIDGET(wi)) return;
  SciPlotWidget *w = CHECKW(wi);
  int i, j;
  SciPlotList *p;
  fprintf(fd, "Title=\"%s\"\nXaxis=\"%s\"\nYaxis=\"%s\"\n\n", w->plotTitle, w->xlabel, w->ylabel);
  for (i = 0; i < w->num_plotlist; i++) {
    p = w->plotlist + i;
    if (p->draw) {
      fprintf(fd, "Line=\"%s\"\n", p->legend);
      for (j = 0; j < p->number; j++)
        fprintf(fd, "%e\t%e\n", (double)p->data[j].x, (double)p->data[j].y);
      fprintf(fd, "\n");
    }
  }
}

void
SciPlotUpdate(GtkWidget *wi)
{
  if (!SCIPLOT_IS_WIDGET(wi)) return;
  if (!gtk_widget_get_realized(wi)) return;
  SciPlotWidget *w = CHECKW(wi);
  EraseAll(w);
  ComputeAll(w);
  DrawAll(w);
  gtk_widget_queue_draw(wi);
}

gboolean
SciPlotQuickUpdate(GtkWidget *wi)
{
  if (!SCIPLOT_IS_WIDGET(wi)) return FALSE;
  if (!gtk_widget_get_realized(wi)) return FALSE;
  SciPlotWidget *w = CHECKW(wi);
  gboolean range_check = DrawQuick(w);
  gtk_widget_queue_draw(wi);
  return range_check;
}
