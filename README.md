# SciPlot Widget

A GTK4/Cairo widget for plotting 2D scientific data in cartesian or polar
graphs.

**Version 1.37** — GTK4/Cairo/Pango port by Barak A. Pearlmutter
**Original** — Robert W. McMullen `<rwmcm@orion.ae.utexas.edu>`, 1994–1996


## Overview

SciPlot is a `GtkWidget` subclass that plots cartesian or polar graphs,
including logarithmic axes for cartesian plots.  Drawing uses Cairo; text
layout and font metrics use Pango.

Features:

- Cartesian and polar plots
- Automatic or user-defined axis scales
- Logarithmic axes (cartesian)
- Degrees or radians for polar angles
- Axis labels and title
- Legend
- Multiple data series per plot
- Broken (skip-value) line segments
- Markers: circles, squares, triangles, diamonds, hourglasses, bowties (filled and unfilled variants)
- Line styles: solid, dotted, wide-dot
- Full colour support with named and RGB colour allocation
- User-selectable fonts (Times, Courier, Helvetica, Lucida, LucidaSans, New Century Schoolbook; size and bold/italic attributes)
- PostScript (EPS) output via Cairo PS surface
- Real-time streaming updates via `SciPlotQuickUpdate` / `SciPlotUpdate`
- Horizontal or vertical Y-axis tick labels


## Requirements

- GTK4 ≥ 4.0
- Cairo (with PostScript surface support)
- Pango / pangocairo
- `pkg-config`
- A C99 compiler


## Building

```sh
autoreconf -fi
./configure
make
sudo make install
```

The build system uses autoconf/automake and finds GTK4, Cairo, and Pango via
`pkg-config`.


## Demo Programs

Two demo programs are included:

**`sciplot-xyplot`** — reads x/y data from a text file (or stdin) and
displays an interactive plot window with controls for log axes, labels,
legend, and PostScript export.

```sh
sciplot-xyplot data.txt
sciplot-xyplot --help
```

**`sciplot-realtime`** — reads whitespace-separated numeric data from
standard input and updates the plot as each line arrives.  Column structure
is inferred from the first data line:

| Columns | Interpretation |
|---------|----------------|
| 1: *y* | single Y series; X auto-increments from 0 |
| 2: *x y* | explicit X and one Y series |
| N: *x y1 y2 …* | explicit X and N−1 Y series |

Lines starting with `#` are ignored.  When stdin closes the window stays
open for interactive use.

```sh
sensor-tool | sciplot-realtime --ylabel "Temperature (C)"
multi-sensor | sciplot-realtime --legend "ch1,ch2,ch3"
sciplot-realtime < data.txt
sciplot-realtime --help
yes | awk '{print NR, sqrt(NR)*sin(NR/10.0)}' | pv --rate-limit=1000 --quiet | sciplot-realtime
```

The text file format accepted by `sciplot-xyplot` is documented in
[`SciPlotDemo.html`](SciPlotDemo.html).


## Using the Widget

See [`SciPlotProg.html`](SciPlotProg.html) for the full programmer's
reference.  A minimal example:

```c
#include "SciPlot.h"

GtkWidget *plot = sciplot_widget_new();
gtk_widget_set_size_request(plot, 500, 400);

/* Allocate colours (index 0 = background white, 1 = foreground black
   are pre-allocated; user colours start at 2) */
int red  = SciPlotAllocNamedColor(plot, "red");
int blue = SciPlotAllocNamedColor(plot, "blue");

float x[] = {1, 2, 3, 4, 5};
float y[] = {1, 4, 9, 16, 25};
int id = SciPlotListCreateFloat(plot, 5, x, y, "y = x^2");
SciPlotListSetStyle(plot, id, red, XtMARKER_CIRCLE, blue, XtLINE_SOLID);

sciplot_widget_set_xlabel(plot, "x");
sciplot_widget_set_ylabel(plot, "y");
sciplot_widget_set_plot_title(plot, "Demo");

/* Call after the widget is realized and sized */
SciPlotUpdate(plot);
```

### Key API changes from the X11/Xt version

The GTK4 port is **not** ABI- or source-compatible with the original Xt
widget (soname bumped from `.so.1` to `.so.2`).  The main differences:

| Old (Xt)                          | New (GTK4)                              |
|-----------------------------------|-----------------------------------------|
| `XtVaCreateManagedWidget(..., sciplotWidgetClass, ...)` | `sciplot_widget_new()` |
| `XtVaSetValues(w, XtNplotTitle, "foo", NULL)` | `sciplot_widget_set_plot_title(w, "foo")` |
| `XtVaGetValues(w, XtNxLog, &v, NULL)` | `v = sciplot_widget_get_xlog(w)` |
| `Boolean` / `True` / `False`      | `gboolean` / `TRUE` / `FALSE`           |
| `XtIsSciPlot(w)`                  | `SCIPLOT_IS_WIDGET(w)`                  |
| `#include <X11/...>`              | `#include <gtk/gtk.h>`                  |

`Widget` is now a `typedef` for `GtkWidget *` for light compatibility, and
the `SciPlot*` function names are unchanged.


## Bug Reports and Contributions

Please report issues and submit patches via the
[repository](https://github.com/barak/sciplot).


## Authors

- Robert W. McMullen `<rwmcm@orion.ae.utexas.edu>` — original X11/Xt widget, 1994–1996
- Barak A. Pearlmutter `<bap@debian.org>` — Debian packaging and GTK4/Cairo port, 2003–2026


## Copyright

Copyright © 1996 Robert W. McMullen
Copyright © 2026 Barak A. Pearlmutter (GTK4/Cairo port)

This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

This library is distributed in the hope that it will be useful, but
**without any warranty**; without even the implied warranty of
merchantability or fitness for a particular purpose.  See the GNU Library
General Public License for more details.
