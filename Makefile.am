ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libsciplot.la
libsciplot_la_SOURCES = SciPlot.c SciPlotUtil.c
libsciplot_la_CFLAGS = $(GTK4_CFLAGS)
libsciplot_la_LIBADD = $(GTK4_LIBS)
libsciplot_la_LDFLAGS = -version-info 2:0:0

bin_PROGRAMS = realtime xyplot

realtime_SOURCES = realtime.c
xyplot_SOURCES   = xyplot.c
realtime_CFLAGS  = $(GTK4_CFLAGS)
xyplot_CFLAGS    = $(GTK4_CFLAGS)
realtime_LDADD   = libsciplot.la $(GTK4_LIBS)
xyplot_LDADD     = libsciplot.la $(GTK4_LIBS)

include_HEADERS = SciPlot.h SciPlotP.h SciPlotUtil.h

dist_doc_DATA  = data.txt
dist_doc_DATA += ANNOUNCE CHANGES LICENSE README.md
dist_doc_DATA += SciPlot.html SciPlotDemo.html SciPlotProg.html gpl.html
dist_doc_DATA += SciPlotDemo.gif SciPlotExample.gif SciPlotTitle.gif new.gif
