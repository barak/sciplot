#ifndef _SCIPLOTUTIL_H
#define _SCIPLOTUTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gtk/gtk.h>
#include <stdio.h>
#include "SciPlot.h"

GtkWidget *SciPlotDialog(GtkWidget *parent, const char *title);
void       SciPlotDialogPopup(GtkWidget *plot);
void       SciPlotReadDataFile(GtkWidget *parent, FILE *fd);

#ifdef __cplusplus
}
#endif

#endif /* _SCIPLOTUTIL_H */
