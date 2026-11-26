/* The A: and C: windows: a directory read with DOS findfirst/findnext. */
#ifndef FILES_H
#define FILES_H

#include "app.h"

/* Read w->path into w->rows: folders first, ".." (in a subdirectory) at the top. Sets
 * w->free_kb and w->not_ready (the drive couldn't be read: no disk, say). */
void files_load(Win *w, int by_size);
void files_sort(Win *w, int by_size);

#endif
