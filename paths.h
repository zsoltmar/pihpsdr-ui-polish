#ifndef PIHPSDR_PATHS_H
#define PIHPSDR_PATHS_H

#include <glib.h>

/* Locate bundled data (e.g. hpsdr.png) from CWD, executable dir, or system paths. */
gchar *pihpsdr_resolve_data_file(const gchar *basename);

#endif
