ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libsciplot.la
libsciplot_la_SOURCES = SciPlot.c SciPlotUtil.c
libsciplot_la_CFLAGS = $(GTK4_CFLAGS)
libsciplot_la_LIBADD = $(GTK4_LIBS)
libsciplot_la_LDFLAGS = -version-info 2:0:0

bin_PROGRAMS = sciplot-xyplot sciplot-realtime

sciplot_xyplot_SOURCES  = xyplot.c
sciplot_xyplot_CFLAGS   = $(GTK4_CFLAGS)
sciplot_xyplot_LDADD    = libsciplot.la $(GTK4_LIBS)

sciplot_realtime_SOURCES = realtime.c
sciplot_realtime_CFLAGS  = $(GTK4_CFLAGS)
sciplot_realtime_LDADD   = libsciplot.la $(GTK4_LIBS)

include_HEADERS = SciPlot.h SciPlotP.h SciPlotUtil.h

# Man pages via help2man
if HAVE_HELP2MAN
man_MANS = sciplot-xyplot.1 sciplot-realtime.1
CLEANFILES = $(man_MANS)

sciplot-xyplot.1: sciplot-xyplot$(EXEEXT) sciplot-xyplot.h2m
	$(HELP2MAN) --no-info \
	  --name="plot x/y or polar data files" \
	  --include=$(srcdir)/sciplot-xyplot.h2m \
	  ./$< > $@

sciplot-realtime.1: sciplot-realtime$(EXEEXT) sciplot-realtime.h2m
	$(HELP2MAN) --no-info \
	  --name="real-time SciPlot demonstration" \
	  --include=$(srcdir)/sciplot-realtime.h2m \
	  ./$< > $@
endif

dist_doc_DATA  = data.txt
dist_doc_DATA += ANNOUNCE CHANGES LICENSE README.md
dist_doc_DATA += SciPlot.html SciPlotDemo.html SciPlotProg.html gpl.html
dist_doc_DATA += SciPlotDemo.gif SciPlotExample.gif SciPlotTitle.gif new.gif

EXTRA_DIST = sciplot-xyplot.h2m sciplot-realtime.h2m
