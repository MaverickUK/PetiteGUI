/* The A: and C: windows: a directory read with DOS findfirst/findnext. */
#ifndef FILES_H
#define FILES_H

#include "app.h"

/* Read w->path into w->rows: folders first, ".." (in a subdirectory) at the top, then by
 * name, size (biggest first) or date (newest first), a SORT_ value. Sets w->free_kb and
 * w->not_ready (the drive couldn't be read: no disk, say); nothing is picked. */
void files_load(Win *w, int by);
void files_sort(Win *w, int by);

#endif
